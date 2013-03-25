#ifndef TUVOK_RENDERER_WRITEBRICK_H
#define TUVOK_RENDERER_WRITEBRICK_H

#include <array>
#include <cstdio>
#include <fstream>
#include <vector>
#include "Basics/Checksums/MD5.h"
#include "IO/Brick.h"

#ifdef _MSC_VER
# define snprintf _snprintf
#endif

// Debug code: write data's md5 hash to a file.

namespace {
  template<typename T>
  void writeBrick(const tuvok::BrickKey& k, const std::vector<T>& data) {
    char filename[1024];
    snprintf(filename, 1024, "%u-%02u-%03u.md5",
             static_cast<unsigned>(std::get<0>(k)),
             static_cast<unsigned>(std::get<1>(k)),
             static_cast<unsigned>(std::get<2>(k)));
    std::ofstream ofs(filename, std::ios::trunc);
    ofs << std::hex;
    std::array<uint8_t,16> checksum =
      md5<typename std::vector<T>::const_iterator,T>(data.begin(), data.end());
    for(auto i = checksum.begin(); i != checksum.end(); ++i) {
      ofs << uint16_t(*i);
    }
    ofs << "\n";
    ofs.close();
  }

  template<typename Iter, typename T>
  void writeBrickT(const tuvok::BrickKey& k, Iter begin, Iter end) {
    char filename[1024];
    snprintf(filename, 1024, "%u-%02u-%03u.md5",
             static_cast<unsigned>(std::get<0>(k)),
             static_cast<unsigned>(std::get<1>(k)),
             static_cast<unsigned>(std::get<2>(k)));
    std::ofstream ofs(filename, std::ios::trunc);
    ofs << std::hex;
    std::array<uint8_t,16> checksum = md5<Iter,T>(begin, end);
    for(auto i = checksum.begin(); i != checksum.end(); ++i) {
      ofs << uint16_t(*i);
    }
    ofs << "\n";
    ofs.close();
  }
}
#endif
