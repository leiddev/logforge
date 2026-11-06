# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Nothing yet.

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
