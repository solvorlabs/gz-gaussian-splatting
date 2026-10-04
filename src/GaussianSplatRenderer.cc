// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Solvor Labs
#include "GaussianSplatRenderer.hh"
#include "gz_gaussian_splatting/Cache.hh"
#include <cstring>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <gz/common/Console.hh>
#include <gz/gui/Application.hh>
#include <gz/gui/GuiEvents.hh>
#include <gz/gui/MainWindow.hh>
#include <gz/math/Angle.hh>
#include <gz/math/Matrix4.hh>
#include <gz/math/Pose3.hh>
#include <gz/msgs/pose_v.pb.h>
#include <gz/plugin/Register.hh>
#include <gz/rendering/RenderEngine.hh>
#include <gz/rendering/RenderingIface.hh>
#include <gz/rendering/Camera.hh>
#include <gz/rendering/Image.hh>
#include <gz/rendering/Scene.hh>
#include <gz/rendering/ogre2/Ogre2Node.hh>
#include <gz/rendering/ogre2/Ogre2Scene.hh>
#include <gz/transport/Node.hh>

#include <OgreColourValue.h>
#include <OgreGpuProgramParams.h>
#include <OgreHighLevelGpuProgram.h>
#include <OgreHighLevelGpuProgramManager.h>
#include <OgreHlmsDatablock.h>
#include <OgreManualObject2.h>
#include <OgreMaterial.h>
#include <OgreMaterialManager.h>
#include <OgrePass.h>
#include <OgreResourceGroupManager.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreTechnique.h>
#include <OgreVector3.h>
#include <OgreVector4.h>
#include <Vao/OgreIndexBufferPacked.h>
#include <QImage>

