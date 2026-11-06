#ifndef LOGFORGE_LEVEL_HPP
#define LOGFORGE_LEVEL_HPP

#include <cstddef>

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

namespace detail {

// Case insensitive comparison against a lower case literal.  Written out rather
// than pulled from <cctype> so that it cannot depend on the current locale -- a
// config file that says INFO means info in every locale.
inline bool equals_ignore_case(const char *text, const char *lower) {
    for (std::size_t index = 0;; ++index) {
        const char left = text[index];
        const char right = lower[index];
        if (right == '\0') {
            return left == '\0';
        }
        if (left == '\0') {
            return false;
        }
        char folded = left;
        if (folded >= 'A' && folded <= 'Z') {
            folded = static_cast<char>(folded - 'A' + 'a');
        }
        if (folded != right) {
            return false;
        }
    }
}

} // namespace detail

// Reads a level name out of configuration text.
//
// The names this accepts are the ones that have appeared in configuration files
// over the years: the seven canonical names, plus the aliases people actually
// type -- "warning", "err" and "fatal".  Matching is case insensitive and the
// whole string has to match, so a typo fails instead of guessing.
//
// `out` is only written when the text names a level, which lets a caller keep a
// default without saving and restoring it around the call.
inline bool parse_level(const char *text, Level &out) {
    if (text == 0) {
        return false;
    }

    struct Named {
        const char *name;
        Level level;
    };
    static const Named names[] = {{"trace", Level::trace},
                                  {"debug", Level::debug},
                                  {"info", Level::info},
                                  {"warn", Level::warn},
                                  {"warning", Level::warn},
                                  {"error", Level::error},
                                  {"err", Level::error},
                                  {"critical", Level::critical},
                                  {"fatal", Level::critical},
                                  {"off", Level::off}};

    for (std::size_t index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (detail::equals_ignore_case(text, names[index].name)) {
            out = names[index].level;
            return true;
        }
    }
    return false;
}

} // namespace logforge

#endif // LOGFORGE_LEVEL_HPP
