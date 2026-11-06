#ifndef LOGFORGE_LEVEL_HPP
#define LOGFORGE_LEVEL_HPP

namespace logforge {

// The seven names pipeforge has always used, kept in the same order so that a
// numeric comparison ("is this line at least a warning?") reads the same way.
// `off` is a real enumerator rather than a sentinel: it is what a configured
// level filter is set to when the operator wants silence, and it makes
// `level >= Level::error` the natural way to ask "is this bad?".
enum class Level { trace, debug, info, warn, error, critical, off };

// A stable spelling per level.  Used for config round-trips, for the spdlog
// backend's pattern and for tests; deliberately not localised.
inline const char *to_string(Level level) {
    switch (level) {
    case Level::trace:
        return "trace";
    case Level::debug:
        return "debug";
    case Level::info:
        return "info";
    case Level::warn:
        return "warn";
    case Level::error:
        return "error";
    case Level::critical:
        return "critical";
    case Level::off:
        return "off";
    }
    // Unreachable with a valid enumerator; keeping the fallback keeps -Wswitch
    // quiet for compilers that warn about a non-covering switch.
    return "off";
}

} // namespace logforge

#endif // LOGFORGE_LEVEL_HPP