namespace gz_gaussian_splatting
{
namespace
{
// Ogre persists compiled microcode between Gazebo sessions. Version these
// resource names whenever the shader ABI changes so an old linked program can
// never be paired with the current ManualObject vertex layout.
constexpr char kVertexProgramName[] = "GazeboGaussianVS_covariance_sh3_v11";
constexpr char kGeometryProgramName[] = "GazeboGaussianGS_covariance_sh3_v11";
constexpr char kFragmentProgramName[] = "GazeboGaussianFS_covariance_sh3_v11";
constexpr char kMaterialName[] = "GazeboGaussianMaterial_covariance_sh3_v11";

constexpr char kVertexShader[] = R"glsl(
#version 330 core

layout(location = 0) in vec4 vertex;
layout(location = 7) in vec4 uv0;
layout(location = 8) in vec4 uv1;
layout(location = 9) in vec4 uv2;
layout(location = 10) in vec4 uv3;
layout(location = 11) in vec4 uv4;
layout(location = 12) in vec4 uv5;
layout(location = 13) in vec4 uv6;
layout(location = 14) in vec4 uv7;

uniform vec3 cameraPosition;

vec3 srgbToLinear(vec3 value)
{
  vec3 low = value / 12.92;
  vec3 high = pow((value + 0.055) / 1.055, vec3(2.4));
  return mix(low, high, step(vec3(0.04045), value));
}

out block
{
  vec3 scale;
  vec4 rotation;
  vec4 colour;
} outVs;

out gl_PerVertex
{
  vec4 gl_Position;
};

void main()
{
  // Two 12-bit signed SH values are packed into each exactly representable
  // 24-bit float integer. This keeps degree-3 RGB SH plus covariance inside
  // Ogre's eight available texture-coordinate attributes.
  vec4 packedValues[6] = vec4[6](uv0, uv1, uv2, uv3, uv4, uv5);
  float sh[48];
  for (int pairIndex = 0; pairIndex < 24; ++pairIndex)
  {
    uint packedCode = uint(
      packedValues[pairIndex / 4][pairIndex % 4] + 0.5);
    sh[pairIndex * 2] =
      (float(packedCode & 4095u) / 4095.0) * 16.0 - 8.0;
    sh[pairIndex * 2 + 1] =
      (float((packedCode >> 12u) & 4095u) / 4095.0) * 16.0 - 8.0;
  }

  vec3 direction = normalize(vertex.xyz - cameraPosition);
  float x = direction.x;
  float y = direction.y;
  float z = direction.z;
  float xx = x * x;
  float yy = y * y;
  float zz = z * z;
  float basis[16];
  basis[0] = 0.28209479177387814;
  basis[1] = -0.4886025119029199 * y;
  basis[2] =  0.4886025119029199 * z;
  basis[3] = -0.4886025119029199 * x;
  basis[4] =  1.0925484305920792 * x * y;
  basis[5] = -1.0925484305920792 * y * z;
  basis[6] =  0.31539156525252005 * (2.0 * zz - xx - yy);
  basis[7] = -1.0925484305920792 * x * z;
  basis[8] =  0.5462742152960396 * (xx - yy);
  basis[9] = -0.5900435899266435 * y * (3.0 * xx - yy);
  basis[10] = 2.890611442640554 * x * y * z;
  basis[11] = -0.4570457994644658 * y * (4.0 * zz - xx - yy);
  basis[12] = 0.3731763325901154 * z * (2.0 * zz - 3.0 * xx - 3.0 * yy);
  basis[13] = -0.4570457994644658 * x * (4.0 * zz - xx - yy);
  basis[14] = 1.445305721320277 * z * (xx - yy);
  basis[15] = -0.5900435899266435 * x * (xx - 3.0 * yy);
  vec3 colour = vec3(0.5);
  for (int coefficient = 0; coefficient < 16; ++coefficient)
  {
    colour += basis[coefficient] * vec3(
      sh[coefficient * 3],
      sh[coefficient * 3 + 1],
      sh[coefficient * 3 + 2]);
  }

  // World-space Gaussian centre is intentionally passed to the geometry
  // shader in gl_Position; projection happens there after covariance work.
  gl_Position = vertex;
  outVs.scale = uv6.xyz;
  vec3 quaternionXyz = uv7.xyz;
  outVs.rotation = vec4(
    quaternionXyz,
    sqrt(max(0.0, 1.0 - dot(quaternionXyz, quaternionXyz))));
  // Nerfstudio trains and exports display-encoded RGB. Ogre's render target
  // performs the final linear-to-sRGB conversion, so feed it linear values
  // to avoid applying the display gamma twice.
  outVs.colour = vec4(srgbToLinear(clamp(colour, 0.0, 1.0)), uv6.w);
}
)glsl";

constexpr char kGeometryShader[] = R"glsl(
#version 330 core

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

// Explicitly match the vertex stage's built-in interface. An implicit
// geometry input contains gl_PointSize / gl_ClipDistance as well, which
// strict Mesa linkers reject against our position-only vertex output.
in gl_PerVertex
{
  vec4 gl_Position;
} gl_in[];

in block
{
  vec3 scale;
  vec4 rotation;
  vec4 colour;
} inGs[];

uniform mat4 worldView;
uniform mat4 projection;
uniform float radiusSigma;
uniform vec4 viewportSize;

out block
{
  vec2 gaussianCoord;
  vec4 colour;
} outGs;

out gl_PerVertex
{
  vec4 gl_Position;
};

mat3 quaternionMatrix(vec4 value)
{
  vec4 q = normalize(value);
  float x = q.x;
  float y = q.y;
  float z = q.z;
  float w = q.w;
  return mat3(
    1.0 - 2.0 * (y * y + z * z),
    2.0 * (x * y + z * w),
    2.0 * (x * z - y * w),
    2.0 * (x * y - z * w),
    1.0 - 2.0 * (x * x + z * z),
    2.0 * (y * z + x * w),
    2.0 * (x * z + y * w),
    2.0 * (y * z - x * w),
    1.0 - 2.0 * (x * x + y * y));
}

void emitCorner(vec2 corner, vec2 majorAxis, vec2 minorAxis, vec4 centreClip)
{
  vec2 offset = radiusSigma *
    (corner.x * majorAxis + corner.y * minorAxis);
  gl_Position = centreClip;
  gl_Position.xy += offset * centreClip.w;
  outGs.gaussianCoord = radiusSigma * corner;
  outGs.colour = inGs[0].colour;
  EmitVertex();
}

void main()
{
  vec4 centreCamera = worldView * gl_in[0].gl_Position;
  vec4 centreClip = projection * centreCamera;
  // Match the reference 3DGS rasterizer's near-plane rejection. Gaussians
  // whose centres are effectively on the camera plane otherwise acquire an
  // unbounded projected covariance and can cover the entire viewport.
  if (centreClip.w <= 0.01)
    return;

  vec3 scale = max(inGs[0].scale, vec3(1e-7));
  mat3 rotation = quaternionMatrix(inGs[0].rotation);
  mat3 covarianceWorld = rotation * mat3(
    scale.x * scale.x, 0.0, 0.0,
    0.0, scale.y * scale.y, 0.0,
    0.0, 0.0, scale.z * scale.z) * transpose(rotation);
  mat3 viewRotation = mat3(worldView);
  mat3 covarianceCamera =
    viewRotation * covarianceWorld * transpose(viewRotation);

  // Quotient-rule Jacobian for NDC projection. This uses Ogre's actual
  // projection matrix, so it remains correct for its clip-space convention.
  // gsplat clamps the covariance Jacobian (but not the projected mean) to a
  // viewport expanded by 15% on each edge. Without this, an off-screen mean
  // can acquire an enormous derivative and wash the image with one quad.
  vec4 covarianceClip = centreClip;
  covarianceClip.x = clamp(
    covarianceClip.x, -1.3 * covarianceClip.w, 1.3 * covarianceClip.w);
  covarianceClip.y = clamp(
    covarianceClip.y, -1.3 * covarianceClip.w, 1.3 * covarianceClip.w);
  float inverseW2 = 1.0 / (centreClip.w * centreClip.w);
  vec3 jacobianX = vec3(
    projection[0][0] * centreClip.w - covarianceClip.x * projection[0][3],
    projection[1][0] * centreClip.w - covarianceClip.x * projection[1][3],
    projection[2][0] * centreClip.w - covarianceClip.x * projection[2][3]) * inverseW2;
  vec3 jacobianY = vec3(
    projection[0][1] * centreClip.w - covarianceClip.y * projection[0][3],
    projection[1][1] * centreClip.w - covarianceClip.y * projection[1][3],
    projection[2][1] * centreClip.w - covarianceClip.y * projection[2][3]) * inverseW2;

  vec3 covarianceTimesX = covarianceCamera * jacobianX;
  vec3 covarianceTimesY = covarianceCamera * jacobianY;
  float a = max(dot(jacobianX, covarianceTimesX), 1e-10);
  float b = dot(jacobianX, covarianceTimesY);
  float d = max(dot(jacobianY, covarianceTimesY), 1e-10);
  // gsplat classic mode adds 0.3 pixel^2 before inversion. Convert that
  // variance into NDC using Ogre's active viewport dimensions.
  a += 1.2 * viewportSize.z * viewportSize.z;
  d += 1.2 * viewportSize.w * viewportSize.w;
  float midpoint = 0.5 * (a + d);
  float spread = sqrt(max(0.25 * (a - d) * (a - d) + b * b, 0.0));
  float lambdaMajor = max(midpoint + spread, 1e-10);
  float lambdaMinor = max(midpoint - spread, 1e-10);
  vec2 direction = abs(b) > 1e-10
    ? normalize(vec2(b, lambdaMajor - a))
    : (a >= d ? vec2(1.0, 0.0) : vec2(0.0, 1.0));
  vec2 majorAxis = direction * sqrt(lambdaMajor);
  vec2 minorAxis = vec2(-direction.y, direction.x) * sqrt(lambdaMinor);

  emitCorner(vec2(-1.0, -1.0), majorAxis, minorAxis, centreClip);
  emitCorner(vec2( 1.0, -1.0), majorAxis, minorAxis, centreClip);
  emitCorner(vec2(-1.0,  1.0), majorAxis, minorAxis, centreClip);
  emitCorner(vec2( 1.0,  1.0), majorAxis, minorAxis, centreClip);
  EndPrimitive();
}
)glsl";

