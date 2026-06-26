#include "rivetmesh/stencil.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>

#include "rivetmesh/checksum.h"

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

int hexNibble(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return 10 + ch - 'a';
  }
  if (ch >= 'A' && ch <= 'F') {
    return 10 + ch - 'A';
  }
  return -1;
}

Outcome<std::uint64_t> parseUnsigned(const std::string& text, std::size_t line) {
  try {
    std::size_t consumed = 0;
    unsigned long long value = std::stoull(text, &consumed, 0);
    if (consumed != text.size()) {
      return Outcome<std::uint64_t>::err(fail(Errc::bad_number, "bad stencil integer", line));
    }
    return Outcome<std::uint64_t>::ok(static_cast<std::uint64_t>(value));
  } catch (...) {
    return Outcome<std::uint64_t>::err(fail(Errc::bad_number, "bad stencil integer", line));
  }
}

Outcome<std::map<std::string, std::string>> attributes(const std::string& rest,
                                                       std::size_t line) {
  std::map<std::string, std::string> attrs;
  std::size_t i = 0;
  while (i < rest.size()) {
    while (i < rest.size() && std::isspace(static_cast<unsigned char>(rest[i]))) {
      ++i;
    }
    if (i >= rest.size()) {
      break;
    }
    const std::size_t key_start = i;
    while (i < rest.size() &&
           (std::isalnum(static_cast<unsigned char>(rest[i])) || rest[i] == '_' ||
            rest[i] == '-')) {
      ++i;
    }
    if (key_start == i || i >= rest.size() || rest[i] != '=') {
      return Outcome<std::map<std::string, std::string>>::err(
          fail(Errc::syntax, "attribute expects key=value", line));
    }
    std::string key = rest.substr(key_start, i - key_start);
    ++i;
    std::string value;
    if (i < rest.size() && rest[i] == '"') {
      ++i;
      bool closed = false;
      while (i < rest.size()) {
        char ch = rest[i++];
        if (ch == '"') {
          closed = true;
          break;
        }
        if (ch == '\\' && i < rest.size()) {
          value.push_back(ch);
          value.push_back(rest[i++]);
        } else {
          value.push_back(ch);
        }
      }
      if (!closed) {
        return Outcome<std::map<std::string, std::string>>::err(
            fail(Errc::eof, "unterminated attribute", line));
      }
    } else {
      const std::size_t value_start = i;
      while (i < rest.size() && !std::isspace(static_cast<unsigned char>(rest[i]))) {
        ++i;
      }
      value = rest.substr(value_start, i - value_start);
    }
    attrs[std::move(key)] = std::move(value);
  }
  return Outcome<std::map<std::string, std::string>>::ok(std::move(attrs));
}

std::size_t plannedSize(const std::string& encoded) {
  std::size_t size = 0;
  for (std::size_t i = 0; i < encoded.size(); ++i) {
    if (encoded[i] != '\\' && encoded[i] != '~') {
      ++size;
      continue;
    }
    if (encoded[i] == '\\') {
      if (i + 1 >= encoded.size()) {
        ++size;
        continue;
      }
      const char kind = encoded[++i];
      if (kind == 'x' && i + 2 < encoded.size()) {
        i += 2;
      }
      ++size;
      continue;
    }
    if (encoded[i] == '~') {
      while (i + 1 < encoded.size() &&
             std::isdigit(static_cast<unsigned char>(encoded[i + 1]))) {
        ++i;
      }
      if (i + 1 < encoded.size() && encoded[i + 1] == ':') {
        ++i;
        if (i + 1 < encoded.size()) {
          ++i;
        }
      }
      ++size;
    }
  }
  return size;
}

Outcome<std::vector<std::uint8_t>> expandPatchPayload(const std::string& encoded,
                                                      std::size_t line) {
  const std::size_t estimate = plannedSize(encoded);
  std::vector<std::uint8_t> out(estimate + 1);
  std::size_t used = 0;

  for (std::size_t i = 0; i < encoded.size(); ++i) {
    char ch = encoded[i];
    if (ch == '\\') {
      if (i + 1 >= encoded.size()) {
        out[used++] = static_cast<std::uint8_t>('\\');
        continue;
      }
      const char kind = encoded[++i];
      if (kind == 'n') {
        out[used++] = '\n';
      } else if (kind == 't') {
        out[used++] = '\t';
      } else if (kind == 'x') {
        if (i + 2 >= encoded.size()) {
          return Outcome<std::vector<std::uint8_t>>::err(
              fail(Errc::syntax, "short hex escape", line));
        }
        const int hi = hexNibble(encoded[i + 1]);
        const int lo = hexNibble(encoded[i + 2]);
        if (hi < 0 || lo < 0) {
          return Outcome<std::vector<std::uint8_t>>::err(
              fail(Errc::syntax, "bad hex escape", line));
        }
        out[used++] = static_cast<std::uint8_t>((hi << 4) | lo);
        i += 2;
      } else {
        out[used++] = static_cast<std::uint8_t>(kind);
      }
      continue;
    }

    if (ch == '~') {
      const std::size_t start = i + 1;
      while (i + 1 < encoded.size() &&
             std::isdigit(static_cast<unsigned char>(encoded[i + 1]))) {
        ++i;
      }
      if (start == i + 1 || i + 1 >= encoded.size() || encoded[i + 1] != ':') {
        out[used++] = '~';
        continue;
      }
      auto repeat = parseUnsigned(encoded.substr(start, i - start + 1), line);
      if (!repeat) {
        return Outcome<std::vector<std::uint8_t>>::err(repeat.status());
      }
      if (repeat.value() > 65536) {
        return Outcome<std::vector<std::uint8_t>>::err(
            fail(Errc::limit, "repeat too large", line));
      }
      i += 2;
      if (i >= encoded.size()) {
        return Outcome<std::vector<std::uint8_t>>::err(
            fail(Errc::eof, "repeat missing value", line));
      }
      const std::uint8_t value = static_cast<std::uint8_t>(encoded[i]);
      for (std::uint64_t n = 0; n < repeat.value(); ++n) {
        out[used++] = value;
      }
      continue;
    }

    out[used++] = static_cast<std::uint8_t>(ch);
  }

  out.resize(used);
  return Outcome<std::vector<std::uint8_t>>::ok(std::move(out));
}

