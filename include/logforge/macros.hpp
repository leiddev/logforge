#ifndef LOGFORGE_MACROS_HPP
#define LOGFORGE_MACROS_HPP

#include <logforge/ambient.hpp>
#include <logforge/format.hpp>
#include <logforge/level.hpp>
#include <logforge/logger.hpp>

// The macro layer.  Everything a call site needs in one line:
//
//   LOGFORGE_LOG_INFO("pad {} refused {} message(s)", name, count);
//   LOGFORGE_LOG_TAGGED("udp", ::logforge::Level::warn, "socket closed");
//   LOGFORGE_LOG_AT(element_logger, ::logforge::Level::debug, "tick {}", n);
//
// Two properties are worth stating because they are easy to break later:
//
//   * The level is checked before the arguments are rendered, so the cost of a
//     disabled statement is one call to enabled() and nothing else.  A call site
//     can therefore pass something expensive without guarding it.
//   * The macros expand to a do/while(false) block and name their locals with a
//     logforge_detail_ prefix, so they are safe as the body of an if without
//     braces and do not capture anything from the enclosing scope.

// Source location is captured by default because a log line without a file and
// a line number is much harder to act on.  Defining LOGFORGE_SOURCE_LOCATION to
// 0 removes __FILE__/__LINE__ from the binary entirely -- worth doing for a
// release build of a small embedded target, where the paths are long and the
// flash is short.
#ifndef LOGFORGE_SOURCE_LOCATION
#define LOGFORGE_SOURCE_LOCATION 1
#endif

#if LOGFORGE_SOURCE_LOCATION
#define LOGFORGE_DETAIL_FILE __FILE__
#define LOGFORGE_DETAIL_LINE __LINE__
#else
#define LOGFORGE_DETAIL_FILE 0
#define LOGFORGE_DETAIL_LINE 0
#endif

// The one expansion every public macro funnels through.
//
// The logger and the level are each evaluated exactly once even though both are
// used twice, so a call site may pass an expression without being surprised.
#define LOGFORGE_DETAIL_LOG(logger, tag, level, ...)                                           \
    do {                                                                                       \
        ::logforge::Logger *const logforge_detail_target_ = (logger);                          \
        const ::logforge::Level logforge_detail_level_ = (level);                              \
        if (logforge_detail_target_ != 0 &&                                                    \
            logforge_detail_target_->enabled(logforge_detail_level_)) {                        \
            ::logforge::log_to(logforge_detail_target_, logforge_detail_level_,                \
                               ::logforge::format_message(__VA_ARGS__), (tag),                 \
                               LOGFORGE_DETAIL_FILE, LOGFORGE_DETAIL_LINE);                    \
        }                                                                                      \
    } while (false)

// The logger of the current thread, or the process default.
#define LOGFORGE_LOG(level, ...) LOGFORGE_DETAIL_LOG(::logforge::current(), 0, level, __VA_ARGS__)

// An explicit logger, ignoring the thread binding.  Used where an object owns
// its logger and must not depend on who is calling.
#define LOGFORGE_LOG_AT(logger, level, ...) LOGFORGE_DETAIL_LOG(logger, 0, level, __VA_ARGS__)

// A component tag on the current logger.  The tag reaches the backend as a
// field, so a pattern decides how (and whether) it appears.
#define LOGFORGE_LOG_TAGGED(tag, level, ...)                                                   \
    LOGFORGE_DETAIL_LOG(::logforge::current(), tag, level, __VA_ARGS__)

#define LOGFORGE_LOG_AT_TAGGED(logger, tag, level, ...)                                        \
    LOGFORGE_DETAIL_LOG(logger, tag, level, __VA_ARGS__)

#define LOGFORGE_LOG_TRACE(...) LOGFORGE_LOG(::logforge::Level::trace, __VA_ARGS__)
#define LOGFORGE_LOG_DEBUG(...) LOGFORGE_LOG(::logforge::Level::debug, __VA_ARGS__)
#define LOGFORGE_LOG_INFO(...) LOGFORGE_LOG(::logforge::Level::info, __VA_ARGS__)
#define LOGFORGE_LOG_WARN(...) LOGFORGE_LOG(::logforge::Level::warn, __VA_ARGS__)
#define LOGFORGE_LOG_ERROR(...) LOGFORGE_LOG(::logforge::Level::error, __VA_ARGS__)
#define LOGFORGE_LOG_CRITICAL(...) LOGFORGE_LOG(::logforge::Level::critical, __VA_ARGS__)

#endif // LOGFORGE_MACROS_HPP
