# logforge

[![CI](https://github.com/leiddev/logforge/actions/workflows/ci.yml/badge.svg)](https://github.com/leiddev/logforge/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A small logging bridge: one tiny interface that every producer talks to, and a
backend that receives finished text and writes it somewhere. Ships with a
spdlog backend and a renderer for `{}`-style format strings.

- **The interface is text.** `log(level, text, size)` and `enabled(level)`.
  Nothing about formatting, sinks or spdlog crosses it, so it never has to
  change and a backend can be replaced without touching a single call site.
- **C++11, no dependencies.** The core is header only and includes nothing.
- **Rendering never throws.** A malformed format specifier comes out verbatim
  instead of raising `format_error`, because a logging call inside an event
  handler is the last place that may unwind.
- **Backends are optional.** `logforge::core` alone is enough to compile a
  producer; the spdlog backend is a separate target you link only if you want it.

> Status: **0.2.0, API not frozen.** The core interface is stable; the backend
> and renderer land in the milestones listed at the bottom.

## Dependency graph

```
        your code
            │  LOGFORGE_LOG_INFO("...", args)     <- renders, then calls
            ▼
     logforge::core          header only, C++11, zero dependencies
     (Level, Logger, thread binding, macros, renderer)
            ▲
            │  implements logforge::Logger
     logforge::spdlog        static library
     (sinks, patterns, level mapping, config)
            │
            ▼
         spdlog
```

Producers depend on `logforge::core` only. That is what makes "which backend"
a link-time choice instead of an `#include` in a public header.

## Targets

| Target | Kind | Needs | Use it when |
|---|---|---|---|
| `logforge::core` | INTERFACE | nothing | You produce log lines, or you write your own backend |
| `logforge::spdlog` | STATIC | spdlog | You want rotating files, console colours and UDP without writing a sink |

## Requirements

- CMake 3.16 or newer
- A C++11 compiler (GCC 7+, Clang 6+, MSVC 2017+)
- GoogleTest, only for the tests (`LOGFORGE_REQUIRE_TESTS` / `LOGFORGE_FETCH_GTEST`)

## Build and test

```sh
cmake -S . -B build -DLOGFORGE_REQUIRE_TESTS=ON -DLOGFORGE_FETCH_GTEST=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

On Windows the default Visual Studio generator works as well:

```powershell
cmake -S . -B build -A x64 -DLOGFORGE_REQUIRE_TESTS=ON -DLOGFORGE_FETCH_GTEST=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Use it

```cmake
find_package(logforge 0.1 REQUIRED)
target_link_libraries(app PRIVATE logforge::core)      # or logforge::spdlog
```

## Options

| Option | Default (top level) | Meaning |
|---|---|---|
| `LOGFORGE_BUILD_TESTS` | `ON` | Build the unit tests |
| `LOGFORGE_INSTALL` | `ON` | Generate install and `find_package` rules |
| `LOGFORGE_REQUIRE_TESTS` | `OFF` | Fail configure instead of skipping when GoogleTest is missing |
| `LOGFORGE_FETCH_GTEST` | `OFF` | Download googletest when it is not installed |
| `LOGFORGE_WERROR` | `OFF` | Treat warnings in this repository's own targets as errors |

As a subproject (`add_subdirectory`) everything defaults to off except the
library itself, so vendoring logforge adds build time and no surprises.

## Design notes

**Why text instead of a structured record.** The producers this library was
written for do not all have a format string and a parameter list to hand: a
pipeline runtime concatenates a component name with a message, a stream parser
has already run something through `operator<<`. An interface that demands
"format + arguments" would force those producers to keep their own formatter and
re-render, which is exactly the duplication this library exists to remove.

**Why the renderer lives next to the macros.** Rendering is a front-end
concern: by the time a record reaches `Logger::log()` it has to be a string,
because that is what every sink ultimately writes. Keeping the renderer in the
front end also means one implementation shared by every producer instead of one
per backend.

**Why `const char*, size_t` instead of `const std::string&`.** It lets a caller
render into a reusable thread-local buffer and hand out a view of it, and it
keeps `<string>` out of a header that is included by every translation unit.

**Contract for backend authors.** `Logger::log()` is synchronous: once it
returns, the text and the metadata may be freed. A backend that wants to keep
either must copy it. This is what allows the interface to have no ownership
story at all.

## Repository layout

```
include/logforge/     public headers (core)
src/                  the spdlog backend (added with that target)
cmake/                package config and generated version header template
tests/                unit tests; tests/consumer builds against an install prefix
examples/             small programs, one per feature
.github/workflows/    the CI matrix
```

## Roadmap

| Milestone | Content |
|---|---|
| M1 | Repository skeleton, `logforge::core` with `Level`, CI matrix, install rules |
| M2 | `Logger`, `Record`, `NullLogger`, `StderrLogger`, thread-local binding |
| M3 | Non-throwing renderer and the macros (`LOGFORGE_LOG_*`, `LOG_*` compat) |
| M4 | `logforge::spdlog` backend: sinks, patterns, level mapping, config |
| M5 | Examples, docs, `v1.0.0` tag |
| M6 | pipeforge 5.0.0 consumes `logforge::core`; its own log layer is deleted |
| M7 | xec migrates its three logging vocabularies onto this one |

## License

MIT. See [LICENSE](LICENSE).
