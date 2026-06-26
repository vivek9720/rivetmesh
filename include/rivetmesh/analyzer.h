#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "rivetmesh/status.h"

namespace rivetmesh {

struct Analysis {
  std::size_t sections = 0;
  std::size_t rule_nodes = 0;
  std::size_t table_rows = 0;
  std::size_t route_nodes = 0;
  std::size_t route_edges = 0;
  std::size_t stencil_patches = 0;
  std::size_t stencil_bytes = 0;
  std::size_t profile_findings = 0;
  std::uint32_t profile_signature = 0;
  std::uint32_t digest = 0;
  std::string notes;
};

Outcome<Analysis> analyzeBundleBytes(const std::uint8_t* data, std::size_t size);
std::string formatAnalysis(const Analysis& analysis);

}  // namespace rivetmesh