constexpr char kFragmentShader[] = R"glsl(
#version 330 core

in block
{
  vec2 gaussianCoord;
  vec4 colour;
} inPs;

layout(location = 0) out vec4 fragColor;

void main()
{
  float exponent = -0.5 * dot(inPs.gaussianCoord, inPs.gaussianCoord);
  float alpha = min(0.99, inPs.colour.a * exp(exponent));
  if (alpha < (1.0 / 255.0))
    discard;
  fragColor = vec4(inPs.colour.rgb, alpha);
}
)glsl";

Ogre::HighLevelGpuProgramPtr CreateProgram(
  const Ogre::String &name,
  const Ogre::String &source,
  Ogre::GpuProgramType type)
{
  auto &manager = Ogre::HighLevelGpuProgramManager::getSingleton();
  auto existing = manager.getByName(name);
  if (existing)
    return existing;
  auto program = manager.createProgram(
    name,
    Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
    "glsl",
    type);
  program->setSource(source);
  program->load();
  if (program->hasCompileError() || !program->isSupported())
    throw std::runtime_error("Gaussian shader failed to compile: " + name);
  return program;
}

}  // namespace

class GaussianSplatRendererPrivate
{
  public: ~GaussianSplatRendererPrivate()
  {
    // Stop incoming callbacks before the mutex and staged pose are destroyed.
    if (!this->poseTopic.empty())
      this->transportNode.Unsubscribe(this->poseTopic);
  }
  public: bool Initialize();
  public: void FindCamera();
  public: void OnPoses(const gz::msgs::Pose_V &_message);
  public: void MaybeFollowCapturedView();
  public: void MaybeSort();
  public: void ConfigureCapturedCamera();
  public: void MaybeCaptureRenderedFrame();
  public: void SortForCamera(const gz::rendering::CameraPtr &_camera);
  public: std::string cachePath;
  public: double radiusSigma{3.0};
  public: std::vector<GaussianRecord> records;
  public: bool initialized{false};
  public: bool failed{false};
  public: bool capturedView{false};
  public: bool capturedViewLogged{false};
  public: std::string poseTopic;
  public: std::string cameraTarget;
  public: double horizontalFov{0.0};
  public: std::string capturePath;
  public: unsigned int captureWidth{0};
  public: unsigned int captureHeight{0};
  public: double captureFx{0.0};
  public: double captureFy{0.0};
  public: double captureCx{0.0};
  public: double captureCy{0.0};
  public: bool captureConfigured{false};
  public: bool captureCompleted{false};
  public: unsigned int captureFrames{0};
  public: gz::transport::Node transportNode;
  public: std::mutex targetPoseMutex;
  public: std::optional<gz::math::Pose3d> targetPose;
  public: std::optional<gz::math::Pose3d> lastAppliedTargetPose;
  public: gz::rendering::ScenePtr scene;
  public: Ogre::MaterialPtr material;
  public: Ogre::ManualObject *splats{nullptr};
  public: Ogre::IndexBufferPacked *indexBuffer{nullptr};
  public: gz::rendering::CameraPtr camera;
  public: gz::math::Pose3d lastSortedPose;
  public: bool hasSortedPose{false};
  public: std::chrono::steady_clock::time_point lastSortTime{};
  public: std::vector<float> depths;
  public: std::vector<std::uint32_t> sortedIndices;
};

