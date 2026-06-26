# RivetMesh

RivetMesh is a dependency-free C++17 library for inspecting offline mesh-routing bundles. A bundle combines policy rules, tabular records, route graphs, and tile stencil patches in one deterministic container format. It is intended as a private-repository Fenrir candidate that can be reviewed, expanded, fuzzed, and patched locally.

The project is original source code for this workspace. Keep the final GitHub repository private, original, and primarily human-written.

## Components

- `bundle` parses text and binary `RIVETMESH/1` / `RMB1` containers with metadata, section tables, digests, limits, and typed section dispatch.
- `rules` parses a small policy DSL with assignments, nested blocks, arithmetic expressions, strings, and function calls.
- `table` decodes embedded typed record tables with schemas, rows, integer/decimal/text/flag cells, and validation.
- `route` decodes route graphs with nodes, edges, costs, flags, reachability, and dangling-edge diagnostics.
- `stencil` decodes tile patch records with coordinates, layers, escaped payload bytes, repeat tokens, and per-patch digests.
- `profile` cross-checks metadata, section inventory, table schemas, route connectivity, and stencil patch summaries.
- `analyzer` connects all sections and produces a deterministic summary.

No code performs network access, prompts interactively, reads external include files, or requires credentials.

## Layout

```text
rivetmesh/
  CMakeLists.txt
  README.md
  include/rivetmesh/
  src/
  tests/
  tools/
  fuzz/
    rivetmesh_bundle_fuzzer.cc
    rivetmesh_rules_fuzzer.cc
    corpus/
    dictionary.txt
  .clusterfuzzlite/
    build.sh
    project.yaml
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --config RelWithDebInfo
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## CLI

```bash
./build/rm_inspect fuzz/corpus/rivetmesh_bundle_fuzzer/full_bundle.txt
```

## Fuzzing

ClusterFuzzLite uses `.clusterfuzzlite/build.sh` and writes these targets to `$OUT`:

- `rivetmesh_bundle_fuzzer`
- `rivetmesh_rules_fuzzer`

Local ClusterFuzzLite-style example:

```bash
mkdir -p out
SRC="$PWD" OUT="$PWD/out" CXX=clang++ CXXFLAGS="-O1 -g -fsanitize=fuzzer-no-link,address,undefined" LIB_FUZZING_ENGINE="-fsanitize=fuzzer" ./.clusterfuzzlite/build.sh
./out/rivetmesh_bundle_fuzzer fuzz/corpus/rivetmesh_bundle_fuzzer -dict=fuzz/dictionary.txt -runs=1000
./out/rivetmesh_rules_fuzzer fuzz/corpus/rivetmesh_rules_fuzzer -dict=fuzz/dictionary.txt -runs=1000
```

## Seed Corpus

The recognized seed corpus is under `fuzz/corpus/`.

- `rivetmesh_bundle_fuzzer` seeds contain complete bundles with rules, tables, routes, and stencil patches.
- `rivetmesh_rules_fuzzer` seeds contain standalone DSL programs.

The dictionary contains bundle magic, record keywords, section names, field names, stencil escape tokens, delimiters, and common literals.

## Fenrir Readiness Checklist

- [x] `.clusterfuzzlite/build.sh` exists at repository root.
- [x] `project.yaml` lists every fuzz target.
- [x] Both fuzz targets call real project code.
- [x] Seed corpus exists in a recognized location.
- [x] Build is deterministic and non-interactive.
- [x] Build uses repository-relative or `$SRC` paths only.
- [x] No network access, credentials, prompts, or local absolute paths.
- [x] Source is substantial enough for meaningful fuzzing.

## Manual Review Before Submission

- Confirm the final GitHub repository is private and not a fork.
- Confirm the submitted history is original and primarily human-written.
- Review the source for originality and remove any local scratch artifacts.
- Run the normal tests and the ClusterFuzzLite build from a clean checkout.
- Keep PoC files outside the repository unless Fenrir explicitly asks for one as an upload.
- Do not patch a validated crash until Fenrir asks for the patch workflow.
