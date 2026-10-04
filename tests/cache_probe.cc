// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Solvor Labs
#include "gz_gaussian_splatting/Cache.hh"
#include <iostream>
int main(int argc, char **argv)
{
  if (argc != 2) return 2;
  try
  {
    const auto records = gz_gaussian_splatting::ReadCache(argv[1]);
    const auto &record = records.front();
    std::cout << records.size() << ' ' << record.x << ' ' << record.y << ' '
              << record.z << ' ' << record.scaleY << ' ' << record.quaternionW
              << ' ' << static_cast<unsigned int>(record.alpha) << '\n';
    return 0;
  }
  catch (const std::exception &error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