void GaussianSplatRendererPrivate::FindCamera()
{
  if (this->camera || !this->scene)
    return;
  for (unsigned int index = 0; index < this->scene->SensorCount(); ++index)
  {
    this->camera = std::dynamic_pointer_cast<gz::rendering::Camera>(
      this->scene->SensorByIndex(index));
    if (this->camera)
      break;
  }
  if (this->camera && this->capturedView)
    this->ConfigureCapturedCamera();
}

void GaussianSplatRendererPrivate::ConfigureCapturedCamera()
{
  if (!this->camera || !this->capturedView)
    return;
  if (this->horizontalFov > 0.0)
    this->camera->SetHFOV(gz::math::Angle(this->horizontalFov));
  if (this->captureWidth == 0 || this->captureConfigured)
    return;

  this->camera->SetImageWidth(this->captureWidth);
  this->camera->SetImageHeight(this->captureHeight);
  this->camera->SetImageFormat(gz::rendering::PF_R8G8B8);
  this->camera->SetAspectRatio(
    static_cast<double>(this->captureWidth) / this->captureHeight);
  this->camera->SetHFOV(gz::math::Angle(
    2.0 * std::atan(static_cast<double>(this->captureWidth) /
      (2.0 * this->captureFx))));

  // Start with Gazebo / Ogre's native depth mapping and replace only the
  // calibrated pinhole terms, preserving the renderer's depth convention.
  auto projection = this->camera->ProjectionMatrix();
  projection(0, 0) = 2.0 * this->captureFx / this->captureWidth;
  projection(1, 1) = 2.0 * this->captureFy / this->captureHeight;
  projection(0, 2) = 1.0 - 2.0 * this->captureCx / this->captureWidth;
  projection(1, 2) = 2.0 * this->captureCy / this->captureHeight - 1.0;
  this->camera->SetProjectionMatrix(projection);
  this->captureConfigured = true;
  gzmsg << "Gaussian calibrated camera configured: "
        << this->captureWidth << "x" << this->captureHeight
        << " fx=" << this->captureFx << " fy=" << this->captureFy
        << " cx=" << this->captureCx << " cy=" << this->captureCy
        << std::endl;
}

void GaussianSplatRendererPrivate::MaybeCaptureRenderedFrame()
{
  if (this->capturePath.empty() || this->captureCompleted ||
      !this->camera || !this->hasSortedPose)
  {
    return;
  }

  if (++this->captureFrames < 10)
    return;
  auto image = this->camera->CreateImage();
  this->camera->Copy(image);
  if (image.Format() != gz::rendering::PF_R8G8B8)
    throw std::runtime_error("Screenshot requires an RGB8 GUI camera");
  QImage pixels(image.Data<unsigned char>(), image.Width(), image.Height(),
    image.Width() * 3, QImage::Format_RGB888);
  // Copy the completed Ogre render target on its owning render thread.
  // No extra render, desktop capture, GUI-thread access or pixel rescaling.
  if (!pixels.save(QString::fromStdString(this->capturePath), "PNG"))
    throw std::runtime_error("Could not save Gaussian screenshot: " + this->capturePath);
  gzmsg << "Gaussian rendered image saved: " << this->capturePath
        << " dimensions=" << image.Width() << "x" << image.Height() << std::endl;
  this->captureCompleted = true;
}

void GaussianSplatRendererPrivate::OnPoses(
  const gz::msgs::Pose_V &_message)
{
  for (const auto &pose : _message.pose())
  {
    if (pose.name() != this->cameraTarget)
      continue;
    const auto &position = pose.position();
    const auto &orientation = pose.orientation();
    const std::array<double, 7> values{position.x(), position.y(), position.z(),
      orientation.w(), orientation.x(), orientation.y(), orientation.z()};
    if (!std::all_of(values.begin(), values.end(), [](double v) { return std::isfinite(v); }))
    {
      gzwarn << "Ignoring non-finite Gaussian camera pose" << std::endl;
      return;
    }
    gz::math::Quaterniond rotation(orientation.w(), orientation.x(), orientation.y(), orientation.z());
    if (!std::isfinite(rotation.Dot(rotation)) || rotation.Dot(rotation) < 1e-12)
    {
      gzwarn << "Ignoring zero Gaussian camera quaternion" << std::endl;
      return;
    }
    rotation.Normalize();
    gz::math::Pose3d capturedPose(
      gz::math::Vector3d(position.x(), position.y(), position.z()),
      rotation);
    std::lock_guard<std::mutex> lock(this->targetPoseMutex);
    this->targetPose = capturedPose;
    return;
  }
}

