#include "rivetmesh/profile.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "rivetmesh/checksum.h"
#include "rivetmesh/reader.h"
#include "rivetmesh/route.h"
#include "rivetmesh/rules.h"
#include "rivetmesh/stencil.h"
#include "rivetmesh/table.h"

namespace rivetmesh {
namespace {

void addFinding(BundleProfile& profile, FindingLevel level, std::string area,
                std::string message, std::size_t location = 0) {
  profile.findings.push_back(Finding{level, std::move(area), std::move(message), location});
}

bool validMetaKey(const std::string& key) {
  if (key.empty()) {
    return false;
  }
  for (char ch : key) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (!std::isalnum(uch) && ch != '_' && ch != '-' && ch != '.') {
      return false;
    }
  }
  return true;
}

std::string normalized(std::string text) {
  std::string out;
  out.reserve(text.size());
  bool last_sep = false;
  for (char ch : text) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (std::isalnum(uch)) {
      out.push_back(static_cast<char>(std::tolower(uch)));
      last_sep = false;
    } else if (!last_sep) {
      out.push_back('_');
      last_sep = true;
    }
  }
  while (!out.empty() && out.back() == '_') {
    out.pop_back();
  }
  while (!out.empty() && out.front() == '_') {
    out.erase(out.begin());
  }
  return out.empty() ? "default" : out;
}

std::uint32_t mix(std::uint32_t state, const std::string& text) {
  const std::uint32_t piece = meshDigest(text);
  state ^= piece + 0x9e3779b9u + (state << 6) + (state >> 2);
  return state;
}

std::uint32_t mix(std::uint32_t state, std::uint64_t value) {
  std::string bytes;
  for (int i = 0; i < 8; ++i) {
    bytes.push_back(static_cast<char>((value >> (i * 8)) & 0xff));
  }
  return mix(state, bytes);
}

void signProfile(BundleProfile& profile) {
  std::uint32_t sig = 0x524d5052u;
  for (const auto& section : profile.sections) {
    sig = mix(sig, section.name);
    sig = mix(sig, sectionKindName(section.kind));
    sig = mix(sig, section.bytes);
    sig = mix(sig, section.digest_present ? 1 : 0);
  }
  for (const auto& item : profile.metadata_keys) {
    sig = mix(sig, item.first);
    sig = mix(sig, item.second);
  }
  sig = mix(sig, profile.table.tables);
  sig = mix(sig, profile.table.columns);
  sig = mix(sig, profile.table.rows);
  for (const auto& name : profile.table.column_names) {
    sig = mix(sig, name);
  }
  sig = mix(sig, profile.route.graphs);
  sig = mix(sig, profile.route.nodes);
  sig = mix(sig, profile.route.edges);
  sig = mix(sig, profile.route.dangling_edges);
  sig = mix(sig, profile.route.flags);
  sig = mix(sig, profile.stencil.stencils);
  sig = mix(sig, profile.stencil.patches);
  sig = mix(sig, profile.stencil.expanded_bytes);
  for (const auto& layer : profile.stencil.layers) {
    sig = mix(sig, layer);
  }
  sig = mix(sig, profile.rule_nodes);
  profile.signature = sig;
}

void inspectMetadata(const Bundle& bundle, BundleProfile& profile, ProfileOptions options) {
  for (const auto& item : bundle.metadata) {
    profile.metadata_keys[normalized(item.first)]++;
    if (!validMetaKey(item.first)) {
      addFinding(profile, FindingLevel::warning, "metadata",
                 "metadata key contains nonportable characters");
    }
    if (item.second.size() > options.max_metadata_value) {
      addFinding(profile, FindingLevel::warning, "metadata",
                 "metadata value is unusually large");
    }
  }
  if (bundle.metadata.find("producer") == bundle.metadata.end()) {
    addFinding(profile, FindingLevel::info, "metadata", "bundle has no producer key");
  }
}

void inspectInventory(const Bundle& bundle, BundleProfile& profile, ProfileOptions options) {
  bool saw_rules = false;
  bool saw_table = false;
  bool saw_route = false;
  bool saw_stencil = false;
  std::set<std::string> names;

  for (const auto& section : bundle.sections) {
    profile.sections.push_back(SectionProfile{section.name, section.kind, section.body.size(),
                                              section.digest != 0});
    if (!names.insert(section.name).second) {
      addFinding(profile, FindingLevel::warning, "bundle", "duplicate section name");
    }
    if (section.body.empty()) {
      addFinding(profile, FindingLevel::info, "bundle", "section payload is empty");
    }
    switch (section.kind) {
      case SectionKind::rules:
        saw_rules = true;
        break;
      case SectionKind::table:
        saw_table = true;
        break;
      case SectionKind::route:
        saw_route = true;
        break;
      case SectionKind::stencil:
        saw_stencil = true;
        break;
      case SectionKind::unknown:
        addFinding(profile, FindingLevel::error, "bundle", "unknown section kind");
        break;
      case SectionKind::manifest:
      case SectionKind::note:
        break;
    }
  }

  if (options.require_rules && !saw_rules) {
    addFinding(profile, FindingLevel::error, "bundle", "required rules section missing");
  }
  if (options.require_table && !saw_table) {
    addFinding(profile, FindingLevel::error, "bundle", "required table section missing");
  }
  if (options.require_route && !saw_route) {
    addFinding(profile, FindingLevel::error, "bundle", "required route section missing");
  }
  if (options.require_stencil && !saw_stencil) {
    addFinding(profile, FindingLevel::error, "bundle", "required stencil section missing");
  }
}

