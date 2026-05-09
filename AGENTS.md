# sonic-cpp Agent Guide

This file is for AI coding tools and human contributors working in this
repository. It follows the common AGENTS.md shape used by public projects:
project overview, commands, architecture, style, testing, security, and
agent-specific rules. Keep it practical: prefer facts that can be verified from
source, build files, and tests.

If a subdirectory gets its own `AGENTS.md` later, treat that more specific file
as overriding this root guide for files under that subtree.

## Project Shape

`sonic-cpp` is a header-only, SIMD-accelerated C++ JSON parser/serializer.
The public API is under namespace `sonic_json` and is normally consumed through:

- `include/sonic/sonic.h`
- `sonic_json::Document`
- `sonic_json::Node`

The README says C++11 or above, but the active CMake and Bazel builds use
C++17. Treat build files and tests as the practical source of truth.

Primary source directories:

- `include/sonic/dom/`: DOM tree, document, parser, handlers, JSON pointer.
- `include/sonic/internal/`: low-level stack, SIMD helpers, arch dispatch.
- `include/sonic/internal/arch/`: x86/Arm/SVE2 implementation details.
- `include/sonic/jsonpath/`: JSONPath query and dump helpers.
- `include/sonic/experiment/`: experimental helpers such as lazy update.
- `tests/`: GoogleTest unit tests.
- `benchmark/`: benchmark binary sources.
- `fuzz/`: CMake-only fuzz target.

Important public plumbing:

- Errors: `include/sonic/error.h`
- Allocator: `include/sonic/allocator.h`
- Internal stack: `include/sonic/internal/stack.h`
- Write buffer: `include/sonic/writebuffer.h`
- Parse flags: `include/sonic/dom/flags.h`

## Agent Workflow

Before changing behavior:

1. Read the relevant public header and its tests.
2. Search with `rg`; avoid broad filesystem scans.
3. Preserve existing API style unless the task explicitly asks for a breaking
   API change.
4. Add focused tests before or with behavior changes.
5. Run the smallest relevant test first, then the broader suite if the change
   touches shared parsing, DOM, allocator, SIMD, or serialization code.

When reviewing changes:

- Lead with correctness bugs, memory-safety risks, API compatibility problems,
  and missing tests.
- Pay special attention to allocation failure paths. Silent failure is usually
  not acceptable in new code.
- Do not simplify arch dispatch or SIMD code without checking both build flags
  and tests.

## Change Checklist

Use this checklist before handing work back:

1. The change is scoped to the requested behavior.
2. Public API compatibility has been considered and documented if affected.
3. Allocation failures either propagate `SonicError` or preserve the old object
   state for legacy APIs.
4. Relevant unit tests were added or updated.
5. The smallest relevant test was run.
6. Broader tests were run when shared parser, DOM, allocator, SIMD, or
   serialization behavior changed.
7. No build outputs, dependency caches, or benchmark artifacts were edited.

## Build And Test Commands

Quick command recap:

| Task | Command |
| --- | --- |
| Configure CMake build | `cmake -S . -B build` |
| Build CMake unit test | `cmake --build build --target unittest -j` |
| Run CMake unit test | `./build/tests/unittest` |
| Run Bazel unit test | `bazel run :unittest --//:sonic_arch=haswell --//:sonic_dispatch=static` |
| Run full Bazel helper | `bash scripts/unittest.sh -g --arch=haswell --dispatch=static` |
| Run benchmark with Bazel | `bazel run :benchmark --compilation_mode=opt` |

### CMake

Common local flow:

```bash
cmake -S . -B build
cmake --build build --target unittest -j
./build/tests/unittest
```

Useful CMake options:

- `BUILD_UNITTEST=ON` by default.
- `BUILD_FUZZ=OFF` by default.
- `BUILD_BENCH=OFF` by default.
- `ENABLE_SVE2_128=OFF` by default.

Sanitizers in CMake tests:

- `tests/CMakeLists.txt` enables ASAN by default through `ENABLE_ASAN=ON`.
- UBSAN can be enabled with `-DENABLE_UBSAN=ON`.

