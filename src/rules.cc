#include "rivetmesh/rules.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace rivetmesh {
namespace {

enum class Tok {
  end,
  ident,
  number,
  string,
  lbrace,
  rbrace,
  lparen,
  rparen,
  comma,
  eq,
  semi,
  dot,
  plus,
  minus,
  star,
  slash,
};

struct Token {
  Tok kind = Tok::end;
  std::string text;
  std::size_t offset = 0;
};

class Lexer {
 public:
  explicit Lexer(std::string_view text) : text_(text) {}

  Outcome<Token> next() {
    skip();
    if (pos_ >= text_.size()) {
      return Outcome<Token>::ok(Token{Tok::end, "", pos_});
    }
    const std::size_t start = pos_;
    const char ch = text_[pos_++];
    switch (ch) {
      case '{':
        return Outcome<Token>::ok(Token{Tok::lbrace, "{", start});
      case '}':
        return Outcome<Token>::ok(Token{Tok::rbrace, "}", start});
      case '(':
        return Outcome<Token>::ok(Token{Tok::lparen, "(", start});
      case ')':
        return Outcome<Token>::ok(Token{Tok::rparen, ")", start});
      case ',':
        return Outcome<Token>::ok(Token{Tok::comma, ",", start});
      case '=':
        return Outcome<Token>::ok(Token{Tok::eq, "=", start});
      case ';':
        return Outcome<Token>::ok(Token{Tok::semi, ";", start});
      case '.':
        return Outcome<Token>::ok(Token{Tok::dot, ".", start});
      case '+':
        return Outcome<Token>::ok(Token{Tok::plus, "+", start});
      case '-':
        if (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
          --pos_;
          return number();
        }
        return Outcome<Token>::ok(Token{Tok::minus, "-", start});
      case '*':
        return Outcome<Token>::ok(Token{Tok::star, "*", start});
      case '/':
        return Outcome<Token>::ok(Token{Tok::slash, "/", start});
      case '"':
        return quoted(start);
      default:
        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
          --pos_;
          return ident();
        }
        if (std::isdigit(static_cast<unsigned char>(ch))) {
          --pos_;
          return number();
        }
        return Outcome<Token>::err(fail(Errc::syntax, "unexpected rule character", start));
    }
  }

 private:
  void skip() {
    while (pos_ < text_.size()) {
      char ch = text_[pos_];
      if (std::isspace(static_cast<unsigned char>(ch))) {
        ++pos_;
        continue;
      }
      if (ch == '#') {
        while (pos_ < text_.size() && text_[pos_] != '\n') {
          ++pos_;
        }
        continue;
      }
      if (ch == '/' && pos_ + 1 < text_.size() && text_[pos_ + 1] == '/') {
        pos_ += 2;
        while (pos_ < text_.size() && text_[pos_] != '\n') {
          ++pos_;
        }
        continue;
      }
      break;
    }
  }

  Outcome<Token> ident() {
    const std::size_t start = pos_;
    while (pos_ < text_.size()) {
      char ch = text_[pos_];
      if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_' && ch != '-') {
        break;
      }
      ++pos_;
    }
    return Outcome<Token>::ok(Token{Tok::ident, std::string(text_.substr(start, pos_ - start)),
                                   start});
  }

  Outcome<Token> number() {
    const std::size_t start = pos_;
    if (text_[pos_] == '-') {
      ++pos_;
    }
    while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
      ++pos_;
    }
    if (pos_ < text_.size() && text_[pos_] == '.') {
      ++pos_;
      while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
        ++pos_;
      }
    }
    return Outcome<Token>::ok(Token{Tok::number, std::string(text_.substr(start, pos_ - start)),
                                   start});
  }

  Outcome<Token> quoted(std::size_t start) {
    std::string value;
    while (pos_ < text_.size()) {
      const char ch = text_[pos_++];
      if (ch == '"') {
        return Outcome<Token>::ok(Token{Tok::string, value, start});
      }
      if (ch == '\\') {
        if (pos_ >= text_.size()) {
          return Outcome<Token>::err(fail(Errc::eof, "unterminated rule escape", start));
        }
        const char esc = text_[pos_++];
        switch (esc) {
          case 'n':
            value.push_back('\n');
            break;
          case 't':
            value.push_back('\t');
            break;
          case 'r':
            value.push_back('\r');
            break;
          default:
            value.push_back(esc);
            break;
        }
      } else {
        value.push_back(ch);
      }
    }
    return Outcome<Token>::err(fail(Errc::eof, "unterminated rule string", start));
  }

  std::string_view text_;
  std::size_t pos_ = 0;
};

class Parser {
 public:
  explicit Parser(std::string_view text) : lexer_(text) {}

