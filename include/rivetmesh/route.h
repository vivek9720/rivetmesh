#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "rivetmesh/status.h"

namespace rivetmesh {

struct Node {
  std::uint64_t id = 0;
  double x = 0.0;
  double y = 0.0;
  std::string label;
};

struct Edge {
  std::uint64_t from = 0;
  std::uint64_t to = 0;
  double cost = 0.0;
  std::uint32_t flags = 0;
};

struct RouteGraph {
  std::map<std::uint64_t, Node> nodes;
  std::vector<Edge> edges;
};

struct RouteStats {
  std::size_t reachable_nodes = 0;
  std::size_t dangling_edges = 0;
  double total_cost = 0.0;
  std::uint32_t flag_union = 0;
};

Outcome<RouteGraph> parseRouteGraph(const std::string& text);
Outcome<RouteStats> analyzeRouteGraph(const RouteGraph& graph);
std::string routeSummary(const RouteGraph& graph, const RouteStats& stats);

}  // namespace rivetmesh
