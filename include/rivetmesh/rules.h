#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "rivetmesh/status.h"

namespace rivetmesh {

enum class RuleExprKind {
  number,
  string,
  identifier,
  binary,
  call,
};

struct RuleExpr {
  RuleExprKind kind = RuleExprKind::identifier;
  std::string text;
  double number = 0.0;
  char op = 0;
  std::unique_ptr<RuleExpr> left;
  std::unique_ptr<RuleExpr> right;
  std::vector<RuleExpr> args;

  RuleExpr() = default;
  RuleExpr(const RuleExpr& other);
  RuleExpr& operator=(const RuleExpr& other);
  RuleExpr(RuleExpr&&) noexcept = default;
  RuleExpr& operator=(RuleExpr&&) noexcept = default;
};

struct RuleBinding {
  std::string name;
  RuleExpr expression;
};

struct RuleBlock {
  std::string name;
  std::vector<RuleBinding> bindings;
  std::vector<RuleBlock> children;
};

struct RuleSet {
  std::vector<RuleBinding> globals;
  std::vector<RuleBlock> blocks;
};

Outcome<RuleSet> parseRules(std::string_view text);
std::string ruleExprToString(const RuleExpr& expr);
std::size_t countRuleNodes(const RuleSet& rules);

}  // namespace rivetmesh