Outcome<void> parseTile(Stencil& stencil, const std::map<std::string, std::string>& attrs,
                        std::size_t line) {
  auto id = attrs.find("id");
  if (id == attrs.end() || id->second.empty()) {
    return Outcome<void>::err(fail(Errc::syntax, "tile missing id", line));
  }
  stencil.tile_id = id->second;
  auto revision = attrs.find("rev");
  if (revision != attrs.end()) {
    auto parsed = parseUnsigned(revision->second, line);
    if (!parsed) {
      return Outcome<void>::err(parsed.status());
    }
    stencil.revision = static_cast<std::uint32_t>(parsed.value());
  }
  return Outcome<void>::success();
}

Outcome<Patch> parsePatch(const std::map<std::string, std::string>& attrs, std::size_t line) {
  Patch patch;
  auto id = attrs.find("id");
  auto data = attrs.find("data");
  if (id == attrs.end() || id->second.empty()) {
    return Outcome<Patch>::err(fail(Errc::syntax, "patch missing id", line));
  }
  if (data == attrs.end()) {
    return Outcome<Patch>::err(fail(Errc::syntax, "patch missing data", line));
  }
  patch.id = id->second;
  auto layer = attrs.find("layer");
  patch.layer = layer == attrs.end() ? "base" : layer->second;

  auto read_dim = [&](const char* name) -> Outcome<std::uint32_t> {
    auto it = attrs.find(name);
    if (it == attrs.end()) {
      return Outcome<std::uint32_t>::ok(0);
    }
    auto parsed = parseUnsigned(it->second, line);
    if (!parsed) {
      return Outcome<std::uint32_t>::err(parsed.status());
    }
    return Outcome<std::uint32_t>::ok(static_cast<std::uint32_t>(parsed.value()));
  };

  auto x = read_dim("x");
  auto y = read_dim("y");
  auto w = read_dim("w");
  auto h = read_dim("h");
  if (!x) {
    return Outcome<Patch>::err(x.status());
  }
  if (!y) {
    return Outcome<Patch>::err(y.status());
  }
  if (!w) {
    return Outcome<Patch>::err(w.status());
  }
  if (!h) {
    return Outcome<Patch>::err(h.status());
  }
  patch.x = x.value();
  patch.y = y.value();
  patch.width = w.value();
  patch.height = h.value();

  auto payload = expandPatchPayload(data->second, line);
  if (!payload) {
    return Outcome<Patch>::err(payload.status());
  }
  patch.payload = payload.take();
  patch.digest = meshDigest(patch.payload);
  return Outcome<Patch>::ok(std::move(patch));
}

}  // namespace

Outcome<Stencil> parseStencil(const std::string& text) {
  std::istringstream input(text);
  std::string line;
  std::size_t line_number = 0;
  bool saw_header = false;
  Stencil stencil;

  while (std::getline(input, line)) {
    ++line_number;
    line = trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    if (!saw_header) {
      if (line != "STN2") {
        return Outcome<Stencil>::err(fail(Errc::bad_magic, "stencil missing STN2", line_number));
      }
      saw_header = true;
      continue;
    }
    if (line.rfind("tile ", 0) == 0) {
      auto attrs = attributes(line.substr(5), line_number);
      if (!attrs) {
        return Outcome<Stencil>::err(attrs.status());
      }
      auto parsed = parseTile(stencil, attrs.value(), line_number);
      if (!parsed) {
        return Outcome<Stencil>::err(parsed.status());
      }
      continue;
    }
    if (line.rfind("patch ", 0) == 0) {
      auto attrs = attributes(line.substr(6), line_number);
      if (!attrs) {
        return Outcome<Stencil>::err(attrs.status());
      }
      auto patch = parsePatch(attrs.value(), line_number);
      if (!patch) {
        return Outcome<Stencil>::err(patch.status());
      }
      stencil.expanded_bytes += patch.value().payload.size();
      stencil.patches.push_back(patch.take());
      if (stencil.patches.size() > 4096) {
        return Outcome<Stencil>::err(fail(Errc::limit, "too many patches", line_number));
      }
      continue;
    }
    return Outcome<Stencil>::err(fail(Errc::syntax, "unknown stencil record", line_number));
  }

  if (!saw_header) {
    return Outcome<Stencil>::err(fail(Errc::bad_magic, "empty stencil", 0));
  }
  if (stencil.tile_id.empty()) {
    return Outcome<Stencil>::err(fail(Errc::state, "stencil missing tile", 0));
  }
  return Outcome<Stencil>::ok(std::move(stencil));
}

std::string stencilSummary(const Stencil& stencil) {
  std::ostringstream out;
  out << "tile=" << stencil.tile_id << " rev=" << stencil.revision
      << " patches=" << stencil.patches.size() << " bytes=" << stencil.expanded_bytes;
  for (const auto& patch : stencil.patches) {
    out << " [" << patch.id << ":" << patch.layer << ":" << patch.width << "x"
        << patch.height << ":" << patch.digest << "]";
  }
  return out.str();
}

}  // namespace rivetmesh
