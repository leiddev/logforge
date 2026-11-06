#ifndef LOGFORGE_LOGGER_HPP
#define LOGFORGE_LOGGER_HPP

#include <logforge/level.hpp>

#include <cstddef>
#include <string>

namespace logforge {

// One log line, already rendered.  The backend receives it as a value and is
// expected to write it before returning: nothing here owns memory, and nothing
// here may outlive the call.
//
// Deliberately an aggregate without default member initialisers -- in C++11 an
// NSDMI would stop it from being an aggregate, and brace initialisation at every
// construction site is what keeps this file free of a constructor that would
// have to be exported from a header-only library.
struct Record {
    Level level;
    const char *text;
    std::size_t size;
    // Optional tags.  Borrowed, not owned: they must stay valid for the length of
    // the call, which is all a synchronous backend may rely on.  A caller with a
    // std::string passes c_str() and keeps the string alive across the statement.
    const char *component; // e.g. "udp" or an element name
    const char *file;      // optional source location
    int line;              // 0 when unknown
};

inline Record make_record(Level level, const char *text, std::size_t size,
                          const char *component = 0, const char *file = 0, int line = 0) {
    Record record;
    record.level = level;
    record.text = text;
    record.size = size;
    record.component = component;
    record.file = file;
    record.line = line;
    return record;
}

// The interface every producer talks to and every backend implements.
//
// Two virtuals, and that is the whole design: `enabled()` so that a disabled
// call site never evaluates its arguments or renders a message, and `log()` so
// that a backend only ever has to write text somewhere.
//
// There is intentionally no std::string overload here.  A derived class that
// overrides `log()` would hide it, and the resulting "no matching function"
// error at a call site is far worse than the small convenience it buys; the
// free functions below cover that case without the hiding problem.
class Logger {
public:
    virtual ~Logger() {}

    // True when a record at this level would be written.  Must be cheap and
    // thread safe: it is called on every hot-path log statement.
    virtual bool enabled(Level level) const = 0;

    // Synchronous.  When this returns the caller may free `record.text` and the
    // metadata strings; an asynchronous backend has to copy what it keeps.
    virtual void log(const Record &record) = 0;
};

// Free-function entry points.  Their purpose is to spare callers (and macros)
// the brace-initialisation noise while keeping the virtual surface minimal.
inline void log_to(Logger *logger, const Record &record) { logger->log(record); }

inline void log_to(Logger *logger, Level level, const char *text, std::size_t size,
                   const char *component = 0, const char *file = 0, int line = 0) {
    logger->log(make_record(level, text, size, component, file, line));
}

inline void log_to(Logger *logger, Level level, const std::string &text,
                   const char *component = 0, const char *file = 0, int line = 0) {
    logger->log(make_record(level, text.data(), text.size(), component, file, line));
}

} // namespace logforge

#endif // LOGFORGE_LOGGER_HPP
