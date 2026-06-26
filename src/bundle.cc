#include "rivetmesh/bundle.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "rivetmesh/checksum.h"
#include "rivetmesh/reader.h"

namespace rivetmesh {
namespace {

struct PendingHeader {
  std::string name;
  SectionKind kind = SectionKind::unknown;
  std::uint32_t flags = 0;
  std::uint32_t length = 0;
  std::uint32_t digest = 0;
};

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

std::vector<std::string> words(const std::string& line) {
  std::istringstream in(line);
  std::vector<std::string> result;
  std::string word;
  while (in >> word) {
    result.push_back(word);
  }
  return result;
}

Outcome<std::uint32_t> parseU32(const std::string& text, std::size_t offset) {
  try {
    std::size_t consumed = 0;
    unsigned long value = std::stoul(text, &consumed, 0);
    if (consumed != text.size() || value > 0xffffffffUL) {
      return Outcome<std::uint32_t>::err(fail(Errc::bad_number, "bad uint32", offset));
    }
    return Outcome<std::uint32_t>::ok(static_cast<std::uint32_t>(value));
  } catch (...) {
    return Outcome<std::uint32_t>::err(fail(Errc::bad_number, "bad uint32", offset));
  }
}

Outcome<void> readMeta(Bundle& bundle, const std::string& line, std::size_t line_number) {
  const std::size_t eq = line.find('=');
  if (eq == std::string::npos || eq == 0) {
    return Outcome<void>::err(fail(Errc::syntax, "metadata expects key=value", line_number));
  }
  bundle.metadata[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
  return Outcome<void>::success();
}

}  // namespace

const char* sectionKindName(SectionKind kind) {
  switch (kind) {
    case SectionKind::manifest:
      return "manifest";
    case SectionKind::rules:
      return "rules";
    case SectionKind::table:
      return "table";
    case SectionKind::route:
      return "route";
    case SectionKind::stencil:
      return "stencil";
    case SectionKind::note:
      return "note";
    case SectionKind::unknown:
      return "unknown";
  }
  return "unknown";
}

SectionKind sectionKindFromName(const std::string& text) {
  std::string lowered = text;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (lowered == "manifest" || lowered == "meta") {
    return SectionKind::manifest;
  }
  if (lowered == "rules" || lowered == "policy") {
    return SectionKind::rules;
  }
  if (lowered == "table" || lowered == "records") {
    return SectionKind::table;
  }
  if (lowered == "route" || lowered == "graph") {
    return SectionKind::route;
  }
  if (lowered == "stencil" || lowered == "patches") {
    return SectionKind::stencil;
  }
  if (lowered == "note" || lowered == "notes") {
    return SectionKind::note;
  }
  return SectionKind::unknown;
}

Outcome<Bundle> parseBundle(const std::uint8_t* data, std::size_t size,
                            BundleLimits limits) {
  if (data != nullptr && size >= 4 &&
      std::equal(data, data + 4, reinterpret_cast<const std::uint8_t*>("RMB1"))) {
    return parseBinaryBundle(data, size, limits);
  }
  return parseTextBundle(data, size, limits);
}

Outcome<Bundle> parseBinaryBundle(const std::uint8_t* data, std::size_t size,
                                  BundleLimits limits) {
  Reader reader(data, size);
  if (!reader.hasPrefix("RMB1", 4)) {
    return Outcome<Bundle>::err(fail(Errc::bad_magic, "binary bundle missing RMB1", 0));
  }
  auto skipped = reader.skip(4);
  if (!skipped) {
    return Outcome<Bundle>::err(skipped.status());
  }
  auto version = reader.le16();
  auto count = reader.le16();
  if (!version) {
    return Outcome<Bundle>::err(version.status());
  }
  if (!count) {
    return Outcome<Bundle>::err(count.status());
  }
  if (version.value() == 0 || version.value() > 2) {
    return Outcome<Bundle>::err(fail(Errc::bad_version, "unsupported bundle version", 4));
  }
  if (count.value() > limits.max_sections) {
    return Outcome<Bundle>::err(fail(Errc::limit, "too many sections", reader.offset()));
  }

  std::vector<PendingHeader> headers;
  for (std::size_t i = 0; i < count.value(); ++i) {
    auto name_len = reader.u8();
    if (!name_len) {
      return Outcome<Bundle>::err(name_len.status());
    }
    auto name = reader.string(name_len.value());
    auto kind = reader.u8();
    auto flags = reader.le16();
    auto length = reader.le32();
    auto digest = reader.le32();
    if (!name) {
      return Outcome<Bundle>::err(name.status());
    }
    if (!kind) {
      return Outcome<Bundle>::err(kind.status());
    }
    if (!flags) {
      return Outcome<Bundle>::err(flags.status());
    }
    if (!length) {
      return Outcome<Bundle>::err(length.status());
    }
    if (!digest) {
      return Outcome<Bundle>::err(digest.status());
    }
    if (length.value() > limits.max_payload) {
      return Outcome<Bundle>::err(fail(Errc::limit, "section payload too large",
                                       reader.offset()));
    }
    SectionKind mapped = SectionKind::unknown;
    switch (kind.value()) {
      case 1:
        mapped = SectionKind::manifest;
        break;
      case 2:
        mapped = SectionKind::rules;
        break;
      case 3:
        mapped = SectionKind::table;
        break;
      case 4:
        mapped = SectionKind::route;
        break;
      case 5:
        mapped = SectionKind::stencil;
        break;
      case 6:
        mapped = SectionKind::note;
        break;
      default:
        mapped = SectionKind::unknown;
        break;
    }
    headers.push_back(PendingHeader{name.take(), mapped, flags.value(), length.value(),
                                    digest.value()});
  }

  Bundle bundle;
  bundle.version = version.value();
  for (const auto& header : headers) {
    auto body = reader.bytes(header.length);
    if (!body) {
      return Outcome<Bundle>::err(body.status());
    }
    if (header.digest != 0 && meshDigest(body.value()) != header.digest) {
      return Outcome<Bundle>::err(fail(Errc::checksum, "section digest mismatch",
                                       reader.offset()));
    }
    Section section;
    section.name = header.name;
    section.kind = header.kind;
    section.flags = header.flags;
    section.digest = header.digest;
    section.body = body.take();
    bundle.sections.push_back(std::move(section));
  }
  return Outcome<Bundle>::ok(std::move(bundle));
}

Outcome<Bundle> parseTextBundle(const std::uint8_t* data, std::size_t size,
                                BundleLimits limits) {
  std::string text;
  if (data != nullptr && size != 0) {
    text.assign(reinterpret_cast<const char*>(data), size);
  }
  std::istringstream input(text);
  std::string line;
  std::size_t line_number = 0;
  Bundle bundle;

  if (!std::getline(input, line)) {
    return Outcome<Bundle>::err(fail(Errc::eof, "empty bundle", 0));
  }
  ++line_number;
  if (trim(line) != "RIVETMESH/1") {
    return Outcome<Bundle>::err(fail(Errc::bad_magic, "missing RIVETMESH/1 header", 0));
  }

  while (std::getline(input, line)) {
    ++line_number;
    if (line_number > limits.max_lines) {
      return Outcome<Bundle>::err(fail(Errc::limit, "too many bundle lines", line_number));
    }
    line = trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    if (line == "end") {
      break;
    }
    if (line.rfind("version ", 0) == 0) {
      auto parsed = parseU32(trim(line.substr(8)), line_number);
      if (!parsed) {
        return Outcome<Bundle>::err(parsed.status());
      }
      if (parsed.value() == 0 || parsed.value() > 2) {
        return Outcome<Bundle>::err(fail(Errc::bad_version, "unsupported text version",
                                         line_number));
      }
      bundle.version = static_cast<std::uint16_t>(parsed.value());
      continue;
    }
    if (line.rfind("meta ", 0) == 0) {
      auto meta = readMeta(bundle, trim(line.substr(5)), line_number);
      if (!meta) {
        return Outcome<Bundle>::err(meta.status());
      }
      continue;
    }
    if (line.rfind("section ", 0) == 0) {
      auto parts = words(line);
      if (parts.size() < 3 || parts.size() > 4) {
        return Outcome<Bundle>::err(fail(Errc::syntax, "section expects name kind [digest]",
                                         line_number));
      }
      if (bundle.sections.size() >= limits.max_sections) {
        return Outcome<Bundle>::err(fail(Errc::limit, "too many sections", line_number));
      }
      Section section;
      section.name = parts[1];
      section.kind = sectionKindFromName(parts[2]);
      if (parts.size() == 4) {
        auto digest = parseU32(parts[3], line_number);
        if (!digest) {
          return Outcome<Bundle>::err(digest.status());
        }
        section.digest = digest.value();
      }
      std::ostringstream body;
      bool closed = false;
      while (std::getline(input, line)) {
        ++line_number;
        if (trim(line) == ".") {
          closed = true;
          break;
        }
        body << line << '\n';
        if (static_cast<std::size_t>(body.tellp()) > limits.max_payload) {
          return Outcome<Bundle>::err(fail(Errc::limit, "section payload too large",
                                           line_number));
        }
      }
      if (!closed) {
        return Outcome<Bundle>::err(fail(Errc::eof, "unterminated section", line_number));
      }
      const std::string body_text = body.str();
      section.body = toBytes(body_text);
      if (section.digest != 0 && meshDigest(section.body) != section.digest) {
        return Outcome<Bundle>::err(fail(Errc::checksum, "section digest mismatch",
                                         line_number));
      }
      bundle.sections.push_back(std::move(section));
      continue;
    }
    return Outcome<Bundle>::err(fail(Errc::syntax, "unknown bundle directive", line_number));
  }
  return Outcome<Bundle>::ok(std::move(bundle));
}

Outcome<void> validateBundle(const Bundle& bundle) {
  if (bundle.version == 0 || bundle.version > 2) {
    return Outcome<void>::err(fail(Errc::bad_version, "invalid version", 0));
  }
  if (bundle.sections.empty()) {
    return Outcome<void>::err(fail(Errc::state, "bundle has no sections", 0));
  }
  for (const auto& section : bundle.sections) {
    if (section.name.empty()) {
      return Outcome<void>::err(fail(Errc::state, "empty section name", 0));
    }
    if (section.kind == SectionKind::unknown) {
      return Outcome<void>::err(fail(Errc::unknown_section, "unknown section kind", 0));
    }
  }
  return Outcome<void>::success();
}

}  // namespace rivetmesh
