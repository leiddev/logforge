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

> Status: **1.3.0.** The 1.0.0 API is frozen; a breaking change from here needs a
> major version.

Full guide: [docs/LOGGING.md](docs/LOGGING.md) -- the interface, where a line
goes, the format specifiers, backend configuration, writing a backend, and
migrating an existing logging layer.

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
- spdlog 1.10 or newer, only for `logforge::spdlog`. The backend uses the UDP sink
  in its config-struct form, which 1.10 already has; CI builds against 1.17.0. A
  build that already has an `spdlog::spdlog` target -- a project that vendors
  spdlog, or one that adds this directory to a tree where spdlog is a subproject --
  is used as it is, with no version check and nothing downloaded.
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
find_package(logforge 1.0 REQUIRED)
target_link_libraries(app PRIVATE logforge::core)      # or logforge::spdlog
```

```cpp
#include <logforge/macros.hpp>
#include <logforge/stderr_logger.hpp>

int main() {
    logforge::StderrLogger logger(logforge::Level::info);
    logforge::set_default(&logger);

    LOGFORGE_LOG_INFO("pad {} refused {} message(s)", "in", 3);   // [info] pad in refused 3 message(s)
    LOGFORGE_LOG_WARN("dropped {:04x} frames", 255);              // [warn] dropped 00ff frames
}
```

Contexts that own their own logger bind it for the duration of a callback, and
anything inside -- a macro in a third-party header, say -- writes to the right
place:

```cpp
void Pad::dispatch() {
    const logforge::ScopedBinding binding(logger_);
    LOGFORGE_LOG_DEBUG("tick {}", tick_);   // goes to logger_, on this thread only
}
```

## The spdlog backend

```cpp
#include <logforge/spdlog_backend.hpp>

logforge::SpdlogBackend::Config config;
config.level = logforge::Level::info;
config.file_path = "logs/pipeline.log";   // empty: no file
config.max_file_bytes = 10 * 1024 * 1024; // 0: one file, no rotation
config.max_files = 5;
config.udp_host = "10.0.0.7";             // empty: no UDP sink
config.udp_port = 5140;
config.pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v";

logforge::SpdlogBackend backend(config);
logforge::set_default(&backend);
```

The sinks are spdlog's, so a line written to a rotation, a colour console and a
UDP collector at once costs nothing extra. `backend.native()` hands out the
underlying `spdlog::logger` for everything this struct does not cover -- named
loggers, custom sinks, spdlog's backtrace buffer.

`backend.native_shared()` hands out the same logger as a `shared_ptr`, which is
the form spdlog's registry takes. That is the bridge for a program that still has
raw spdlog calls, or that depends on a library which logs through spdlog's global
logger: those lines land on these sinks instead of spdlog's built-in default.

```cpp
spdlog::set_default_logger(backend.native_shared());
```

A record's `component` becomes a `"component: "` prefix on the message body, the
same text `StderrLogger` produces, so changing backends changes where a line goes
and how the pattern decorates it -- not what it says. Source location goes to
spdlog as a `source_loc`, which is what makes `%s`/`%g`/`%#` work.

## Options

| Option | Default (top level) | Meaning |
|---|---|---|
| `LOGFORGE_BUILD_TESTS` | `ON` | Build the unit tests |
| `LOGFORGE_BUILD_SPDLOG` | `ON` | Build the `logforge::spdlog` backend |
| `LOGFORGE_FETCH_SPDLOG` | `OFF` | Download spdlog instead of finding an installed one |
| `LOGFORGE_INSTALL` | `ON` | Generate install and `find_package` rules |
| `LOGFORGE_REQUIRE_TESTS` | `OFF` | Fail configure instead of skipping when GoogleTest is missing |
| `LOGFORGE_FETCH_GTEST` | `OFF` | Download googletest when it is not installed |
| `LOGFORGE_WERROR` | `OFF` | Treat warnings in this repository's own targets as errors |

Fetching is an explicit choice rather than a fallback, so CI and a developer's
machine cannot quietly build against different spdlog versions. A build that
fetches spdlog also installs it into the same prefix: the exported
`logforge::spdlog` names `spdlog::spdlog` as a usage requirement, and a prefix
without spdlog cannot satisfy it.

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
include/logforge/     public headers (core, macros, backends)
src/                  the spdlog backend
cmake/                package config and generated version header template
docs/                 LOGGING.md, the full guide
tests/                unit tests; tests/consumer builds against an install prefix
examples/             quickstart, scoped_binding, backend_configuration
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

`v1.1.0` adds `set_current()` -- a binding that is not tied to a scope -- while
pipeforge integrates. Nothing else moved, so a 1.0.0 consumer needs no edits.

## License

MIT. See [LICENSE](LICENSE).
