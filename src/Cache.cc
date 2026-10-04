// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Solvor Labs
#include "gz_gaussian_splatting/Cache.hh"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace gz_gaussian_splatting
{
std::vector<GaussianRecord> ReadCache(const std::string &path)
{
  const std::uint16_t endian = 1;
  if (*reinterpret_cast<const unsigned char *>(&endian) != 1)
    throw std::runtime_error("v1 cache requires a little-endian host");
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input)
    throw std::runtime_error("Gaussian cache not found: " + path);
  const auto length = input.tellg();
  input.seekg(0);
  std::array<char, 8> magic{};
  std::uint32_t count{};
  input.read(magic.data(), magic.size());
  input.read(reinterpret_cast<char *>(&count), sizeof(count));
  if (!input || magic != kMagic || count == 0 || count > kMaximumRecords)
    throw std::runtime_error("Invalid Gaussian cache header: " + path);
  const auto expected = static_cast<std::streamoff>(12) +
    static_cast<std::streamoff>(count) * static_cast<std::streamoff>(sizeof(GaussianRecord));
  if (length != expected)
    throw std::runtime_error("Gaussian cache length does not match record count: " + path);
  std::vector<GaussianRecord> records(count);
  input.read(reinterpret_cast<char *>(records.data()), count * sizeof(GaussianRecord));
  if (!input)
    throw std::runtime_error("Cannot read Gaussian cache: " + path);
  for (const auto &record : records)
  {
    const std::array<float, 10> values{record.x, record.y, record.z,
      record.scaleX, record.scaleY, record.scaleZ, record.quaternionX,
      record.quaternionY, record.quaternionZ, record.quaternionW};
    if (!std::all_of(values.begin(), values.end(), [](float v) { return std::isfinite(v); }) ||
        record.scaleX <= 0 || record.scaleY <= 0 || record.scaleZ <= 0 ||
        !std::all_of(record.sphericalHarmonics.begin(), record.sphericalHarmonics.end(),
          [](float v) { return std::isfinite(v); }))
      throw std::runtime_error("Gaussian cache has non-finite or invalid data: " + path);
    const double norm = static_cast<double>(record.quaternionX) * record.quaternionX +
      static_cast<double>(record.quaternionY) * record.quaternionY +
      static_cast<double>(record.quaternionZ) * record.quaternionZ +
      static_cast<double>(record.quaternionW) * record.quaternionW;
    if (norm < 1e-12 || !std::isfinite(norm))
      throw std::runtime_error("Gaussian cache has a zero/invalid quaternion: " + path);
  }
  return records;
}

float PackSphericalHarmonics(float first, float second)
{
  const auto encode = [](float value) {
    return static_cast<std::uint32_t>(std::lround(
      std::clamp((value + 8.0F) / 16.0F, 0.0F, 1.0F) * 4095.0F));
  };
  return static_cast<float>(encode(first) | (encode(second) << 12U));
}
}
