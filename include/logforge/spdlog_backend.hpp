#ifndef LOGFORGE_SPDLOG_BACKEND_HPP
#define LOGFORGE_SPDLOG_BACKEND_HPP

#include <logforge/level.hpp>
#include <logforge/logger.hpp>

#include <cstddef>
#include <memory>
#include <string>

// spdlog is behind this file, not in it: the header forward declares
// spdlog::logger instead of including it, so a translation unit that only
// installs a backend compiles no spdlog and no fmt header, and no spdlog version
// is baked into the ABI.  The type still appears in native() and native_shared()
// -- that is the escape hatch, and a caller that uses them includes spdlog
// itself.
namespace spdlog {
class logger;
}

namespace logforge {

// The backend most programs want: spdlog's sinks, patterns and level filtering,
// driven by a struct instead of by callbacks.
//
// The message a sink receives is the record's text, prefixed with "component: "
// when the record carries one.  That is the same body StderrLogger produces, so
// swapping backends does not change what a log line says -- only where it goes
// and how it is decorated.
class SpdlogBackend : public Logger {
public:
    struct Config {
        // Records below this level are dropped by enabled().  Level::off turns
        // logging off completely.
        Config()
            : level(Level::info), name("logforge"), console(true), max_file_bytes(10 * 1024 * 1024),
              max_files(5), create_directories(false), udp_port(0) {
        }

        Level level;

        // What spdlog's registry knows the logger by, and what a pattern's %n
        // prints.  It is a label, not a destination: two backends with the same
        // name write to their own sinks, and the registry keeps whichever was
        // installed last as the default.  A consumer whose configuration has a
        // logger name -- most do -- passes it here instead of renaming the
        // logger afterwards, which spdlog does not offer.
        std::string name;

        // A colour sink on stdout.
        bool console;

        // An empty path means no file.  A non-zero max_file_bytes rotates; zero
        // writes one file and never rotates.
        std::string file_path;
        std::size_t max_file_bytes;
        std::size_t max_files;

        // Create the directories that contain file_path, outermost first, before
        // the file is opened.  No spdlog sink does that, so a program that ships
        // "logs/pipeline.log" in its configuration either creates "logs" itself or
        // dies on its first start.  Best effort: a directory that cannot be made
        // is reported by the sink that then cannot open the file.
        bool create_directories;

        // An empty host means no UDP sink.  The datagram payload is the sink's
        // own pattern, which spdlog defaults to the date, level and message.
        std::string udp_host;
        unsigned short udp_port;

        // spdlog pattern applied to every sink; empty keeps the default of
        // "[date time] [level] message".
        std::string pattern;
    };

    // What open() could not build.  An empty member means "built", or "not asked
    // for in the configuration"; the strings are the exceptions spdlog threw, which
    // is the most specific thing a program can put in a warning.
    struct Report {
        Report() {
        }

        std::string console;
        std::string file;
        std::string udp;

        bool empty() const {
            return console.empty() && file.empty() && udp.empty();
        }
    };

    SpdlogBackend();
    explicit SpdlogBackend(const Config &config);

    // The same backend, but one destination at a time: a sink that cannot be
    // opened is named in *report and the others are kept.  The result is null only
    // when no destination could be built at all; StderrLogger is the usual answer
    // then.
    //
    // The constructor is still the strict path: a configuration that should stop
    // the program is a broken configuration, and an exception at startup says so.
    // Use open() in a daemon that must come up with what it can.
    static std::unique_ptr<SpdlogBackend> open(const Config &config, Report *report = 0);

    // Installs this backend as both defaults a process has: logforge's (the
    // macros and the ambient binding) and spdlog's (its free functions, which
    // libraries that log behind the caller's back use).  Missing the second one is
    // a silent regression -- those lines go to spdlog's built-in console logger
    // and never reach the file the configuration asked for.
    //
    // The backend must outlive the process default; a program installs one backend
    // at startup and keeps it.  If it does not, the destructor clears logforge's
    // default again rather than leaving a dangling pointer behind.
    void install_as_default();

    // Backends are shared with the rest of the process through the Logger
    // interface, so they must not be copied.
    SpdlogBackend(const SpdlogBackend &) = delete;
    SpdlogBackend &operator=(const SpdlogBackend &) = delete;

    ~SpdlogBackend() override;

    bool enabled(Level level) const override;
    void log(const Record &record) override;

    // Writes whatever the sinks are holding.  Called by record dispatch only when
    // a sink is configured to flush; the tests and a shutdown path call it
    // directly.
    void flush();

    // Changes the pattern of every sink.
    void set_pattern(const std::string &pattern);

    // The escape hatch: the underlying spdlog logger, for the features that only
    // spdlog has (named loggers, custom sinks, spdlog's own backtrace buffer).
    // The pointer is owned by this object and lives as long as it does.
    spdlog::logger *native() const;

    // The same logger as a shared pointer, the form spdlog's registry takes.
    // This is the bridge for a program that still has raw spdlog calls -- a
    // translation unit that has not been ported, a library that logs through
    // spdlog's global logger -- and wants those lines on these sinks rather than
    // on spdlog's built-in default:
    //
    //     spdlog::set_default_logger(backend.native_shared());
    //
    // spdlog then owns a reference, so the logger outlives this object and a
    // line in flight cannot reach freed memory.
    std::shared_ptr<spdlog::logger> native_shared() const;

private:
    struct Impl;

    // Takes over a partially built backend; open() is the only caller.
    explicit SpdlogBackend(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

} // namespace logforge

#endif // LOGFORGE_SPDLOG_BACKEND_HPP
