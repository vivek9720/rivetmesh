#include "rivetmesh/checksum.h"

namespace rivetmesh {

std::uint32_t meshDigest(const std::uint8_t* data, std::size_t size) {
  std::uint32_t state = 0x6d657368u;
  for (std::size_t i = 0; i < size; ++i) {
    state ^= data[i];
    state *= 0x45d9f3bu;
    state ^= (state >> 16);
    state = (state << 7) | (state >> 25);
  }
  return state;
}

std::uint32_t meshDigest(const std::vector<std::uint8_t>& bytes) {
  return meshDigest(bytes.data(), bytes.size());
}

std::uint32_t meshDigest(const std::string& text) {
  return meshDigest(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}

}  // namespace rivetmesh
