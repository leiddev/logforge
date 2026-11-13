# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.4.2] - 2026-11-13

The suite grows the failure paths it was missing, and one of them found a real
defect on MSVC.

### Added

- `tests/format_boundary_test.cpp`: the edges of the renderer. A width of exactly
  512 is accepted and 513 is copied back as written; the precision limit the same
  way; hex with a width, with and without zero padding; zero padding around a
  sign; an unclosed `{{`, a stray `}` and an empty `{}`; and the specifiers that
  parse in part and must still be rejected whole. Two behaviours the tests record
  as they are: `"{7}"` is a width-only specifier and not a literal, and an
  unclosed `"{{"` at the end of a message still collapses to one brace.
- `tests/legacy_names_test.cpp`: the compat header's `#ifndef` guards, which are
  its whole reason for being a separate header. The test defines `LOG_INFO` and
  `LOG_WARN` itself before the include and checks that its own definitions
  survive; it also pins `LOG_ERR` to error and `LOG_CRITICAL` to critical, which
  a ported call site is likely to assume are synonyms.
- `tests/spdlog_forward_decl_test.cpp`: a translation unit that includes
  `<logforge/spdlog_backend.hpp>` and nothing else of spdlog's, compiled without
  a single spdlog include directory on its compile line. The forward declaration
  in that header is a promise no other test can check, because every other test
  includes spdlog itself. Verified by adding an `#include <spdlog/spdlog.h>` to
  the header and watching this target, and only this target, fail to compile.
- `macro_test.cpp`: each of the logger and the level is evaluated once; the
  untested `LOGFORGE_LOG_AT_TAGGED`; a null tag reaching the backend as null; a
  disabled statement that does not call `log()`; and a message with fewer
  arguments than braces still arriving as one record.
- `ambient_test.cpp`: a `ScopedBinding` restored when its scope throws, and a
  nested pair unwound by one exception.
- `spdlog_backend_test.cpp`: `create_directories` for a bare file name, a relative
  path, and a directory that already exists; the strict constructor throwing on
  exactly the configuration `open()` reports and survives, for both a file and a
  UDP destination; a UDP destination missing its host or its port being no sink at
  all; `open()` with a null `Report`; one record reaching a console and a file at
  once; the component prefix and the empty-component case; an embedded zero; a
  null text being dropped; a destroyed backend leaving a *surviving* default
  alone; spdlog keeping the logger alive after the backend is gone; and four
  threads whose component prefixes must not mix.

### Fixed

- `logforge_build_flags` did not pass `/EHsc` to MSVC, so the build used MSVC's
  default `/EHs-`, where a local object is not destroyed while an exception
  unwinds. `ScopedBinding`'s destructor is exactly such an object, so its
  "restore the previous logger on every exit path, including an exception" held
  on GCC and Clang and silently did not on Windows -- a thread that threw out of
  a bound scope kept writing to the logger it had bound. The new ambient tests
  found it before the library shipped; `/EHsc` is now part of the flags target.

### Changed

- `set_pattern("")` is documented and tested for what it does rather than what it
  sounds like: spdlog compiles the empty string into a formatter with no fields,
  so the message body is dropped and only the pattern's newline is written.
  `Config::pattern` still treats empty as "keep the default", and the asymmetry
  between the two is now in the suite.
- The test suite runs on the Windows and Linux builds; it was also run on a
  Hi3516 (Cortex-A7, glibc 2.24) target for the behaviours that do not need
  GoogleTest.

## [1.4.1] - 2026-11-10

Documentation, and the comments and example that had outlived their milestone.
No API, behaviour or build change.

### Added

- README: *Why not spdlog directly* puts what a consumer gets against what it
  gives up, and says when spdlog itself is the better choice. *Where spdlog is
  allowed to appear* splits the boundary into headers, CMake targets, process
  state and package config -- including the one that was never written down: a
  package built with the backend runs `find_dependency(spdlog)` even when the
  consumer links only `logforge::core`. *What the spdlog backend uses* lists the
  API surface and what it leaves alone, so a replacement backend is a known
  amount of work. *Replacing the backend* is the checklist, with the part a
  swap does not fix -- dependencies that log through spdlog's global logger.
