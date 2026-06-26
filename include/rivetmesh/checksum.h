#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rivetmesh {

std::uint32_t meshDigest(const std::uint8_t* data, std::size_t size);
std::uint32_t meshDigest(const std::vector<std::uint8_t>& bytes);
std::uint32_t meshDigest(const std::string& text);

}  // namespace rivetmesh
