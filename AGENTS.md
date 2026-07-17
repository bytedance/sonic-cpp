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

## SIMD Architecture Organization - Detailed Analysis

### Directory Structure Overview

```
include/sonic/internal/arch/
├── simd_dispatch.h          # Central dispatch macro definitions
├── simd_base.h              # Base function dispatch (TrailingZeroes, etc.)
├── simd_quote.h             # Quote/string function dispatch
├── simd_skip.h              # Skip scanner function dispatch
├── simd_itoa.h              # Integer-to-string function dispatch
├── simd_str2int.h           # String-to-integer function dispatch
├── sonic_cpu_feature.h      # CPU feature detection macros
├── common/
│   ├── arm_common/          # Shared ARM implementations
│   │   ├── base.h           # Shared bit manipulation (TrailingZeroes, etc.)
│   │   ├── simd.h           # Shared to_bitmask() function
│   │   ├── quote.h          # Shared Quote() implementation
│   │   ├── skip.inc.h       # Shared skip implementations (template-based)
│   │   ├── itoa.h           # Shared Utoa_8/Utoa_16 implementations
│   │   └── str2int.h        # Shared simd_str2int (scalar fallback)
│   ├── x86_common/          # Shared x86 implementations
│   ├── quote_common.h       # Scalar fallback for parseStringInplace/Quote
│   ├── quote_tables.h       # Escape lookup tables (kEscapedMap, kQuoteTab*)
│   ├── skip_common.h        # Shared EqBytes4, SkipLiteral, IsValidSeparator
│   └── unicode_common.h     # Unicode handling (handle_unicode_codepoint, etc.)
├── neon/                    # ARM NEON implementation
│   ├── base.h               # Re-exports from arm_common
│   ├── simd.h               # NEON-specific simd8/simd8x64 templates
│   ├── quote.h              # NEON-specific parseStringInplace
│   ├── skip.h               # NEON-specific SkipContainer, skip_space
│   ├── itoa.h               # Re-exports from arm_common
│   ├── str2int.h            # Re-exports from arm_common
│   └── unicode.h            # NEON-specific StringBlock, GetNonSpaceBits
├── sve2-128/                # ARM SVE2-128 implementation
│   ├── base.h               # Re-exports from arm_common
│   ├── simd.h               # SVE2-specific type definitions (svuint8x16_t)
│   ├── quote.h              # SVE2-specific parseStringInplace
│   ├── skip.h               # SVE2-specific SkipContainer, skip_space
│   ├── itoa.h               # Re-exports from arm_common
│   ├── str2int.h            # SVE2-specific simd_str2int (optimized)
│   └── unicode.h            # SVE2-specific StringBlock, GetNonSpaceBits
├── avx2/                    # x86 AVX2 implementation
├── sse/                     # x86 SSE implementation
└── x86_ifuncs/              # x86 dynamic dispatch (ifunc)
```

### Required Files Per Architecture Directory

Each architecture directory (`neon/`, `sve2-128/`, `avx2/`, `sse/`) must contain these files:

| File | Purpose |
|------|---------|
| `base.h` | Bit manipulation utilities (TrailingZeroes, ClearLowestBit, LeadingZeroes, CountOnes, PrefixXor, Xmemcpy, InlinedMemcmp, InlinedMemcmpEq) |
| `simd.h` | SIMD vector type definitions (`simd8<T>`, `simd8x64<T>`) with operator overloads, `to_bitmask()`, `eq()`, `lteq()` |
| `quote.h` | `parseStringInplace<ParseFlags>()` and `Quote<SerializeFlags>()` functions |
| `skip.h` | `SkipContainer()`, `skip_space()`, `skip_space_safe()`, plus includes `skip.inc.h` for `GetNextToken()`, `SkipString()`, `GetStringBits()` |
| `itoa.h` | `Utoa_8()` and `Utoa_16()` for integer-to-string conversion |
| `str2int.h` | `simd_str2int()` for string-to-integer conversion |
| `unicode.h` | `StringBlock` struct with `Find()`, `HasQuoteFirst()`, `HasBackslash()`, `HasUnescaped()`, `GetNonSpaceBits()` |

