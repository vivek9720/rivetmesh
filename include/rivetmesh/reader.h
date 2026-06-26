#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "rivetmesh/status.h"

namespace rivetmesh {

class Reader {
 public:
  Reader(const std::uint8_t* data, std::size_t size);
  explicit Reader(const std::vector<std::uint8_t>& bytes);

  std::size_t offset() const { return pos_; }
  std::size_t remaining() const;
  bool empty() const { return remaining() == 0; }
  bool hasPrefix(const char* text, std::size_t length) const;

  Outcome<std::uint8_t> u8();
  Outcome<std::uint16_t> le16();
  Outcome<std::uint32_t> le32();
  Outcome<std::uint64_t> le64();
  Outcome<std::uint64_t> varint(std::uint64_t max_value);
  Outcome<std::vector<std::uint8_t>> bytes(std::size_t count);
  Outcome<std::string> string(std::size_t count);
  Outcome<void> skip(std::size_t count);

 private:
  Outcome<void> need(std::size_t count) const;

  const std::uint8_t* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t pos_ = 0;
};

std::vector<std::uint8_t> toBytes(const std::string& text);
std::string toString(const std::vector<std::uint8_t>& bytes);

}  // namespace rivetmesh
