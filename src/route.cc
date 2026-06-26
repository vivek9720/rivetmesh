#include "rivetmesh/route.h"

#include <algorithm>
#include <cctype>
#include <queue>
#include <sstream>
#include <set>

namespace rivetmesh {
namespace {

std::string trim(std::string text) {
  auto space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(),
                                        [&](char ch) { return !space(ch); }));
  text.erase(std::find_if(text.rbegin(), text.rend(),
                          [&](char ch) { return !space(ch); })
                 .base(),
             text.end());
  return text;
}

std::vector<std::string> split(const std::string& line) {
  std::istringstream in(line);
  std::vector<std::string> out;
  std::string word;
  while (in >> word) {
    out.push_back(word);
  }
  return out;
}

Outcome<std::uint64_t> parseU64(const std::string& text, std::size_t line) {
  try {
    std::size_t consumed = 0;
    unsigned long long value = std::stoull(text, &consumed, 0);
    if (consumed != text.size()) {
      return Outcome<std::uint64_t>::err(fail(Errc::bad_number, "bad route integer", line));
    }
    return Outcome<std::uint64_t>::ok(static_cast<std::uint64_t>(value));
  } catch (...) {
    return Outcome<std::uint64_t>::err(fail(Errc::bad_number, "bad route integer", line));
  }
}

Outcome<double> parseDouble(const std::string& text, std::size_t line) {
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0') {
    return Outcome<double>::err(fail(Errc::bad_number, "bad route decimal", line));
  }
  return Outcome<double>::ok(value);
}

Outcome<void> parseNode(RouteGraph& graph, const std::vector<std::string>& tokens,
                        std::size_t line) {
  if (tokens.size() < 4 || tokens.size() > 5) {
    return Outcome<void>::err(fail(Errc::syntax, "node expects id x y [label]", line));
  }
  auto id = parseU64(tokens[1], line);
  auto x = parseDouble(tokens[2], line);
  auto y = parseDouble(tokens[3], line);
  if (!id) {
    return Outcome<void>::err(id.status());
  }
  if (!x) {
    return Outcome<void>::err(x.status());
  }
  if (!y) {
    return Outcome<void>::err(y.status());
  }
  Node node;
  node.id = id.value();
  node.x = x.value();
  node.y = y.value();
  if (tokens.size() == 5) {
    node.label = tokens[4];
  }
  graph.nodes[node.id] = std::move(node);
  return Outcome<void>::success();
}

Outcome<void> parseEdge(RouteGraph& graph, const std::vector<std::string>& tokens,
                        std::size_t line) {
  if (tokens.size() < 4 || tokens.size() > 5) {
    return Outcome<void>::err(fail(Errc::syntax, "edge expects from to cost [flags]", line));
  }
  auto from = parseU64(tokens[1], line);
  auto to = parseU64(tokens[2], line);
  auto cost = parseDouble(tokens[3], line);
  if (!from) {
    return Outcome<void>::err(from.status());
  }
  if (!to) {
    return Outcome<void>::err(to.status());
  }
  if (!cost) {
    return Outcome<void>::err(cost.status());
  }
  Edge edge;
  edge.from = from.value();
  edge.to = to.value();
  edge.cost = cost.value();
  if (tokens.size() == 5) {
    auto flags = parseU64(tokens[4], line);
    if (!flags) {
      return Outcome<void>::err(flags.status());
    }
    edge.flags = static_cast<std::uint32_t>(flags.value());
  }
  graph.edges.push_back(edge);
  return Outcome<void>::success();
}

}  // namespace

Outcome<RouteGraph> parseRouteGraph(const std::string& text) {
  std::istringstream input(text);
  std::string line;
  std::size_t line_number = 0;
  bool saw_header = false;
  RouteGraph graph;

  while (std::getline(input, line)) {
    ++line_number;
    line = trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    auto tokens = split(line);
    if (!saw_header) {
      if (tokens.size() != 1 || tokens[0] != "RTE2") {
        return Outcome<RouteGraph>::err(fail(Errc::bad_magic, "route graph missing RTE2",
                                             line_number));
      }
      saw_header = true;
      continue;
    }
    Outcome<void> parsed =
        tokens[0] == "node" ? parseNode(graph, tokens, line_number)
                            : tokens[0] == "edge"
                                  ? parseEdge(graph, tokens, line_number)
                                  : Outcome<void>::err(fail(Errc::syntax, "unknown route record",
                                                            line_number));
    if (!parsed) {
      return Outcome<RouteGraph>::err(parsed.status());
    }
    if (graph.nodes.size() > 8192 || graph.edges.size() > 32768) {
      return Outcome<RouteGraph>::err(fail(Errc::limit, "route graph too large", line_number));
    }
  }
  if (!saw_header) {
    return Outcome<RouteGraph>::err(fail(Errc::bad_magic, "empty route graph", 0));
  }
  return Outcome<RouteGraph>::ok(std::move(graph));
}

Outcome<RouteStats> analyzeRouteGraph(const RouteGraph& graph) {
  RouteStats stats;
  stats.dangling_edges = 0;
  for (const auto& edge : graph.edges) {
    stats.total_cost += edge.cost;
    stats.flag_union |= edge.flags;
    if (graph.nodes.count(edge.from) == 0 || graph.nodes.count(edge.to) == 0) {
      ++stats.dangling_edges;
    }
  }

  if (!graph.nodes.empty()) {
    std::set<std::uint64_t> visited;
    std::queue<std::uint64_t> queue;
    queue.push(graph.nodes.begin()->first);
    visited.insert(graph.nodes.begin()->first);
    while (!queue.empty()) {
      const auto id = queue.front();
      queue.pop();
      for (const auto& edge : graph.edges) {
        if (edge.from == id && graph.nodes.count(edge.to) != 0 && visited.insert(edge.to).second) {
          queue.push(edge.to);
        }
      }
    }
    stats.reachable_nodes = visited.size();
  }
  return Outcome<RouteStats>::ok(stats);
}

std::string routeSummary(const RouteGraph& graph, const RouteStats& stats) {
  std::ostringstream out;
  out << "nodes=" << graph.nodes.size() << " edges=" << graph.edges.size()
      << " reachable=" << stats.reachable_nodes << " dangling=" << stats.dangling_edges
      << " total_cost=" << stats.total_cost << " flags=" << stats.flag_union;
  return out.str();
}

}  // namespace rivetmesh