### Architecture Switching via Macro Definitions

The entire dispatch mechanism is controlled by two key macros defined in `simd_dispatch.h:21-55`:

**1. CPU Feature Detection (`sonic_cpu_feature.h:17-47`)**
```cpp
// x86 detection
#if defined(__SSE2__)
#define SONIC_HAVE_SSE
// ... SSE3, SSSE3, SSE4_1, SSE4_2, AVX, AVX2
#endif

// ARM detection
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define SONIC_HAVE_NEON
#endif
#if defined(__ARM_FEATURE_SVE2) && (__ARM_FEATURE_SVE_BITS == 128)
#define SONIC_HAVE_SVE2_128
#endif
```

**2. Static Dispatch (`simd_dispatch.h:25-42`)**
```cpp
#if defined(SONIC_STATIC_DISPATCH)
// x86 priority: AVX2 > SSE
#if defined(SONIC_HAVE_AVX2)
#define SONIC_USING_ARCH_FUNC(func) using avx2::func
#define INCLUDE_ARCH_FILE(file) SONIC_STRINGIFY(avx2/file)
#elif defined(SONIC_HAVE_SSE)
#define SONIC_USING_ARCH_FUNC(func) using sse::func
#define INCLUDE_ARCH_FILE(file) SONIC_STRINGIFY(sse/file)
#endif

// ARM priority: SVE2-128 > NEON
#if defined(SONIC_HAVE_SVE2_128)
#define SONIC_USING_ARCH_FUNC(func) using sve2_128::func
#define INCLUDE_ARCH_FILE(file) SONIC_STRINGIFY(sve2-128/file)
#elif defined(SONIC_HAVE_NEON)
#define SONIC_USING_ARCH_FUNC(func) using neon::func
#define INCLUDE_ARCH_FILE(file) SONIC_STRINGIFY(neon/file)
#endif
```

**3. Dynamic Dispatch (`simd_dispatch.h:44-53`)**
```cpp
#elif defined(SONIC_DYNAMIC_DISPATCH)
#if defined(__x86_64__)
#define SONIC_USING_ARCH_FUNC(func)  // Empty - uses ifunc resolution
#define INCLUDE_ARCH_FILE(file) SONIC_STRINGIFY(x86_ifuncs/file)
#elif defined(SONIC_HAVE_NEON)
#define SONIC_USING_ARCH_FUNC(func) using neon::func
#define INCLUDE_ARCH_FILE(file) SONIC_STRINGIFY(neon/file)
#endif
```

**4. Dispatch Header Pattern (e.g., `simd_base.h:17-36`)**
```cpp
#include "simd_dispatch.h"
#include INCLUDE_ARCH_FILE(base.h)  // Includes e.g., "neon/base.h"

namespace sonic_json {
namespace internal {
SONIC_USING_ARCH_FUNC(TrailingZeroes);   // Becomes: using neon::TrailingZeroes;
SONIC_USING_ARCH_FUNC(ClearLowestBit);
// ... more functions
}  // namespace internal
}  // namespace sonic_json
```

### SIMD Function Naming and Organization

**Namespace Hierarchy:**
```
sonic_json::internal::
├── common::           # Scalar fallbacks and cross-arch utilities
├── arm_common::       # Shared ARM implementations
├── x86_common::       # Shared x86 implementations
├── neon::             # NEON-specific implementations
├── sve2_128::         # SVE2-128-specific implementations
├── avx2::             # AVX2-specific implementations
├── sse::              # SSE-specific implementations
└── (direct)           # Dispatched functions via using declarations
```

**Key Function Patterns:**

