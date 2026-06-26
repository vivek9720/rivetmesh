#include <cstddef>
#include <cstdint>

#include "rivetmesh/analyzer.h"
#include "rivetmesh/profile.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  auto analysis = rivetmesh::analyzeBundleBytes(data, size);
  if (analysis) {
    volatile std::size_t sink = analysis.value().sections + analysis.value().rule_nodes +
                                analysis.value().table_rows + analysis.value().route_edges +
                                analysis.value().stencil_bytes +
                                analysis.value().profile_findings;
    (void)sink;
  }
  auto profile = rivetmesh::profileBytes(data, size);
  if (profile) {
    auto valid = rivetmesh::ensureProfileHasNoErrors(profile.value());
    volatile std::size_t sink = profile.value().sections.size() +
                                profile.value().stencil.patches + (valid ? 1u : 0u);
    (void)sink;
  }
  return 0;
}
