# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Nothing yet.

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