1. **SIMD Vector Types** (`simd.h`):
   - `simd8<T>` - 16-byte vector (16 elements of type T)
   - `simd8x64<T>` - 64-byte block (4 x simd8<T> chunks)
   - Methods: `load()`, `store()`, `to_bitmask()`, `eq()`, `lteq()`, `reduce_or()`, operator overloads

2. **String Block Pattern** (`unicode.h`):
   ```cpp
   struct StringBlock {
     static StringBlock Find(const uint8_t* src);  // Load and analyze 16 bytes
     template<ParseFlags> bool HasQuoteFirst() const;
     bool HasBackslash() const;
     bool HasUnescaped() const;
     int QuoteIndex() const;
     int BsIndex() const;
     uint64_t bs_bits, quote_bits, unescaped_bits;  // NEON: bitmasks
     // OR for SVE2: unsigned bs_index, quote_index, unescaped_index;
   };
   ```

3. **String Parsing** (`quote.h`):
   ```cpp
   template<ParseFlags parseFlags = kParseDefault>
   sonic_force_inline size_t parseStringInplace(uint8_t*& src, SonicError& err);
   ```

4. **Skip Functions** (`skip.h`):
   ```cpp
   sonic_force_inline bool SkipContainer(const uint8_t* data, size_t& pos,
                                         size_t len, uint8_t left, uint8_t right);
   sonic_force_inline uint8_t skip_space(const uint8_t* data, size_t& pos,
                                         size_t&, uint64_t&);
   ```

### CPU Feature Detection

**Compile-time detection only** (no runtime CPUID for ARM):
- `sonic_cpu_feature.h` uses compiler predefined macros
- ARM: `__ARM_NEON`, `__ARM_FEATURE_SVE2`, `__ARM_FEATURE_SVE_BITS`
- x86: `__SSE2__`, `__AVX2__`, etc.
- No runtime feature detection for ARM (static dispatch only)
- x86 supports dynamic dispatch via `x86_ifuncs/` using GNU ifunc attribute

**Build system integration:**
- CMake: `-DENABLE_SVE2_128=ON` enables SVE2 compiler flags
- Bazel: `--//:sonic_arch={arm|sve2|haswell|westmere}`
- Bazel: `--//:sonic_dispatch={static|dynamic}`

### Common Code Organization

**Three levels of code sharing:**

1. **`common/*.h` - Architecture-independent:**
   - `quote_common.h`: Scalar fallback `parseStringInplace()` and `Quote()` for x86 dynamic dispatch
   - `skip_common.h`: `EqBytes4()`, `SkipLiteral()`, `IsValidSeparator()` - pure scalar
   - `unicode_common.h`: `handle_unicode_codepoint()`, `codepoint_to_utf8()`, `hex_to_u32_nocheck()`, `GetEscaped()`
   - `quote_tables.h`: `kEscapedMap[256]`, `kQuoteTabLowerCase[256]`, `kQuoteTabUpperCase[256]`, `kNeedEscaped[256]`

2. **`common/arm_common/*.h` - ARM-specific shared code:**
   - `base.h`: `TrailingZeroes()` → `__builtin_ctzll()`, `ClearLowestBit()`, `LeadingZeroes()` → `__builtin_clzll()`, `CountOnes()` → `__builtin_popcountll()`, `PrefixXor()`, `Xmemcpy()`, `InlinedMemcmp()`, `InlinedMemcmpEq()`
   - `simd.h`: `to_bitmask(uint8x16_t)` using NEON `vshrn_n_u16`
   - `quote.h`: `Quote<SerializeFlags>()` using NEON intrinsics, shared by both NEON and SVE2-128
   - `skip.inc.h`: Template implementations of `GetStringBits<T>()`, `GetNextToken<N>()`, `SkipString()`, `skip_container<T>()` - included with `#include` (not `using`)
   - `itoa.h`: `UtoaNeon()`, `Utoa_8()`, `Utoa_16()` using NEON intrinsics
   - `str2int.h`: Scalar fallback `simd_str2int()` (used by NEON, overridden by SVE2-128)

