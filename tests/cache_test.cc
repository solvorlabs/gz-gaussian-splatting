// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Solvor Labs
#include "gz_gaussian_splatting/Cache.hh"
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace gz_gaussian_splatting;

void require(bool value, const char *message)
{
  if (!value) throw std::runtime_error(message);
}
int main()
{
  const auto path = std::filesystem::temp_directory_path() /
    ("gzgs-test-" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
  auto write = [&](GaussianRecord record, bool trailing = false) {
    std::ofstream output(path, std::ios::binary);
    std::uint32_t count = 1;
    output.write(kMagic.data(), 8);
    output.write(reinterpret_cast<char *>(&count), 4);
    output.write(reinterpret_cast<char *>(&record), sizeof(record));
    if (trailing) output.put('x');
  };
  auto rejected = [&]() {
    try { ReadCache(path.string()); return false; }
    catch (const std::runtime_error &) { return true; }
  };
  try
  {
    GaussianRecord record{};
    record.scaleX = record.scaleY = record.scaleZ = .05F;
    record.quaternionW = 1;
    record.alpha = 255;
    write(record);
    require(ReadCache(path.string()).size() == 1, "valid cache rejected");
    write(record, true);
    require(rejected(), "trailing cache bytes accepted");
    record.quaternionW = 0;
    write(record);
    require(rejected(), "zero quaternion accepted");
    record.quaternionW = 1;
    record.scaleX = -1;
    write(record);
    require(rejected(), "negative scale accepted");
    for (float value : {-8.0F, -3.0F, 0.0F, 2.0F, 8.0F})
    {
      auto packed = static_cast<std::uint32_t>(PackSphericalHarmonics(value, -value));
      const float first = float(packed & 4095U) / 4095 * 16 - 8;
      const float second = float((packed >> 12U) & 4095U) / 4095 * 16 - 8;
      require(std::abs(first-value) < .0021F && std::abs(second+value) < .0021F,
              "SH packing exceeds quantization bound");
    }
    std::filesystem::remove(path);
    std::cout << "cache validation and SH quantization passed\n";
    return 0;
  }
  catch (const std::exception &error)
  {
    std::filesystem::remove(path);
    std::cerr << error.what() << '\n';
    return 1;
  }
}
