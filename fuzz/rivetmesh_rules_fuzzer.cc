#include <cstddef>
#include <cstdint>
#include <string>

#include "rivetmesh/rules.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size > 131072) {
    return 0;
  }
  std::string text;
  if (data != nullptr && size != 0) {
    text.assign(reinterpret_cast<const char*>(data), size);
  }
  auto rules = rivetmesh::parseRules(text);
  if (rules) {
    volatile std::size_t sink = rivetmesh::countRuleNodes(rules.value());
    (void)sink;
  }
  return 0;
}