3. **Architecture-specific files (`neon/`, `sve2-128/`):**
   - Thin wrapper headers that `using` declarations from `arm_common`
   - Architecture-specific optimizations where needed:
     - NEON: Full `simd8<T>` and `simd8x64<T>` template implementations
     - SVE2-128: Minimal `simd.h` (reuses NEON simd types via `../neon/simd.h` in skip.h), optimized `simd_str2int()` using SVE2 `svdot`, predicate-based `StringBlock` using `svmatch`/`svbrkb`/`svcntp`

### Key Architectural Patterns

**1. Re-export Pattern (most files):**
```cpp
// neon/base.h
#include "../common/arm_common/base.h"
namespace neon {
using arm_common::TrailingZeroes;
using arm_common::ClearLowestBit;
// ...
}
```

**2. Template Include Pattern (`skip.inc.h`):**
```cpp
// neon/skip.h
#include "../common/arm_common/skip.inc.h"  // Injects template functions directly

sonic_force_inline bool SkipContainer(...) {
  return skip_container<simd8x64<uint8_t>>(data, pos, len, left, right);
}
```

**3. Cross-architecture Reuse (SVE2-128 using NEON):**
```cpp
// sve2-128/skip.h
#include "../neon/simd.h"  // Reuse NEON simd types for skip_container

sonic_force_inline bool SkipContainer(...) {
  // Use NEON implementation since it's faster for comparisons
  return skip_container<neon::simd8x64<uint8_t>>(data, pos, len, left, right);
}
```

**4. Override Pattern (SVE2-128 str2int):**
```cpp
// sve2-128/str2int.h - does NOT include arm_common/str2int.h
// Instead provides optimized SVE2 implementation
sonic_force_inline uint64_t simd_str2int(const char* c, int& man_nd) {
  // Uses svld1, svmatch, svdot, etc.
}
```

### NEON vs SVE2-128 Implementation Differences

| Aspect | NEON | SVE2-128 |
|--------|------|----------|
| Vector type | `uint8x16_t` | `svuint8x16_t` (fixed 128-bit SVE) |
| Load | `vld1q_u8(ptr)` | `svld1_u8(svptrue_b8(), ptr)` |
| Store | `vst1q_u8(ptr, v)` | `svst1_u8(svptrue_b8(), ptr, v)` |
| Compare | `vceqq_u8(v, dup)` → bitmask via `to_bitmask()` | `svmatch(ptrue, v, dup)` → predicate |
| Bitmask format | `uint64_t` (4 bits per lane) | `svbool_t` (predicate) |
| Find first | `TrailingZeroes(bitmask) >> 2` | `svcntp_b8(ptrue, svbrkb_z(ptrue, pmatch))` |
| StringBlock | Stores 3x `uint64_t` bitmasks | Stores 3x `unsigned` indices (0-15) |
| str2int | Scalar loop from arm_common | Optimized SVE2 `svdot` implementation |
| SkipContainer | Uses `neon::simd8x64` | Also uses `neon::simd8x64` (faster for comparisons) |

### Build Configuration Matrix

| Bazel Flag | CMake Equivalent | Effect |
|------------|------------------|--------|
| `--//:sonic_arch=arm` | (default on ARM) | Enables NEON |
| `--//:sonic_arch=sve2` | `-DENABLE_SVE2_128=ON` | Enables SVE2-128 (higher priority than NEON) |
| `--//:sonic_arch=haswell` | (default on x86) | Enables AVX2 |
| `--//:sonic_arch=westmere` | (manual) | Enables SSE4.2 |
| `--//:sonic_dispatch=static` | (default) | Static dispatch via macros |
| `--//:sonic_dispatch=dynamic` | (manual) | Dynamic dispatch via ifunc (x86 only) |

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