void GaussianSplatRendererPrivate::MaybeFollowCapturedView()
{
  if (!this->capturedView)
    return;
  this->FindCamera();
  if (!this->camera)
    return;

  std::optional<gz::math::Pose3d> pose;
  {
    std::lock_guard<std::mutex> lock(this->targetPoseMutex);
    pose = this->targetPose;
  }
  if (!pose)
    return;
  if (this->lastAppliedTargetPose &&
      pose->Pos().Distance(this->lastAppliedTargetPose->Pos()) < 1e-9 &&
      std::abs(pose->Rot().Dot(this->lastAppliedTargetPose->Rot())) > 1.0 - 1e-12)
  {
    return;
  }

  // Nerfstudio's OpenGL camera looks along -Z (+X right, +Y up), while the
  // Gazebo GUI camera looks along +X (+Y left, +Z up). This fixed local basis
  // rotation maps Gazebo axes onto the recorded Nerfstudio camera axes.
  const gz::math::Quaterniond gazeboToNerfstudio(0.5, -0.5, 0.5, 0.5);
  const gz::math::Pose3d cameraPose(
    pose->Pos(), pose->Rot() * gazeboToNerfstudio);
  this->camera->SetWorldPose(cameraPose);
  this->lastAppliedTargetPose = pose;
  if (!this->capturedViewLogged)
  {
    gzmsg << "Gazebo captured view following " << this->cameraTarget
          << " on " << this->poseTopic
          << " horizontal_fov=" << this->camera->HFOV().Radian()
          << std::endl;
    this->capturedViewLogged = true;
  }
}

void GaussianSplatRendererPrivate::SortForCamera(
  const gz::rendering::CameraPtr &_camera)
{
  if (!_camera || !this->indexBuffer || this->records.empty())
    return;

  constexpr std::size_t kBucketCount = 65'536;
  const auto started = std::chrono::steady_clock::now();
  const auto pose = _camera->WorldPose();
  const auto cameraPosition = pose.Pos();
  const auto cameraForward = pose.Rot().RotateVector(gz::math::Vector3d::UnitX);
  this->depths.resize(this->records.size());
  this->sortedIndices.resize(this->records.size());

  float minimumDepth = std::numeric_limits<float>::max();
  float maximumDepth = std::numeric_limits<float>::lowest();
  for (std::size_t index = 0; index < this->records.size(); ++index)
  {
    const auto &record = this->records[index];
    const auto offset = gz::math::Vector3d(
      record.x, record.y, record.z) - cameraPosition;
    const float depth = static_cast<float>(offset.Dot(cameraForward));
    this->depths[index] = depth;
    minimumDepth = std::min(minimumDepth, depth);
    maximumDepth = std::max(maximumDepth, depth);
  }

  std::array<std::uint32_t, kBucketCount> counts{};
  std::array<std::uint32_t, kBucketCount> cursors{};
  const float bucketScale = maximumDepth > minimumDepth
    ? static_cast<float>(kBucketCount - 1) / (maximumDepth - minimumDepth)
    : 0.0F;
  const auto bucketFor = [&](const float depth)
  {
    return static_cast<std::size_t>(std::clamp(
      (depth - minimumDepth) * bucketScale,
      0.0F, static_cast<float>(kBucketCount - 1)));
  };
  for (const float depth : this->depths)
    ++counts[bucketFor(depth)];

  std::uint32_t output = 0;
  for (std::size_t bucket = kBucketCount; bucket-- > 0;)
  {
    cursors[bucket] = output;
    output += counts[bucket];
  }
  for (std::uint32_t index = 0; index < this->depths.size(); ++index)
  {
    const auto bucket = bucketFor(this->depths[index]);
    this->sortedIndices[cursors[bucket]++] = index;
  }
  void *mappedIndices = this->indexBuffer->map(0, this->sortedIndices.size());
  // Ogre automatically uses uint16 indices below 65,537 vertices. The R&D
  // scene always exceeded that threshold; blindly writing uint32 corrupts
  // small standalone scenes and overruns their mapped GPU allocation.
  if (this->indexBuffer->getIndexType() == Ogre::IndexBufferPacked::IT_16BIT)
  {
    auto *indices16 = static_cast<std::uint16_t *>(mappedIndices);
    std::transform(this->sortedIndices.begin(), this->sortedIndices.end(),
      indices16, [](std::uint32_t index) { return static_cast<std::uint16_t>(index); });
  }
  else
  {
    std::memcpy(mappedIndices, this->sortedIndices.data(),
      this->sortedIndices.size() * sizeof(std::uint32_t));
  }
  this->indexBuffer->unmap(Ogre::UO_UNMAP_ALL);
  this->lastSortedPose = pose;
  this->hasSortedPose = true;
  this->lastSortTime = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    this->lastSortTime - started);
  gzdbg << "Gazebo Gaussian depth order updated in "
        << elapsed.count() << " ms" << std::endl;
}

