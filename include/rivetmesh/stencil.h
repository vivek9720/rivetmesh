#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "rivetmesh/status.h"

namespace rivetmesh {

struct Patch {
  std::string id;
  std::string layer;
  std::uint32_t x = 0;
  std::uint32_t y = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t digest = 0;
  std::vector<std::uint8_t> payload;
};

struct Stencil {
  std::string tile_id;
  std::uint32_t revision = 0;
  std::vector<Patch> patches;
  std::size_t expanded_bytes = 0;
};

Outcome<Stencil> parseStencil(const std::string& text);
std::string stencilSummary(const Stencil& stencil);

}  // namespace rivetmesh
