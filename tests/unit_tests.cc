#include <cassert>
#include <cstdint>
#include <string>

#include "rivetmesh/analyzer.h"
#include "rivetmesh/bundle.h"
#include "rivetmesh/profile.h"
#include "rivetmesh/route.h"
#include "rivetmesh/rules.h"
#include "rivetmesh/stencil.h"
#include "rivetmesh/table.h"

namespace {

std::string sampleBundle() {
  return R"(RIVETMESH/1
version 1
meta producer=test
section policy rules
let max_cost = 12 + 3 * 4;
tile {
  let speed = weighted(max_cost, 2);
}
.
section records table
TBL2
schema id:int name:text active:flag score:decimal
row id=1 name="alpha" active=true score=7.5
row id=2 name="beta" active=false score=4.25
.
section graph route
RTE2
node 1 0.0 0.0 depot
node 2 1.0 0.0 relay
edge 1 2 3.5 1
.
section patches stencil
STN2
tile id=tile-17 rev=3
patch id=base layer=roads x=0 y=0 w=2 h=2 data="abcd"
patch id=mask layer=land x=2 y=1 w=1 h=1 data="\x41~1:B"
.
end
)";
}

void testRules() {
  auto parsed = rivetmesh::parseRules("let a = 1 + 2 * 3; block { let b = mix(a, 4); }");
  assert(parsed);
  assert(rivetmesh::countRuleNodes(parsed.value()) >= 8);
}

void testTable() {
  auto parsed = rivetmesh::parseTable(
      "TBL2\nschema id:int label:text ok:flag\nrow id=9 label=\"nine\" ok=true\n");
  assert(parsed);
  auto valid = rivetmesh::validateTable(parsed.value());
  assert(valid);
  assert(parsed.value().rows.size() == 1);
}

void testRoute() {
  auto graph = rivetmesh::parseRouteGraph(
      "RTE2\nnode 1 0 0 start\nnode 2 1 1 end\nedge 1 2 5.5 3\n");
  assert(graph);
  auto stats = rivetmesh::analyzeRouteGraph(graph.value());
  assert(stats);
  assert(stats.value().reachable_nodes == 2);
}

void testStencil() {
  auto stencil = rivetmesh::parseStencil(
      "STN2\ntile id=t0 rev=1\npatch id=p0 layer=a x=0 y=0 w=1 h=3 data=\"A\\x42~1:C\"\n");
  assert(stencil);
  assert(stencil.value().patches.size() == 1);
  assert(stencil.value().expanded_bytes == 3);
}

void testBundle() {
  const auto text = sampleBundle();
  auto bundle = rivetmesh::parseBundle(reinterpret_cast<const std::uint8_t*>(text.data()),
                                       text.size());
  assert(bundle);
  assert(bundle.value().sections.size() == 4);
  auto analysis = rivetmesh::analyzeBundleBytes(
      reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
  assert(analysis);
  assert(analysis.value().rule_nodes > 0);
  assert(analysis.value().table_rows == 2);
  assert(analysis.value().route_nodes == 2);
  assert(analysis.value().stencil_patches == 2);
  assert(analysis.value().stencil_bytes == 6);
  assert(analysis.value().profile_signature != 0);
}

void testProfile() {
  const auto text = sampleBundle();
  auto profile = rivetmesh::profileBytes(reinterpret_cast<const std::uint8_t*>(text.data()),
                                        text.size());
  assert(profile);
  assert(profile.value().sections.size() == 4);
  assert(profile.value().table.rows == 2);
  assert(profile.value().route.edges == 1);
  assert(profile.value().stencil.patches == 2);
  auto valid = rivetmesh::ensureProfileHasNoErrors(profile.value());
  assert(valid);
  assert(rivetmesh::formatProfile(profile.value()).find("profile_signature=") !=
         std::string::npos);
}

}  // namespace

int main() {
  testRules();
  testTable();
  testRoute();
  testStencil();
  testBundle();
  testProfile();
  return 0;
}