- `docs/LOGGING.md`: the backend contract states that `log()` is called
  concurrently and that `enabled()` must agree with what `log()` writes, and
  explains why the destructor clears logforge's default but not spdlog's.

### Changed

- The first README bullet named `log(level, text, size)`, which is not the
  interface, and said the core "includes nothing", which reads as "no standard
  headers". Both now describe the code as it is.
- The README no longer says a backend swap changes nothing but where a line
  goes: the body is unchanged, but a pattern's `%l` is spdlog's `warning` where
  `to_string(Level::warn)` is `warn`.

### Fixed

- `spdlog_backend.hpp` claimed a public surface free of a specific spdlog
  version, while `native()` and `native_shared()` name `spdlog::logger`. The
  comment now says what the forward declaration actually buys.
- `examples/backend_configuration.cpp` installed its backend with
  `set_default()` and cleared it by hand, instead of the `install_as_default()`
  the documentation recommends since 1.4.0.
- `CMakeLists.txt` still described the backend as "added in a later milestone"
  and as joining the export in one.

## [1.4.0] - 2026-11-09

### Added

- `SpdlogBackend::open(config, report)` builds the same backend one destination at
  a time. A sink that cannot be opened is named in the `Report` and the others are
  kept; the result is null only when no destination at all could be built, and the
  constructor's console fallback is kept. This is the shape a daemon needs -- it
  must come up and say what it lost -- and it was 50 lines of ladder in a consumer
  before, with the failure swallowed: the process kept printing that it was writing
  `logs/xec-log.txt` after dropping that sink. `open()` does not print the report
  itself, because only the caller knows where a warning can still be seen.
- `SpdlogBackend::Config::create_directories` creates the directories that contain
  `file_path`, outermost first, before the file is opened. No spdlog sink does
  this, so every program that ships `logs/pipeline.log` in its configuration either
  wrote the platform-specific `mkdir` walk itself or died on its first start. Off
  by default: a library should not create directories it was not asked to create.
