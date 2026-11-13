# logforge

[![CI](https://github.com/leiddev/logforge/actions/workflows/ci.yml/badge.svg)](https://github.com/leiddev/logforge/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A small logging bridge: one tiny interface that every producer talks to, and a
backend that receives finished text and writes it somewhere. Ships with a
spdlog backend and a renderer for `{}`-style format strings.

- **The interface is text.** `Logger` has two virtuals -- `enabled(level)` and
  `log(const Record &)` -- and the record carries the message as
  `(const char *, size_t)`. Nothing about formatting, sinks or spdlog crosses a
  core header, so call sites survive a backend change.
- **C++11, no dependencies.** The core is header only and includes no
  third-party header: only `<cstddef>`, `<cstdio>`, `<cstring>`, `<string>` and
  `<atomic>`.
- **Rendering never throws.** A malformed format specifier comes out verbatim
  instead of raising `format_error`, because a logging call inside an event
  handler is the last place that may unwind.
- **Backends are optional.** `logforge::core` alone is enough to compile a
  producer; the spdlog backend is a separate target you link only if you want
  it. A package built with the backend enabled still asks CMake for spdlog at
  configure time -- see [Where spdlog is allowed to appear](#where-spdlog-is-allowed-to-appear).

> Status: **1.4.2.** The 1.0.0 API is frozen; a breaking change from here needs a
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

## Why not spdlog directly

spdlog is a complete logger: fmt formatting, a dozen sinks, asynchronous
logging, backtrace, a registry of named loggers. logforge is a **front end plus
one backend**, and the front end is what a code base spread over several
binaries usually lacks. It does not compete with spdlog on features; it decides
where the boundary sits.

| You get | You give up |
|---|---|
| Call sites depend on `logforge::core` only; a backend swap is a link-time and startup-time change | spdlog's API at the call site: `logger->info(...)`, `spdlog::set_level()`, named-logger lookup |
| Producers never compile spdlog or fmt; `<logforge/spdlog_backend.hpp>` forward declares `spdlog::logger` | fmt's format language: no positional arguments, alignment or fill, chrono types, or compile-time format checking |
| Rendering never throws; a malformed specifier comes out verbatim | the same mistake is silent at run time instead of a compile error or an exception |
| Message text supports `{}`, `{:4}`, `{:x}`, `{:X}`, `{:04x}`, `{:.2f}`, `{:7.2f}`, `{{`/`}}` | everything else fmt offers |
| Thread-scoped routing (`ScopedBinding`), so a per-object logger reaches third-party code | spdlog's registry model; there is no `get_logger("x")` here |
| A `Config` struct, `open()` with a per-sink `Report`, `create_directories`, `install_as_default()` | everything else spdlog configures at run time: async queues, periodic flush, the error handler, backtrace, per-sink levels and patterns, most sink types |
| One `std::string` per enabled statement, rendered in the front end | spdlog's formatting into a reusable buffer; this library ships no benchmark, so do not read the table as a performance claim |

Keep spdlog directly when the program needs what is in the right column: an
async logger with a queue, per-module named loggers, syslog or daily files, or a
sink of your own. `native()` exists so that using one of those does not force
the rest of the code to know about spdlog.

## Where spdlog is allowed to appear

Four layers, and only one of them leaks on purpose:

| Layer | Rule |
|---|---|
| Headers | No core header includes spdlog. `<logforge/spdlog_backend.hpp>` forward declares `spdlog::logger` and nothing else, which keeps a producer's translation units free of spdlog and fmt. |
| CMake targets | `logforge::core` is INTERFACE with no dependencies. `logforge::spdlog` is STATIC and links `spdlog::spdlog` PUBLIC, because `native()` returns a `spdlog::logger *` and the caller needs the complete type. |
| Process state | `install_as_default()` writes logforge's default *and* spdlog's (`spdlog::set_default_logger()`), so a dependency that logs through spdlog's globals lands on the same sinks. That is a deliberate side effect, not an implementation detail. |
| Package config | A package built with `LOGFORGE_BUILD_SPDLOG=ON` runs `find_dependency(spdlog CONFIG)` at configure time. A consumer that links only `logforge::core` still needs spdlog *installed*, unless the package was built with `-DLOGFORGE_BUILD_SPDLOG=OFF`. |

So the claim is "no spdlog header required", not "no spdlog type anywhere": the
two `native*` functions name `spdlog::logger`, which is the price of an escape
hatch.

## What the spdlog backend uses

The whole spdlog API surface behind `logforge::spdlog`, so that a replacement
backend is a known amount of work rather than an unknown one.

| Area | Used |
|---|---|
| Logger object | `spdlog::logger` built from a `sink_ptr` list; `set_level()`, `set_pattern()`, `flush()` |
| Dispatch | `logger->log(source_loc, level, string_view_t)` -- the string-view overload, so text the renderer already produced is never parsed by fmt a second time |
| Sinks | `stdout_color_sink_mt`, `basic_file_sink_mt`, `rotating_file_sink_mt`, `udp_sink_mt` with `udp_sink_config` |
| Types | `spdlog::level::level_enum`, `source_loc`, `string_view_t`, `sink_ptr` |
| Global state | `set_default_logger()`, `default_logger()` |
| Text | spdlog's pattern language (`%Y`, `%l`, `%s`, `%#`, `%n`, `%v`), and the `what()` of its exceptions, which `Report` stores verbatim |
| Build | `find_package(spdlog 1.10 CONFIG)` or FetchContent v1.17.0; `ws2_32` on Windows, which the UDP sink needs |

Not used: asynchronous logging and its thread pool, periodic flush, the error
handler, backtrace, the registry's named lookup, per-sink levels or patterns,
any sink outside the four above, and fmt's formatting at dispatch time.

## Replacing the backend

Call sites do not change. Start-up code does, and so does the amount of
behaviour the new backend has to reproduce.

1. Implement `logforge::Logger` and link `logforge::core` only -- not
   `logforge::spdlog`. The contract is in
   [docs/LOGGING.md](docs/LOGGING.md#7-writing-a-backend): `log()` is
   synchronous, must not keep the borrowed text, may be called from several
   threads at once, and must not log into itself.
2. Install it with `logforge::set_default(&backend)`, or bind it per thread with
   `ScopedBinding`. `install_as_default()` is spdlog-specific; a new backend does
   not need an equivalent.
3. Delete the `SpdlogBackend` uses from start-up: `Config`, `open()`, `Report`,
   `flush()`, `set_pattern()`, and every call of `native()` or `native_shared()`.
4. If the process must not depend on spdlog at all, install logforge with
   `-DLOGFORGE_BUILD_SPDLOG=OFF`, so the package config stops requiring spdlog
   at configure time.

To produce the same lines, a new backend needs: console, file, rotation and UDP
destinations; a timestamp -- `Record` carries none, so the backend takes it in
`log()`; the `"component: "` prefix on the body; level filtering with `off`
always silenced; `create_directories`; and a `flush()`.

**The part a replacement does not fix.** `install_as_default()` also sets
spdlog's default logger, which is how a dependency calling `spdlog::info()`
reaches the file. Dropping `logforge::spdlog` does not drop spdlog from the
process: either keep the dependency and install a logger built on the new
backend through `spdlog::set_default_logger()`, or accept that those lines land
on spdlog's built-in console logger.

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
config.name = "pipeline";                 // what %n prints, and the registry's name
config.level = logforge::Level::info;
config.console = true;                    // a colour sink on stdout
config.file_path = "logs/pipeline.log";   // empty: no file
config.max_file_bytes = 10 * 1024 * 1024; // 0: one file, no rotation
config.max_files = 5;
config.create_directories = true;         // make "logs" before opening the file
config.udp_host = "10.0.0.7";             // empty: no UDP sink
config.udp_port = 5140;
config.pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v";

logforge::SpdlogBackend backend(config);  // strict: a sink that fails throws
backend.install_as_default();             // both defaults the process has
```

A program that must come up even when a destination does not opens the backend
one sink at a time:

```cpp
logforge::SpdlogBackend::Report report;
std::unique_ptr<logforge::SpdlogBackend> backend =
    logforge::SpdlogBackend::open(config, &report);
if (!backend) {
    // nothing could be built at all; StderrLogger is the last resort
}
if (!report.file.empty()) {
    LOGFORGE_LOG_WARN("file sink dropped: {}", report.file);
}
```

`open()` is the same backend with two differences: a sink that cannot be opened
is named in `report` and the others are kept, and `nullptr` comes back only when
no destination at all could be built. It does not make the degradation quiet for
you -- printing `report.file` is the caller's job, because only the caller knows
where a warning can still be seen.

`install_as_default()` sets both defaults a process has: logforge's (macros,
ambient) and spdlog's registry (its free functions, and libraries that log
behind the caller's back). Two calls were easy to make one of, and the
half-installed state is invisible until the file is missing lines.

The sinks are spdlog's, so a line written to a rotation, a colour console and a
UDP collector at once costs nothing extra. `backend.native()` hands out the
underlying `spdlog::logger` for everything this struct does not cover -- named
loggers, custom sinks, spdlog's backtrace buffer; `native_shared()` is the same
logger as the `shared_ptr` spdlog's registry takes, for a program that installs
it by hand.

A record's `component` becomes a `"component: "` prefix on the message body, the
same text `StderrLogger` produces, so changing backends does not change the
message body. The pattern still decorates it in the backend's own spelling --
spdlog prints `%l` as `warning`, while `to_string(Level::warn)` is `warn`.
Source location goes to spdlog as a `source_loc`, which is what makes
`%s`/`%g`/`%#` work.

## Options

| Option | Default (top level) | Meaning |
|---|---|---|
| `LOGFORGE_BUILD_TESTS` | `ON` | Build the unit tests |
| `LOGFORGE_BUILD_SPDLOG` | `ON` | Build the `logforge::spdlog` backend. `OFF` also drops spdlog from the package config, so a consumer needs no spdlog at all |
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