### Bazel

Bazel uses Bzlmod. `.bazelversion` pins the expected version.

Useful commands:

```bash
bazel run :unittest --//:sonic_arch=haswell --//:sonic_dispatch=static
bazel run :benchmark --compilation_mode=opt
bash scripts/unittest.sh -g --arch=haswell --dispatch=static
```

Bazel flags:

- `--//:sonic_arch={default|arm|sve2|westmere|haswell}`
- `--//:sonic_dispatch={static|dynamic}`
- `--//:sonic_sanitizer={no|gcc|clang}`

Note: `scripts/unittest.sh` accepts `--arch=aarch64` and `--arch=arm64`, then
maps them to Bazel's `arm` setting. When invoking Bazel directly, use
`--//:sonic_arch=arm`.

## Error Handling And Allocation Rules

Code in this repository often runs in parser, serializer, allocator, and SIMD
hot paths. Allocation failure handling must be explicit enough that callers can
distinguish resource failure from valid empty/null JSON values.

Preferred patterns:

- Parser/handler failures should return a concrete `SonicError`, especially
  `kErrorNoMem` for allocation failure.
- APIs that can fail should expose failure through the existing project style:
  `ParseResult`, `SonicError`, boolean success, or allocator error state.
- Legacy public APIs may need to keep source-compatible return types. In that
  case, preserve the previous object state on allocation failure whenever
  practical and provide or use a checked path for callers that need diagnostics.
- Do not ignore return values from low-level buffer, stack, allocator, parser,
  or handler methods that can fail.
- Check integer overflow before size arithmetic for allocations, padding,
  capacity growth, and SIMD lookahead buffers.

Key places to inspect for allocation-sensitive changes:

- `include/sonic/allocator.h`
- `include/sonic/internal/stack.h`
- `include/sonic/writebuffer.h`
- `include/sonic/dom/parser.h`
- `include/sonic/dom/handler.h`
- `include/sonic/dom/schema_handler.h`
- `include/sonic/dom/dynamicnode.h`
- `tests/allocator_test.cpp`
- parser/DOM tests under `tests/`

## DOM And Memory Model

`GenericDocument` owns or references an allocator and is also the root JSON
node. Re-parsing a document discards the previous tree; any raw pointer,
iterator, or node reference from the old tree must be reacquired after parse.

`DNode` stores arrays and objects in compact contiguous buffers. Object member
keys are part of the container's lookup invariants; avoid APIs or internal
changes that let callers mutate keys in a way that invalidates cached lookup
structures.

String storage has two modes:

- Non-owning string views for parsed/raw/const strings.
- Allocator-backed copies for APIs that copy strings.

When copying or mutating DOM nodes:

- Prefer commit-after-success updates for operations that can fail halfway.
- Keep source compatibility for existing public APIs where possible.
- Avoid `memmove`/raw byte copies for non-trivial node/member objects unless
  the type is explicitly safe for that operation.

## ParseOnDemand And JSONPath

`ParseOnDemand` is optimized to find a target subtree without fully materializing
the document.

Rule of thumb:

- Default behavior should preserve the fast short-circuit path.
- Full-document validation in on-demand paths is a semantic and performance
  choice; do not add it by default unless the API or caller explicitly asks for
  it.
- Skipped branches still must not swallow local parse errors such as malformed
  strings, invalid numbers, missing separators, or impossible object/array
  syntax.

Relevant files:

- `include/sonic/dom/parser.h`
- `include/sonic/internal/arch/simd_skip.h`
- `include/sonic/jsonpath/*.h`
- `tests/document_test.cpp`
- `tests/jsonpath_test.cpp`
- `tests/json_tuple_test.cpp`

## SIMD And Architecture Dispatch

The SIMD layer has static and dynamic dispatch modes. Do not assume only AVX2
exists even though x86 AVX2 is the primary documented target.

Important files:

- `include/sonic/internal/arch/sonic_cpu_feature.h`
- `include/sonic/internal/arch/simd_dispatch.h`
- `include/sonic/internal/arch/avx2/`
- `include/sonic/internal/arch/common/`
- `include/sonic/internal/arch/neon/`
- `include/sonic/internal/arch/sve2-128/`
- `include/sonic/internal/arch/x86_ifuncs/`

