#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include "rivetmesh/analyzer.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: rm_inspect <bundle>\n";
    return 2;
  }
  std::ifstream input(argv[1], std::ios::binary);
  if (!input) {
    std::cerr << "failed to open input\n";
    return 2;
  }
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
  auto analysis = rivetmesh::analyzeBundleBytes(bytes.data(), bytes.size());
  if (!analysis) {
    std::cerr << rivetmesh::errcName(analysis.status().code) << ": "
              << analysis.status().message << " at " << analysis.status().offset << "\n";
    return 1;
  }
  std::cout << rivetmesh::formatAnalysis(analysis.value());
  return 0;
}
