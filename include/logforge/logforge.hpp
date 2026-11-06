#ifndef LOGFORGE_LOGFORGE_HPP
#define LOGFORGE_LOGFORGE_HPP

// Umbrella header for the library.  Includes only the core: the spdlog backend
// is a separate include (<logforge/spdlog_backend.hpp>) so that a producer does
// not have to compile spdlog's headers to write a log line.
#include <logforge/ambient.hpp>
#include <logforge/format.hpp>
#include <logforge/level.hpp>
#include <logforge/logger.hpp>
#include <logforge/macros.hpp>
#include <logforge/null_logger.hpp>
#include <logforge/stderr_logger.hpp>
#include <logforge/version.hpp>

// compat/legacy_names.hpp is deliberately not included here: it defines the bare
// LOG_* names, which belong to glog and syslog and must be opted into.

#endif // LOGFORGE_LOGFORGE_HPP