void GaussianSplatRendererPrivate::MaybeSort()
{
  this->FindCamera();
  if (!this->camera)
    return;

  const auto now = std::chrono::steady_clock::now();
  if (this->hasSortedPose && now - this->lastSortTime < std::chrono::milliseconds(100))
    return;
  const auto pose = this->camera->WorldPose();
  const double positionDelta = pose.Pos().Distance(this->lastSortedPose.Pos());
  const double orientationDelta =
    1.0 - std::abs(pose.Rot().Dot(this->lastSortedPose.Rot()));
  if (!this->hasSortedPose || positionDelta > 0.01 || orientationDelta > 1e-5)
    this->SortForCamera(this->camera);
}

bool GaussianSplatRendererPrivate::Initialize()
{
  const auto loadedEngines = gz::rendering::loadedEngines();
  if (loadedEngines.empty())
    return false;
  auto engine = gz::rendering::engine("ogre2");
  if (!engine || engine->SceneCount() == 0)
    return false;
  this->scene = engine->SceneByIndex(0);
  if (!this->scene || !this->scene->IsInitialized())
    return false;

  auto ogreScene = std::dynamic_pointer_cast<gz::rendering::Ogre2Scene>(this->scene);
  auto root = std::dynamic_pointer_cast<gz::rendering::Ogre2Node>(this->scene->RootVisual());
  if (!ogreScene || !root || !root->Node())
  {
    gzerr << "Gazebo Gaussian renderer requires Gazebo Ogre2" << std::endl;
    this->failed = true;
    return false;
  }

  auto *sceneManager = ogreScene->OgreSceneManager();
  try
  {
    CreateProgram(kVertexProgramName, kVertexShader, Ogre::GPT_VERTEX_PROGRAM);
    CreateProgram(kGeometryProgramName, kGeometryShader, Ogre::GPT_GEOMETRY_PROGRAM);
    CreateProgram(kFragmentProgramName, kFragmentShader, Ogre::GPT_FRAGMENT_PROGRAM);
  }
  catch (const Ogre::Exception &exception)
  {
    gzerr << "Could not compile Gazebo Gaussian shaders: "
          << exception.getFullDescription() << std::endl;
    this->failed = true;
    return false;
  }

  const auto materialName = std::string(kMaterialName) + "_" +
    std::to_string(reinterpret_cast<std::uintptr_t>(this));
  this->material = Ogre::MaterialManager::getSingleton().create(
    materialName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
  auto *pass = this->material->getTechnique(0)->getPass(0);
  pass->setVertexProgram(kVertexProgramName);
  pass->setGeometryProgram(kGeometryProgramName);
  pass->setFragmentProgram(kFragmentProgramName);
  auto vertexParameters = pass->getVertexProgramParameters();
  vertexParameters->setNamedAutoConstant(
    "cameraPosition", Ogre::GpuProgramParameters::ACT_CAMERA_POSITION);
  auto geometryParameters = pass->getGeometryProgramParameters();
  geometryParameters->setNamedAutoConstant(
    "worldView", Ogre::GpuProgramParameters::ACT_WORLDVIEW_MATRIX);
  geometryParameters->setNamedAutoConstant(
    "projection", Ogre::GpuProgramParameters::ACT_PROJECTION_MATRIX);
  geometryParameters->setNamedAutoConstant(
    "viewportSize", Ogre::GpuProgramParameters::ACT_VIEWPORT_SIZE);
  geometryParameters->setNamedConstant(
    "radiusSigma", static_cast<float>(this->radiusSigma));

  Ogre::HlmsMacroblock macroblock = *pass->getMacroblock();
  macroblock.mDepthCheck = true;
  macroblock.mDepthWrite = false;
  macroblock.mCullMode = Ogre::CULL_NONE;
  pass->setMacroblock(macroblock);
  Ogre::HlmsBlendblock blendblock = *pass->getBlendblock();
  blendblock.setBlendType(Ogre::SBT_TRANSPARENT_ALPHA);
  blendblock.setForceTransparentRenderOrder(true);
  pass->setBlendblock(blendblock);
  this->material->setReceiveShadows(false);
  this->material->load();

  this->splats = sceneManager->createManualObject(Ogre::SCENE_DYNAMIC);
  this->splats->estimateVertexCount(this->records.size());
  this->splats->estimateIndexCount(this->records.size());
  this->splats->begin(materialName, Ogre::OT_POINT_LIST);
  std::uint32_t vertexIndex = 0;
  for (const auto &record : this->records)
  {
    this->splats->position(record.x, record.y, record.z);
    std::array<float, 24> packedSphericalHarmonics{};
    for (std::size_t pair = 0; pair < packedSphericalHarmonics.size(); ++pair)
    {
      packedSphericalHarmonics[pair] = PackSphericalHarmonics(
        record.sphericalHarmonics[pair * 2],
        record.sphericalHarmonics[pair * 2 + 1]);
    }
    for (std::size_t group = 0; group < 6; ++group)
    {
      this->splats->textureCoord(
        packedSphericalHarmonics[group * 4],
        packedSphericalHarmonics[group * 4 + 1],
        packedSphericalHarmonics[group * 4 + 2],
        packedSphericalHarmonics[group * 4 + 3]);
    }
    this->splats->textureCoord(
      record.scaleX, record.scaleY, record.scaleZ, record.alpha / 255.0F);
    const double quaternionLength = std::hypot(
      std::hypot(static_cast<double>(record.quaternionX), record.quaternionY),
      std::hypot(static_cast<double>(record.quaternionZ), record.quaternionW));
    const double canonicalScale =
      (record.quaternionW < 0.0F ? -1.0F : 1.0F) /
      quaternionLength;
    this->splats->textureCoord(
      record.quaternionX * canonicalScale,
      record.quaternionY * canonicalScale,
      record.quaternionZ * canonicalScale,
      0.0F);
    this->splats->index(vertexIndex++);
  }
  auto *section = this->splats->end();
  // ManualObject::begin() takes an HLMS datablock name in Ogre 2, not a
  // low-level Material name. Our shaders live in a low-level Material, so it
  // must explicitly override the placeholder datablock after the VAO exists.
  // Without this call Ogre renders the geometry with its white fallback.
  section->setMaterial(this->material);
  section->setCustomParameter(1U, Ogre::Vector4(1.0F, 1.0F, 1.0F, 1.0F));
  const auto &vaos = section->getVaos(Ogre::VpNormal);
  if (vaos.empty() || !vaos.front()->getIndexBuffer())
  {
    gzerr << "Gazebo Gaussian renderer did not create an index buffer"
          << std::endl;
    this->failed = true;
    return false;
  }
  this->indexBuffer = vaos.front()->getIndexBuffer();
  this->splats->setCastShadows(false);
  root->Node()->attachObject(this->splats);
  this->FindCamera();
  this->SortForCamera(this->camera);
  this->initialized = true;
  gzmsg << "Gazebo screen-space Gaussian renderer ready: splats="
        << this->records.size()
        << " radius_sigma=" << this->radiusSigma
        << " projection=full_covariance" << std::endl;
  if (this->camera)
    gzmsg << "Gaussian user camera: " << this->camera->Name()
          << " pose=" << this->camera->WorldPose() << std::endl;
  return true;
}

GaussianSplatRenderer::GaussianSplatRenderer()
  : dataPtr(std::make_unique<GaussianSplatRendererPrivate>())
{
}

GaussianSplatRenderer::~GaussianSplatRenderer()
{
  if (auto *app = gz::gui::App())
    if (auto *window = app->findChild<gz::gui::MainWindow *>())
      window->removeEventFilter(this);
}

void GaussianSplatRenderer::LoadConfig(const tinyxml2::XMLElement *_pluginElem)
{
  if (this->title.empty())
    this->title = "Gazebo Gaussian renderer";
  if (_pluginElem)
  {
    auto readDouble = [this](const tinyxml2::XMLElement *element, double &value)
    {
      if (element && element->QueryDoubleText(&value) != tinyxml2::XML_SUCCESS)
        this->dataPtr->failed = true;
    };
    auto readUnsigned = [this](const tinyxml2::XMLElement *element, unsigned int &value)
    {
      if (element && element->QueryUnsignedText(&value) != tinyxml2::XML_SUCCESS)
        this->dataPtr->failed = true;
    };
    if (const auto *cache = _pluginElem->FirstChildElement("cache_path"); cache && cache->GetText())
      this->dataPtr->cachePath = cache->GetText();
    if (const auto *radius = _pluginElem->FirstChildElement("radius_sigma"); radius)
      readDouble(radius, this->dataPtr->radiusSigma);
    if (const auto *mode = _pluginElem->FirstChildElement("view_mode");
        mode && mode->GetText())
    {
      const std::string viewMode = mode->GetText();
      if (viewMode == "captured")
        this->dataPtr->capturedView = true;
      else if (viewMode != "overview" && viewMode != "diagnostic")
        this->dataPtr->failed = true;
    }
    if (const auto *topic = _pluginElem->FirstChildElement("pose_topic");
        topic && topic->GetText())
      this->dataPtr->poseTopic = topic->GetText();
    if (const auto *target = _pluginElem->FirstChildElement("camera_target");
        target && target->GetText())
      this->dataPtr->cameraTarget = target->GetText();
    if (const auto *fov = _pluginElem->FirstChildElement("horizontal_fov"); fov)
      readDouble(fov, this->dataPtr->horizontalFov);
    if (const auto *path = _pluginElem->FirstChildElement("capture_path");
        path && path->GetText())
      this->dataPtr->capturePath = path->GetText();
    if (const auto *width = _pluginElem->FirstChildElement("capture_width"); width)
      readUnsigned(width, this->dataPtr->captureWidth);
    if (const auto *height = _pluginElem->FirstChildElement("capture_height"); height)
      readUnsigned(height, this->dataPtr->captureHeight);
    if (const auto *fx = _pluginElem->FirstChildElement("capture_fx"); fx)
      readDouble(fx, this->dataPtr->captureFx);
    if (const auto *fy = _pluginElem->FirstChildElement("capture_fy"); fy)
      readDouble(fy, this->dataPtr->captureFy);
    if (const auto *cx = _pluginElem->FirstChildElement("capture_cx"); cx)
      readDouble(cx, this->dataPtr->captureCx);
    if (const auto *cy = _pluginElem->FirstChildElement("capture_cy"); cy)
      readDouble(cy, this->dataPtr->captureCy);
  }
  const bool invalidCapture = (this->dataPtr->captureWidth != 0 ||
    this->dataPtr->captureHeight != 0) &&
    (!this->dataPtr->capturedView || this->dataPtr->captureWidth == 0 ||
     this->dataPtr->captureHeight == 0 || this->dataPtr->captureWidth > 16384 ||
     this->dataPtr->captureHeight > 16384 ||
     !std::isfinite(this->dataPtr->captureFx) || this->dataPtr->captureFx <= 0.0 ||
     !std::isfinite(this->dataPtr->captureFy) || this->dataPtr->captureFy <= 0.0 ||
     !std::isfinite(this->dataPtr->captureCx) ||
     !std::isfinite(this->dataPtr->captureCy));
  if (this->dataPtr->failed || this->dataPtr->cachePath.empty() ||
      !std::isfinite(this->dataPtr->radiusSigma) ||
      this->dataPtr->radiusSigma < 1.0 || this->dataPtr->radiusSigma > 6.0 ||
      (this->dataPtr->capturedView &&
       this->dataPtr->poseTopic.empty() != this->dataPtr->cameraTarget.empty()) ||
      !std::isfinite(this->dataPtr->horizontalFov) ||
      this->dataPtr->horizontalFov < 0.0 || this->dataPtr->horizontalFov >= M_PI ||
      invalidCapture)
  {
    gzerr << "Gazebo Gaussian renderer has invalid configuration: cache_path='"
          << this->dataPtr->cachePath << "' radius_sigma="
          << this->dataPtr->radiusSigma << " captured_view="
          << this->dataPtr->capturedView << " pose_topic='"
          << this->dataPtr->poseTopic << "' camera_target='"
          << this->dataPtr->cameraTarget << "' horizontal_fov="
          << this->dataPtr->horizontalFov << " capture_path='"
          << this->dataPtr->capturePath << "'" << std::endl;
    this->dataPtr->failed = true;
    return;
  }
  try
  {
    this->dataPtr->records = ReadCache(this->dataPtr->cachePath);
  }
  catch (const std::exception &error)
  {
    gzerr << error.what() << std::endl;
    this->dataPtr->failed = true;
    return;
  }
  if (this->dataPtr->capturedView && !this->dataPtr->poseTopic.empty() &&
      !this->dataPtr->transportNode.Subscribe(
        this->dataPtr->poseTopic,
        &GaussianSplatRendererPrivate::OnPoses,
        this->dataPtr.get()))
  {
    gzerr << "Gazebo captured view could not subscribe to "
          << this->dataPtr->poseTopic << std::endl;
    this->dataPtr->failed = true;
    return;
  }
  auto *window = gz::gui::App()->findChild<gz::gui::MainWindow *>();
  if (!window)
  {
    gzerr << "GaussianSplatRenderer requires a Gazebo GUI main window" << std::endl;
    this->dataPtr->failed = true;
    return;
  }
  window->installEventFilter(this);
}

bool GaussianSplatRenderer::eventFilter(QObject *_object, QEvent *_event)
{
  if (_event->type() == gz::gui::events::PreRender::kType)
  {
    if (!this->dataPtr->initialized && !this->dataPtr->failed)
    {
      try
      {
        this->dataPtr->Initialize();
      }
      catch (const Ogre::Exception &exception)
      {
        gzerr << "Gazebo Gaussian renderer initialization failed: "
              << exception.getFullDescription() << std::endl;
        this->dataPtr->failed = true;
      }
      catch (const std::exception &exception)
      {
        gzerr << "Gazebo Gaussian renderer initialization failed: "
              << exception.what() << std::endl;
        this->dataPtr->failed = true;
      }
    }
    else if (this->dataPtr->initialized)
    {
      this->dataPtr->MaybeFollowCapturedView();
      this->dataPtr->MaybeSort();
    }
  }
  else if (_event->type() == gz::gui::events::Render::kType)
  {
    try
    {
      this->dataPtr->MaybeCaptureRenderedFrame();
    }
    catch (const std::exception &exception)
    {
      gzerr << "Gaussian screenshot failed: " << exception.what() << std::endl;
      this->dataPtr->captureCompleted = true;
    }
  }
  return QObject::eventFilter(_object, _event);
}
}  // namespace gz_gaussian_splatting

GZ_ADD_PLUGIN(
  gz_gaussian_splatting::GaussianSplatRenderer,
  gz::gui::Plugin)
