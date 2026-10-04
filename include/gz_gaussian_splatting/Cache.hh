// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Solvor Labs
#ifndef GZ_GAUSSIAN_SPLATTING_CACHE_HH_
#define GZ_GAUSSIAN_SPLATTING_CACHE_HH_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace gz_gaussian_splatting
{
inline constexpr std::array<char, 8> kMagic{'G', 'Z', 'G', 'S', '0', '0', '0', '1'};
inline constexpr std::uint32_t kMaximumRecords = 2'000'000;

#pragma pack(push, 1)
struct GaussianRecord
{
  float x, y, z;
  float scaleX, scaleY, scaleZ;
  float quaternionX, quaternionY, quaternionZ, quaternionW;
  std::array<float, 48> sphericalHarmonics;
  std::uint8_t red, green, blue, alpha;
};
#pragma pack(pop)
static_assert(sizeof(GaussianRecord) == 236, "Gaussian cache ABI changed");

/// Strict v1 cache loading. Invalid files throw std::runtime_error.
std::vector<GaussianRecord> ReadCache(const std::string &path);
/// Two signed SH coefficients packed into an exactly representable float.
float PackSphericalHarmonics(float first, float second);
}
#endif
