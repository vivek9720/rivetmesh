#include "rivetmesh/reader.h"

#include <cstring>

namespace rivetmesh {

Reader::Reader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

Reader::Reader(const std::vector<std::uint8_t>& bytes)
    : data_(bytes.data()), size_(bytes.size()) {}

std::size_t Reader::remaining() const {
  return pos_ <= size_ ? size_ - pos_ : 0;
}

bool Reader::hasPrefix(const char* text, std::size_t length) const {
  return remaining() >= length && std::memcmp(data_ + pos_, text, length) == 0;
}

Outcome<void> Reader::need(std::size_t count) const {
  if (count > remaining()) {
    return Outcome<void>::err(fail(Errc::eof, "not enough bytes", pos_));
  }
  return Outcome<void>::success();
}

Outcome<std::uint8_t> Reader::u8() {
  auto ok = need(1);
  if (!ok) {
    return Outcome<std::uint8_t>::err(ok.status());
  }
  return Outcome<std::uint8_t>::ok(data_[pos_++]);
}

Outcome<std::uint16_t> Reader::le16() {
  auto ok = need(2);
  if (!ok) {
    return Outcome<std::uint16_t>::err(ok.status());
  }
  std::uint16_t value = static_cast<std::uint16_t>(data_[pos_]) |
                        (static_cast<std::uint16_t>(data_[pos_ + 1]) << 8);
  pos_ += 2;
  return Outcome<std::uint16_t>::ok(value);
}

Outcome<std::uint32_t> Reader::le32() {
  auto ok = need(4);
  if (!ok) {
    return Outcome<std::uint32_t>::err(ok.status());
  }
  std::uint32_t value = static_cast<std::uint32_t>(data_[pos_]) |
                        (static_cast<std::uint32_t>(data_[pos_ + 1]) << 8) |
                        (static_cast<std::uint32_t>(data_[pos_ + 2]) << 16) |
                        (static_cast<std::uint32_t>(data_[pos_ + 3]) << 24);
  pos_ += 4;
  return Outcome<std::uint32_t>::ok(value);
}

Outcome<std::uint64_t> Reader::le64() {
  auto ok = need(8);
  if (!ok) {
    return Outcome<std::uint64_t>::err(ok.status());
  }
  std::uint64_t value = 0;
  for (int i = 0; i < 8; ++i) {
    value |= static_cast<std::uint64_t>(data_[pos_ + i]) << (i * 8);
  }
  pos_ += 8;
  return Outcome<std::uint64_t>::ok(value);
}

Outcome<std::uint64_t> Reader::varint(std::uint64_t max_value) {
  std::uint64_t value = 0;
  unsigned shift = 0;
  const std::size_t start = pos_;

  for (int i = 0; i < 10; ++i) {
    auto b = u8();
    if (!b) {
      return Outcome<std::uint64_t>::err(b.status());
    }
    if (shift >= 64 && (b.value() & 0x7f) != 0) {
      return Outcome<std::uint64_t>::err(fail(Errc::bad_number, "varint overflow", start));
    }
    value |= static_cast<std::uint64_t>(b.value() & 0x7f) << shift;
    if ((b.value() & 0x80) == 0) {
      if (value > max_value) {
        return Outcome<std::uint64_t>::err(fail(Errc::limit, "varint exceeds limit", start));
      }
      return Outcome<std::uint64_t>::ok(value);
    }
    shift += 7;
  }

  return Outcome<std::uint64_t>::err(fail(Errc::bad_number, "varint too long", start));
}

Outcome<std::vector<std::uint8_t>> Reader::bytes(std::size_t count) {
  auto ok = need(count);
  if (!ok) {
    return Outcome<std::vector<std::uint8_t>>::err(ok.status());
  }
  std::vector<std::uint8_t> out(data_ + pos_, data_ + pos_ + count);
  pos_ += count;
  return Outcome<std::vector<std::uint8_t>>::ok(std::move(out));
}

Outcome<std::string> Reader::string(std::size_t count) {
  auto b = bytes(count);
  if (!b) {
    return Outcome<std::string>::err(b.status());
  }
  return Outcome<std::string>::ok(toString(b.value()));
}

Outcome<void> Reader::skip(std::size_t count) {
  auto ok = need(count);
  if (!ok) {
    return ok;
  }
  pos_ += count;
  return Outcome<void>::success();
}

std::vector<std::uint8_t> toBytes(const std::string& text) {
  return std::vector<std::uint8_t>(text.begin(), text.end());
}

std::string toString(const std::vector<std::uint8_t>& bytes) {
  return std::string(bytes.begin(), bytes.end());
}

}  // namespace rivetmesh
