# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Nothing yet.

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