- `SpdlogBackend::install_as_default()` installs the backend as both defaults a
  process has: logforge's (macros, ambient binding) and spdlog's registry (its free
  functions, and libraries that log through spdlog's global logger). Doing only
  the first was a half-install that showed up as missing lines in the file and
  nothing else.

### Changed

- A `SpdlogBackend` that is the process default clears logforge's default when it
  is destroyed, instead of leaving a pointer to a dead logger behind while the
  program shuts down.

## [1.3.1] - 2026-11-06

### Fixed

- `SpdlogBackend::Config`'s constructor initialised `console` before `name`, the
  reverse of their declaration order. Compilers that warn about that with
  `-Wreorder` -- GCC does, MSVC does not -- rejected the header under `-Werror`,
  so 1.3.0 did not build for consumers with warnings as errors. The member order is
  unchanged; the initialiser list now follows it.

## [1.3.0] - 2026-11-06

### Added

- `SpdlogBackend::Config::name` sets the name of the underlying spdlog logger,
  defaulting to `"logforge"`. It is what the registry knows the logger by and what
  a pattern's `%n` prints. The alternative a consumer had was renaming the logger
  after construction, which spdlog does not offer at any version this backend
  supports, so a program that keeps its logger name in configuration had to build
  the spdlog logger itself and lose everything else this class does. An empty name
  falls back to the default rather than producing a logger spdlog cannot identify.

## [1.2.0] - 2026-11-06

### Added

- `SpdlogBackend::native_shared()` returns the underlying `spdlog::logger` as a
  `shared_ptr`, the form `spdlog::set_default_logger()` takes. It is the bridge for
  a program that still has raw spdlog call sites, or that depends on a library
  logging through spdlog's global logger: those lines now reach the sinks this
  backend configured instead of spdlog's built-in default, which is a real
  regression when a dependency logs behind the caller's back. The shared pointer
  keeps the logger alive past the backend, so spdlog can be left holding it.
- `spdlog_backend_test.cpp` also checks the level table end to end: every level,
  trace to critical, rendered through `%l`, so a mapping one step off -- which
  writes a line under the wrong name and tells no one -- fails a test. The check
  was seven assertions in the library this backend was extracted from and did not
  come over with the code. Tests are not part of the installed package.

### Changed

- The spdlog backend is used from an existing `spdlog::spdlog` target when the
  surrounding build already has one, with no `find_package` and nothing
  downloaded. That is how a project that vendors spdlog consumes this package.
- The documented spdlog floor is 1.10, not 1.15. The UDP sink has had its
  config-struct constructor since 1.10 and nothing else in the backend is newer;
  the earlier floor came from a changelog rather than from the code. A vendored
  target is not version checked at all: the build fails at the first missing
  header, which is a clearer message than a version comparison.

## [1.1.0] - 2026-11-06

### Added

- `logforge::set_current(logger)`: binds a logger to the calling thread without a
  scope and returns the previous one. `ScopedBinding` is now defined in terms of
  it, so the two can never disagree about which slot they write. The function
  exists for the two callers that cannot use a scope -- a compatibility shim that
  has to keep a "set and hand back the previous logger" signature, and a test that
  wants to leave the thread unbound.

## [1.0.0] - 2026-11-06

The API is frozen. Rendering, the interface, the macros and the spdlog backend
are what a producer and a backend can rely on from here; a breaking change needs
a major version.

### Added

- `docs/LOGGING.md`: the shape of a log line, the interface and its synchronous
  contract, where a line goes (thread binding, process default, lifetime and
  thread rules), macro and specifier tables, backend configuration, how to write
  a backend, level-name parsing, and a migration table from a pipeforge-style
  layer, from raw spdlog and from an adapter that already holds a string.
- Examples, built and executed by CI: `quickstart`, `scoped_binding` (per-object
  routing) and `backend_configuration` (rotating file, console, optional UDP).
- `LOGFORGE_BUILD_EXAMPLES` option, on by default at the top level.

### Changed

- A build that fetches spdlog installs it into the same prefix, so
  `find_package(logforge)` followed by linking `logforge::spdlog` works out of the
  box.

## [0.4.0] - 2026-11-06

The spdlog backend. A record now has somewhere to go that rotates, colours and
forwards over UDP.

### Added

- `logforge::SpdlogBackend`, a `Logger` implementation behind a `Config` struct:
  minimum level, colour console, plain or rotating file, UDP, and a pattern.
  pimpl, so no spdlog header is required to install a backend and no spdlog
  version is baked into the public surface.
- `native()` for the underlying `spdlog::logger`, `set_pattern()` and `flush()`.
- The record is handed to spdlog through its `string_view` overload, so a message
  that the renderer already produced is never parsed by fmt a second time.
- `component` becomes a `"component: "` prefix on the message body -- identical
  to what `StderrLogger` writes, so the two backends say the same thing.
- Source location travels as a `source_loc`, which makes `%s`/`%g`/`%#` work.
- `logforge::parse_level()`, a case-insensitive reader for configuration text
  accepting the canonical names plus `warning`, `err` and `fatal`. It leaves the
  caller's default untouched when the text is not a level, so a typo cannot
  silently lower the verbosity.
- Tests: level filtering including `off`, pattern output, component prefixing,
  source location in the pattern, pattern changes after construction, the native
  handle, and an empty configuration falling back to a sink.

### Changed

- `logforge::spdlog` requires spdlog 1.15 or newer (the UDP sink takes a config
  struct from that release).
- A build that fetches spdlog also installs it: `SPDLOG_INSTALL` is forced on,
  because a prefix containing only logforge cannot satisfy the exported backend's
  `spdlog::spdlog` usage requirement.

## [0.3.0] - 2026-11-06

The renderer and the macros. A call site can now write
`LOGFORGE_LOG_INFO("pad {} refused {}", name, count)` and have it filtered,
rendered and delivered.

### Added

- `logforge::format_message()`, a non-throwing renderer for `{}` text with
  `{:4}`, `{:x}`, `{:X}`, `{:04x}`, `{:.2f}`, `{:7.2f}` and `{{`/`}}`. A
  specifier that does not apply to its argument is ignored; a malformed one is
  copied verbatim; a message with fewer arguments than braces keeps the braces it
  cannot fill. Nothing is truncated and nothing throws -- a mistake shows up in
  the text, which is what an element thread needs.
- Floats default to the shortest of 15 and 17 significant digits that reads back
  as the same value, matching the JSON writer's spelling rather than fmt's
  shortest-of-all.
- A pointer that is not text prints as `0x...`; a null `const char*` prints
  `(null)`; a null format text renders as the empty message.
- `LOGFORGE_LOG_*` macros for the six levels, plus `LOGFORGE_LOG(level, ...)`,
  `LOGFORGE_LOG_AT(logger, ...)`, `LOGFORGE_LOG_TAGGED(tag, ...)` and
  `LOGFORGE_LOG_AT_TAGGED(...)`. The level is checked before the arguments are
  rendered, so a disabled statement costs one virtual call; the logger and the
  level expression are each evaluated once.
- `LOGFORGE_SOURCE_LOCATION=0` compiles `__FILE__`/`__LINE__` out of the binary.
- `logforge/compat/legacy_names.hpp`, an opt-in header defining the bare
  `LOG_TRACE`/`LOG_DEBUG`/`LOG_INFO`/`LOG_WARN`/`LOG_ERR`/`LOG_CRITICAL` names.
  It is kept out of the umbrella header on purpose: those names belong to glog
  and syslog, and a `#ifndef` guard turns include order into silent behaviour.
- 62 assertions covering the renderer (ported from pipeforge's `log_text_test`),
  macro level mapping, disabled-statement evaluation, per-object routing, tags,
  the legacy aliases, and a target built with source location compiled out.

## [0.2.0] - 2026-11-06

The core interface. A producer can now emit a record and a backend can receive
one; rendering and macros follow in 0.3.0.

### Added

- `logforge::Logger`, the two-virtual interface every backend implements:
  `enabled(Level)` for cheap filtering and `log(const Record&)` for writing.
- `logforge::Record`, an aggregate carrying the level, the rendered text as
  `(pointer, size)` and optional component/source-location metadata. No
  ownership, no allocation, no default member initialisers (which would break
  aggregate initialisation in C++11).
- `log_to()` overloads so callers do not have to spell out `Record`.
- `logforge::NullLogger` and `logforge::StderrLogger`. The latter emits each
  record with a single `fwrite()` -- whole lines under concurrency without a
  mutex -- and never touches a 512-byte stack buffer's limits: long records fall
  back to an exact-size path instead of being truncated.
- `logforge::current()`, `set_default()`, `default_logger()` and
  `ScopedBinding`, a thread-local binding with nested unwinding. Binding nothing
  falls back to the process default; binding a `NullLogger` is how a scope is
  silenced on purpose.
- `logforge/logforge.hpp`, the umbrella header for the core only -- the backend
  stays behind its own include so producers do not compile spdlog.
- Tests: interface/record/ownership, thread isolation of the binding, and a
  descriptor-level stderr capture that restores fd 2 so GoogleTest output keeps
  going to the console.

### Changed

- `Level::off` is never writable, regardless of the configured minimum: it is
  the setting that silences logging, not a severity.

## [0.1.0] - 2026-11-06

First milestone: the repository builds, installs and is consumable through
`find_package(logforge)`.

### Added

- `logforge::core`, a header-only, dependency-free target exporting
  `logforge::Level` and `to_string(Level)`.
- `logforge/version.hpp`, generated at configure time so the headers cannot
  disagree with the CMake project version.
- CMake install and export rules (`logforgeTargets.cmake`,
  `logforgeConfig.cmake`, `SameMajorVersion`).
- Unit test harness with optional GoogleTest lookup or `FetchContent` fallback.
- `tests/consumer`, a standalone project built against an install prefix in CI.
- GitHub Actions matrix: Linux GCC/Clang across Debug and Release, MSVC on
  Windows, ASan+UBSan, warnings-as-errors, and the installed-package consumer.
