// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Solvor Labs
#ifndef GZ_GAUSSIAN_SPLATTING_RENDERER_HH_
#define GZ_GAUSSIAN_SPLATTING_RENDERER_HH_

#include <memory>

#include <gz/gui/Plugin.hh>

namespace gz_gaussian_splatting
{
class GaussianSplatRendererPrivate;

/// Render covariance-bearing Gazebo splats directly in Gazebo's Ogre2 scene.
class GaussianSplatRenderer final : public gz::gui::Plugin
{
  Q_OBJECT

  public: GaussianSplatRenderer();
  public: ~GaussianSplatRenderer() override;

  protected: void LoadConfig(const tinyxml2::XMLElement *_pluginElem) override;
  public: bool eventFilter(QObject *_object, QEvent *_event) override;

  private: std::unique_ptr<GaussianSplatRendererPrivate> dataPtr;
};
}  // namespace gz_gaussian_splatting

#endif