  Outcome<RuleSet> parse() {
    auto first = lexer_.next();
    if (!first) {
      return Outcome<RuleSet>::err(first.status());
    }
    cur_ = first.value();
    RuleSet set;
    while (cur_.kind != Tok::end) {
      if (cur_.kind != Tok::ident) {
        return Outcome<RuleSet>::err(fail(Errc::syntax, "expected rule statement", cur_.offset));
      }
      if (peekBlockStart()) {
        auto block = parseBlock();
        if (!block) {
          return Outcome<RuleSet>::err(block.status());
        }
        set.blocks.push_back(block.take());
      } else {
        auto binding = parseBinding();
        if (!binding) {
          return Outcome<RuleSet>::err(binding.status());
        }
        set.globals.push_back(binding.take());
      }
    }
    return Outcome<RuleSet>::ok(std::move(set));
  }

 private:
  bool peekBlockStart() const { return cur_.kind == Tok::ident && cur_.text != "let"; }

  Outcome<RuleBlock> parseBlock() {
    RuleBlock block;
    block.name = cur_.text;
    auto adv = advance();
    if (!adv) {
      return Outcome<RuleBlock>::err(adv.status());
    }
    auto open = expect(Tok::lbrace, "rule block expects '{'");
    if (!open) {
      return Outcome<RuleBlock>::err(open.status());
    }
    while (cur_.kind != Tok::rbrace) {
      if (cur_.kind == Tok::end) {
        return Outcome<RuleBlock>::err(fail(Errc::eof, "unterminated rule block", 0));
      }
      if (cur_.kind != Tok::ident) {
        return Outcome<RuleBlock>::err(fail(Errc::syntax, "expected block item", cur_.offset));
      }
      if (peekBlockStart() && cur_.text != "let") {
        auto child = parseBlock();
        if (!child) {
          return Outcome<RuleBlock>::err(child.status());
        }
        block.children.push_back(child.take());
      } else {
        auto binding = parseBinding();
        if (!binding) {
          return Outcome<RuleBlock>::err(binding.status());
        }
        block.bindings.push_back(binding.take());
      }
    }
    auto close = advance();
    if (!close) {
      return Outcome<RuleBlock>::err(close.status());
    }
    return Outcome<RuleBlock>::ok(std::move(block));
  }

  Outcome<RuleBinding> parseBinding() {
    if (cur_.text == "let") {
      auto adv = advance();
      if (!adv) {
        return Outcome<RuleBinding>::err(adv.status());
      }
    }
    if (cur_.kind != Tok::ident) {
      return Outcome<RuleBinding>::err(fail(Errc::syntax, "binding expects name", cur_.offset));
    }
    RuleBinding binding;
    binding.name = cur_.text;
    auto name = advance();
    if (!name) {
      return Outcome<RuleBinding>::err(name.status());
    }
    auto eq = expect(Tok::eq, "binding expects '='");
    if (!eq) {
      return Outcome<RuleBinding>::err(eq.status());
    }
    auto expr = expression();
    if (!expr) {
      return Outcome<RuleBinding>::err(expr.status());
    }
    binding.expression = expr.take();
    auto semi = expect(Tok::semi, "binding expects ';'");
    if (!semi) {
      return Outcome<RuleBinding>::err(semi.status());
    }
    return Outcome<RuleBinding>::ok(std::move(binding));
  }

  Outcome<RuleExpr> expression() { return additive(); }

  Outcome<RuleExpr> additive() {
    auto left = multiplicative();
    if (!left) {
      return left;
    }
    while (cur_.kind == Tok::plus || cur_.kind == Tok::minus) {
      const char op = cur_.kind == Tok::plus ? '+' : '-';
      auto adv = advance();
      if (!adv) {
        return Outcome<RuleExpr>::err(adv.status());
      }
      auto right = multiplicative();
      if (!right) {
        return right;
      }
      RuleExpr expr;
      expr.kind = RuleExprKind::binary;
      expr.op = op;
      expr.left.reset(new RuleExpr(left.take()));
      expr.right.reset(new RuleExpr(right.take()));
      expr.text = ruleExprToString(*expr.left) + op + ruleExprToString(*expr.right);
      left = Outcome<RuleExpr>::ok(std::move(expr));
    }
    return left;
  }

  Outcome<RuleExpr> multiplicative() {
    auto left = primary();
    if (!left) {
      return left;
    }
    while (cur_.kind == Tok::star || cur_.kind == Tok::slash) {
      const char op = cur_.kind == Tok::star ? '*' : '/';
      auto adv = advance();
      if (!adv) {
        return Outcome<RuleExpr>::err(adv.status());
      }
      auto right = primary();
      if (!right) {
        return right;
      }
      RuleExpr expr;
      expr.kind = RuleExprKind::binary;
      expr.op = op;
      expr.left.reset(new RuleExpr(left.take()));
      expr.right.reset(new RuleExpr(right.take()));
      expr.text = ruleExprToString(*expr.left) + op + ruleExprToString(*expr.right);
      left = Outcome<RuleExpr>::ok(std::move(expr));
    }
    return left;
  }

