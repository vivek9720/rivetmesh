#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "rivetmesh/status.h"

namespace rivetmesh {

enum class SectionKind {
  manifest,
  rules,
  table,
  route,
  stencil,
  note,
  unknown,
};

struct Section {
  std::string name;
  SectionKind kind = SectionKind::unknown;
  std::uint32_t flags = 0;
  std::uint32_t digest = 0;
  std::vector<std::uint8_t> body;
};

struct Bundle {
  std::uint16_t version = 1;
  std::map<std::string, std::string> metadata;
  std::vector<Section> sections;
};

struct BundleLimits {
  std::size_t max_sections = 96;
  std::size_t max_payload = 1 << 20;
  std::size_t max_lines = 8192;
};

const char* sectionKindName(SectionKind kind);
SectionKind sectionKindFromName(const std::string& text);

Outcome<Bundle> parseBundle(const std::uint8_t* data, std::size_t size,
                            BundleLimits limits = BundleLimits{});
Outcome<Bundle> parseTextBundle(const std::uint8_t* data, std::size_t size,
                                BundleLimits limits = BundleLimits{});
Outcome<Bundle> parseBinaryBundle(const std::uint8_t* data, std::size_t size,
                                  BundleLimits limits = BundleLimits{});
Outcome<void> validateBundle(const Bundle& bundle);

}  // namespace rivetmesh