void inspectRules(const Section& section, BundleProfile& profile) {
  auto rules = parseRules(toString(section.body));
  if (!rules) {
    addFinding(profile, FindingLevel::error, "rules", "rules section failed to parse",
               rules.status().offset);
    return;
  }
  const std::size_t nodes = countRuleNodes(rules.value());
  profile.rule_nodes += nodes;
  if (nodes > 10000) {
    addFinding(profile, FindingLevel::warning, "rules", "rules tree is unusually large");
  }
}

void inspectTable(const Section& section, BundleProfile& profile) {
  auto table = parseTable(toString(section.body));
  if (!table) {
    addFinding(profile, FindingLevel::error, "table", "table section failed to parse",
               table.status().offset);
    return;
  }
  ++profile.table.tables;
  profile.table.columns += table.value().columns.size();
  profile.table.rows += table.value().rows.size();
  for (const auto& column : table.value().columns) {
    profile.table.column_names.insert(normalized(column.name));
  }
  auto valid = validateTable(table.value());
  if (!valid) {
    addFinding(profile, FindingLevel::warning, "table", "table validation warning",
               valid.status().offset);
  }
}

void inspectRoute(const Section& section, BundleProfile& profile) {
  auto graph = parseRouteGraph(toString(section.body));
  if (!graph) {
    addFinding(profile, FindingLevel::error, "route", "route section failed to parse",
               graph.status().offset);
    return;
  }
  auto stats = analyzeRouteGraph(graph.value());
  if (!stats) {
    addFinding(profile, FindingLevel::error, "route", "route analysis failed",
               stats.status().offset);
    return;
  }
  ++profile.route.graphs;
  profile.route.nodes += graph.value().nodes.size();
  profile.route.edges += graph.value().edges.size();
  profile.route.dangling_edges += stats.value().dangling_edges;
  profile.route.flags |= stats.value().flag_union;
  if (stats.value().dangling_edges != 0) {
    addFinding(profile, FindingLevel::warning, "route", "route contains dangling edges");
  }
}

void inspectStencil(const Section& section, BundleProfile& profile, ProfileOptions options) {
  auto stencil = parseStencil(toString(section.body));
  if (!stencil) {
    addFinding(profile, FindingLevel::error, "stencil", "stencil section failed to parse",
               stencil.status().offset);
    return;
  }
  ++profile.stencil.stencils;
  profile.stencil.patches += stencil.value().patches.size();
  profile.stencil.expanded_bytes += stencil.value().expanded_bytes;
  for (const auto& patch : stencil.value().patches) {
    profile.stencil.layers.insert(normalized(patch.layer));
    if (patch.payload.size() > options.max_patch_payload) {
      addFinding(profile, FindingLevel::warning, "stencil", "large patch payload");
    }
    if (patch.width == 0 || patch.height == 0) {
      addFinding(profile, FindingLevel::info, "stencil", "patch omits dimensions");
    }
  }
}

}  // namespace

const char* findingLevelName(FindingLevel level) {
  switch (level) {
    case FindingLevel::info:
      return "info";
    case FindingLevel::warning:
      return "warning";
    case FindingLevel::error:
      return "error";
  }
  return "unknown";
}

Outcome<BundleProfile> buildProfile(const Bundle& bundle, ProfileOptions options) {
  auto valid = validateBundle(bundle);
  if (!valid) {
    return Outcome<BundleProfile>::err(valid.status());
  }

  BundleProfile profile;
  inspectMetadata(bundle, profile, options);
  inspectInventory(bundle, profile, options);
  for (const auto& section : bundle.sections) {
    switch (section.kind) {
      case SectionKind::rules:
        inspectRules(section, profile);
        break;
      case SectionKind::table:
        inspectTable(section, profile);
        break;
      case SectionKind::route:
        inspectRoute(section, profile);
        break;
      case SectionKind::stencil:
        inspectStencil(section, profile, options);
        break;
      case SectionKind::manifest:
      case SectionKind::note:
      case SectionKind::unknown:
        break;
    }
  }
  signProfile(profile);
  return Outcome<BundleProfile>::ok(std::move(profile));
}

Outcome<BundleProfile> profileBytes(const std::uint8_t* data, std::size_t size,
                                    ProfileOptions options) {
  auto bundle = parseBundle(data, size);
  if (!bundle) {
    return Outcome<BundleProfile>::err(bundle.status());
  }
  return buildProfile(bundle.value(), options);
}

Outcome<void> ensureProfileHasNoErrors(const BundleProfile& profile) {
  for (const auto& finding : profile.findings) {
    if (finding.level == FindingLevel::error) {
      return Outcome<void>::err(fail(Errc::state, "profile contains an error finding",
                                     finding.location));
    }
  }
  return Outcome<void>::success();
}

std::string formatProfile(const BundleProfile& profile) {
  std::ostringstream out;
  out << "profile_sections=" << profile.sections.size() << '\n';
  out << "profile_rules=" << profile.rule_nodes << '\n';
  out << "profile_tables=" << profile.table.tables << '\n';
  out << "profile_rows=" << profile.table.rows << '\n';
  out << "profile_routes=" << profile.route.graphs << '\n';
  out << "profile_edges=" << profile.route.edges << '\n';
  out << "profile_stencils=" << profile.stencil.stencils << '\n';
  out << "profile_patches=" << profile.stencil.patches << '\n';
  out << "profile_signature=" << profile.signature << '\n';
  for (const auto& finding : profile.findings) {
    out << "finding=" << findingLevelName(finding.level) << ":" << finding.area << ":"
        << finding.message;
    if (finding.location != 0) {
      out << "@" << finding.location;
    }
    out << '\n';
  }
  return out.str();
}

}  // namespace rivetmesh