  Outcome<RuleExpr> primary() {
    if (cur_.kind == Tok::number) {
      RuleExpr expr;
      expr.kind = RuleExprKind::number;
      expr.text = cur_.text;
      expr.number = std::strtod(cur_.text.c_str(), nullptr);
      auto adv = advance();
      if (!adv) {
        return Outcome<RuleExpr>::err(adv.status());
      }
      return Outcome<RuleExpr>::ok(std::move(expr));
    }
    if (cur_.kind == Tok::string) {
      RuleExpr expr;
      expr.kind = RuleExprKind::string;
      expr.text = cur_.text;
      auto adv = advance();
      if (!adv) {
        return Outcome<RuleExpr>::err(adv.status());
      }
      return Outcome<RuleExpr>::ok(std::move(expr));
    }
    if (cur_.kind == Tok::ident) {
      RuleExpr expr;
      expr.kind = RuleExprKind::identifier;
      expr.text = cur_.text;
      auto adv = advance();
      if (!adv) {
        return Outcome<RuleExpr>::err(adv.status());
      }
      if (cur_.kind == Tok::lparen) {
        expr.kind = RuleExprKind::call;
        auto open = advance();
        if (!open) {
          return Outcome<RuleExpr>::err(open.status());
        }
        while (cur_.kind != Tok::rparen) {
          auto arg = expression();
          if (!arg) {
            return Outcome<RuleExpr>::err(arg.status());
          }
          expr.args.push_back(arg.take());
          if (cur_.kind == Tok::comma) {
            auto comma = advance();
            if (!comma) {
              return Outcome<RuleExpr>::err(comma.status());
            }
            continue;
          }
          break;
        }
        auto close = expect(Tok::rparen, "call expects ')'");
        if (!close) {
          return Outcome<RuleExpr>::err(close.status());
        }
      }
      return Outcome<RuleExpr>::ok(std::move(expr));
    }
    if (cur_.kind == Tok::lparen) {
      auto open = advance();
      if (!open) {
        return Outcome<RuleExpr>::err(open.status());
      }
      auto expr = expression();
      if (!expr) {
        return expr;
      }
      auto close = expect(Tok::rparen, "expression expects ')'");
      if (!close) {
        return Outcome<RuleExpr>::err(close.status());
      }
      return expr;
    }
    return Outcome<RuleExpr>::err(fail(Errc::syntax, "expected expression", cur_.offset));
  }

  Outcome<void> expect(Tok kind, const char* message) {
    if (cur_.kind != kind) {
      return Outcome<void>::err(fail(Errc::syntax, message, cur_.offset));
    }
    return advance();
  }

  Outcome<void> advance() {
    auto next = lexer_.next();
    if (!next) {
      return Outcome<void>::err(next.status());
    }
    cur_ = next.value();
    return Outcome<void>::success();
  }

  Lexer lexer_;
  Token cur_;
};

std::size_t countExpr(const RuleExpr& expr) {
  std::size_t n = 1;
  if (expr.left) {
    n += countExpr(*expr.left);
  }
  if (expr.right) {
    n += countExpr(*expr.right);
  }
  for (const auto& arg : expr.args) {
    n += countExpr(arg);
  }
  return n;
}

std::size_t countBlock(const RuleBlock& block) {
  std::size_t n = 1;
  for (const auto& binding : block.bindings) {
    n += 1 + countExpr(binding.expression);
  }
  for (const auto& child : block.children) {
    n += countBlock(child);
  }
  return n;
}

}  // namespace

RuleExpr::RuleExpr(const RuleExpr& other)
    : kind(other.kind), text(other.text), number(other.number), op(other.op),
      args(other.args) {
  if (other.left) {
    left.reset(new RuleExpr(*other.left));
  }
  if (other.right) {
    right.reset(new RuleExpr(*other.right));
  }
}

RuleExpr& RuleExpr::operator=(const RuleExpr& other) {
  if (this == &other) {
    return *this;
  }
  kind = other.kind;
  text = other.text;
  number = other.number;
  op = other.op;
  args = other.args;
  left.reset(other.left ? new RuleExpr(*other.left) : nullptr);
  right.reset(other.right ? new RuleExpr(*other.right) : nullptr);
  return *this;
}

Outcome<RuleSet> parseRules(std::string_view text) {
  Parser parser(text);
  return parser.parse();
}

std::string ruleExprToString(const RuleExpr& expr) {
  switch (expr.kind) {
    case RuleExprKind::number:
      return expr.text;
    case RuleExprKind::string:
      return "\"" + expr.text + "\"";
    case RuleExprKind::identifier:
      return expr.text;
    case RuleExprKind::binary:
      if (expr.left && expr.right) {
        return "(" + ruleExprToString(*expr.left) + expr.op + ruleExprToString(*expr.right) + ")";
      }
      return expr.text;
    case RuleExprKind::call: {
      std::string out = expr.text + "(";
      for (std::size_t i = 0; i < expr.args.size(); ++i) {
        if (i != 0) {
          out += ",";
        }
        out += ruleExprToString(expr.args[i]);
      }
      out += ")";
      return out;
    }
  }
  return expr.text;
}

std::size_t countRuleNodes(const RuleSet& rules) {
  std::size_t n = 0;
  for (const auto& binding : rules.globals) {
    n += 1 + countExpr(binding.expression);
  }
  for (const auto& block : rules.blocks) {
    n += countBlock(block);
  }
  return n;
}

}  // namespace rivetmesh
