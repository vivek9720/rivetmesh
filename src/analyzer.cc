#include "rivetmesh/analyzer.h"

#include <sstream>

#include "rivetmesh/bundle.h"
#include "rivetmesh/checksum.h"
#include "rivetmesh/profile.h"
#include "rivetmesh/reader.h"
#include "rivetmesh/route.h"
#include "rivetmesh/rules.h"
#include "rivetmesh/stencil.h"
#include "rivetmesh/table.h"

namespace rivetmesh {

Outcome<Analysis> analyzeBundleBytes(const std::uint8_t* data, std::size_t size) {
  auto bundle = parseBundle(data, size);
  if (!bundle) {
    return Outcome<Analysis>::err(bundle.status());
  }
  auto valid = validateBundle(bundle.value());
  if (!valid) {
    return Outcome<Analysis>::err(valid.status());
  }

  Analysis analysis;
  analysis.sections = bundle.value().sections.size();
  analysis.digest = meshDigest(data, size);

  auto profile = buildProfile(bundle.value());
  if (!profile) {
    return Outcome<Analysis>::err(profile.status());
  }
  analysis.profile_findings = profile.value().findings.size();
  analysis.profile_signature = profile.value().signature;

  for (const auto& section : bundle.value().sections) {
    const std::string body = toString(section.body);
    switch (section.kind) {
      case SectionKind::manifest:
        analysis.notes += "manifest:" + section.name + ";";
        analysis.digest ^= meshDigest(body);
        break;
      case SectionKind::rules: {
        auto rules = parseRules(body);
        if (!rules) {
          return Outcome<Analysis>::err(rules.status());
        }
        analysis.rule_nodes += countRuleNodes(rules.value());
        break;
      }
      case SectionKind::table: {
        auto table = parseTable(body);
        if (!table) {
          return Outcome<Analysis>::err(table.status());
        }
        analysis.table_rows += table.value().rows.size();
        auto valid_table = validateTable(table.value());
        if (!valid_table) {
          return Outcome<Analysis>::err(valid_table.status());
        }
        analysis.digest ^= meshDigest(tableSummary(table.value()));
        break;
      }
      case SectionKind::route: {
        auto graph = parseRouteGraph(body);
        if (!graph) {
          return Outcome<Analysis>::err(graph.status());
        }
        auto stats = analyzeRouteGraph(graph.value());
        if (!stats) {
          return Outcome<Analysis>::err(stats.status());
        }
        analysis.route_nodes += graph.value().nodes.size();
        analysis.route_edges += graph.value().edges.size();
        analysis.digest ^= meshDigest(routeSummary(graph.value(), stats.value()));
        break;
      }
      case SectionKind::stencil: {
        auto stencil = parseStencil(body);
        if (!stencil) {
          return Outcome<Analysis>::err(stencil.status());
        }
        analysis.stencil_patches += stencil.value().patches.size();
        analysis.stencil_bytes += stencil.value().expanded_bytes;
        analysis.digest ^= meshDigest(stencilSummary(stencil.value()));
        break;
      }
      case SectionKind::note:
        analysis.notes += body;
        analysis.digest ^= meshDigest(body);
        break;
      case SectionKind::unknown:
        return Outcome<Analysis>::err(fail(Errc::unknown_section, "unknown section in analyzer"));
    }
  }

  return Outcome<Analysis>::ok(std::move(analysis));
}

std::string formatAnalysis(const Analysis& analysis) {
  std::ostringstream out;
  out << "sections=" << analysis.sections << '\n';
  out << "rule_nodes=" << analysis.rule_nodes << '\n';
  out << "table_rows=" << analysis.table_rows << '\n';
  out << "route_nodes=" << analysis.route_nodes << '\n';
  out << "route_edges=" << analysis.route_edges << '\n';
  out << "stencil_patches=" << analysis.stencil_patches << '\n';
  out << "stencil_bytes=" << analysis.stencil_bytes << '\n';
  out << "profile_findings=" << analysis.profile_findings << '\n';
  out << "profile_signature=" << analysis.profile_signature << '\n';
  out << "digest=" << analysis.digest << '\n';
  if (!analysis.notes.empty()) {
    out << "notes=" << analysis.notes << '\n';
  }
  return out.str();
}

}  // namespace rivetmesh
