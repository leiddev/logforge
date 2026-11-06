#ifndef LOGFORGE_COMPAT_LEGACY_NAMES_HPP
#define LOGFORGE_COMPAT_LEGACY_NAMES_HPP

#include <logforge/macros.hpp>

// Opt-in aliases for the bare LOG_* names a ported file already uses:
//
//   #include <logforge/compat/legacy_names.hpp>
//
//   LOG_INFO("element {} started on port {}", name, port);
//
// They are deliberately not part of <logforge/macros.hpp>.  LOG_INFO, LOG_WARN
// and friends belong to glog, syslog and several embedded frameworks, and a
// library that defines them from its umbrella header starts a quiet war with
// whatever the including project already uses: the #ifndef guards below mean the
// winner depends on include order, which is the worst possible failure mode
// because it produces no diagnostic at all.
//
// A macro the including file already defines is left alone, so this header never
// fights a definition of its own.
//
// LOG_ERR reports at error, which is the legacy name for that level.
// LOG_CRITICAL reports at critical, which is a level of its own.

#ifndef LOG_TRACE
#define LOG_TRACE(...) LOGFORGE_LOG_TRACE(__VA_ARGS__)
#endif

#ifndef LOG_DEBUG
#define LOG_DEBUG(...) LOGFORGE_LOG_DEBUG(__VA_ARGS__)
#endif

#ifndef LOG_INFO
#define LOG_INFO(...) LOGFORGE_LOG_INFO(__VA_ARGS__)
#endif

#ifndef LOG_WARN
#define LOG_WARN(...) LOGFORGE_LOG_WARN(__VA_ARGS__)
#endif

#ifndef LOG_ERR
#define LOG_ERR(...) LOGFORGE_LOG_ERROR(__VA_ARGS__)
#endif

#ifndef LOG_CRITICAL
#define LOG_CRITICAL(...) LOGFORGE_LOG_CRITICAL(__VA_ARGS__)
#endif

#endif // LOGFORGE_COMPAT_LEGACY_NAMES_HPP