When touching shared SIMD helpers, validate both parser and skip/on-demand
tests. If possible, also test `--//:sonic_dispatch=dynamic` on x86.

## Coding Style

- Follow root `.clang-format` (`BasedOnStyle: Google`).
- Keep public headers self-contained.
- Do not add heavyweight dependencies to the header-only library.
- Prefer simple, explicit control flow in parser and allocator code.
- Keep comments sparse and useful; explain invariants and failure handling,
  not obvious assignments.
- Default to ASCII in source and docs unless a file already uses non-ASCII.

## Testing Guidance

Choose tests based on the touched area:

- Allocator/stack/write buffer: `tests/allocator_test.cpp`,
  `tests/writebuffer_test.cpp`, `tests/parser_oom_test.cpp`.
- DOM mutation/copy/member map: `tests/node_test.cpp`,
  `tests/document_test.cpp`, `tests/parser_oom_test.cpp`.
- Full parser and lazy parser: `tests/parser_oom_test.cpp`,
  `tests/document_test.cpp`.
- Parse schema: `tests/parse_schema_test.cpp`.
- Parse on demand / JSON pointer: `tests/document_test.cpp`,
  `tests/json_pointer_test.cpp`.
- JSONPath / tuple extraction: `tests/jsonpath_test.cpp`,
  `tests/json_tuple_test.cpp`.
- SIMD skip scanner: `tests/skip_test.cpp`.

For broad validation, run:

```bash
cmake --build build --target unittest -j
./build/tests/unittest
```

or:

```bash
bash scripts/unittest.sh -g --arch=haswell --dispatch=static
```

Testing philosophy:

- Test public behavior first; avoid overfitting tests to private helper details.
- For bug fixes, add a regression test that fails on the old behavior.
- For allocation-failure fixes, assert both the reported error and the
  preserved state when preservation is part of the contract.
- For parser fixes, include malformed input around the exact branch being
  changed, not only a happy-path JSON sample.
- For performance-sensitive parser/SIMD changes, keep tests deterministic and
  put speed measurements in benchmarks, not unit tests.

## Benchmarking

Use benchmarks for changes that affect:

- object lookup or member insertion/removal,
- parser hot loops,
- SIMD skip/string paths,
- serialization,
- allocator growth behavior.

Commands:

```bash
cmake -S . -B build-bench -DBUILD_BENCH=ON
cmake --build build-bench --target bench -j
./build-bench/benchmark/bench
```

or:

```bash
bazel run :benchmark --compilation_mode=opt
```

## Common Pitfalls

- Do not treat `operator[]` missing-member behavior as mutable storage; prefer
  `FindMember`.
- Do not mutate object member keys through iterators or internal aliases unless
  every affected lookup structure is rebuilt or updated.
- Do not add parse-on-demand full validation by default unless the task accepts
  a performance/semantic change.
- Do not ignore trailing characters in full parse paths.
- Do not use throwing allocation in low-level parser/SAX paths when the
  surrounding code expects explicit error propagation.
- Do not change public type layout casually; this is a header-only library and
  downstream code may depend on source-level details.
- Do not modify generated build outputs, `build/`, `bazel-*`, or benchmark
  result artifacts unless explicitly asked.

## PR / Handoff Notes

When summarizing work for another tool or reviewer:

- List behavior changes first, then files touched.
- State which tests were run and which were not run.
- Call out API compatibility, memory-safety, and performance tradeoffs.
- Mention any remaining risk if only a narrow test was run.
- For large parser or SIMD changes, include the exact architecture/dispatch mode
  used for validation.

## Security Notes

Inputs should be treated as untrusted JSON. The usage docs state that UTF-8 is
assumed and not verified by default. Always check parse results with:

- `HasParseError()`
- `GetParseError()`
- `GetErrorOffset()`
- `ErrorMsg(...)`

Security issues should not be disclosed through public issues. Follow
`CONTRIBUTING.md` for the reporting contact.
