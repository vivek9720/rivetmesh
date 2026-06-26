#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "rivetmesh/bundle.h"
#include "rivetmesh/status.h"

namespace rivetmesh {

enum class FindingLevel {
  info,
  warning,
  error,
};

struct Finding {
  FindingLevel level = FindingLevel::info;
  std::string area;
  std::string message;
  std::size_t location = 0;
};

struct SectionProfile {
  std::string name;
  SectionKind kind = SectionKind::unknown;
  std::size_t bytes = 0;
  bool digest_present = false;
};

struct TableProfile {
  std::size_t tables = 0;
  std::size_t columns = 0;
  std::size_t rows = 0;
  std::set<std::string> column_names;
};

struct RouteProfile {
  std::size_t graphs = 0;
  std::size_t nodes = 0;
  std::size_t edges = 0;
  std::size_t dangling_edges = 0;
  std::uint32_t flags = 0;
};

struct StencilProfile {
  std::size_t stencils = 0;
  std::size_t patches = 0;
  std::size_t expanded_bytes = 0;
  std::set<std::string> layers;
};

struct BundleProfile {
  std::vector<SectionProfile> sections;
  std::vector<Finding> findings;
  std::map<std::string, std::size_t> metadata_keys;
  TableProfile table;
  RouteProfile route;
  StencilProfile stencil;
  std::size_t rule_nodes = 0;
  std::uint32_t signature = 0;
};

struct ProfileOptions {
  bool require_rules = false;
  bool require_table = false;
  bool require_route = false;
  bool require_stencil = false;
  std::size_t max_metadata_value = 512;
  std::size_t max_patch_payload = 1 << 20;
};

const char* findingLevelName(FindingLevel level);

Outcome<BundleProfile> buildProfile(const Bundle& bundle,
                                    ProfileOptions options = ProfileOptions{});
Outcome<BundleProfile> profileBytes(const std::uint8_t* data, std::size_t size,
                                    ProfileOptions options = ProfileOptions{});
Outcome<void> ensureProfileHasNoErrors(const BundleProfile& profile);
std::string formatProfile(const BundleProfile& profile);

}  // namespace rivetmesh
