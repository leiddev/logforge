#ifndef LOGFORGE_SPDLOG_BACKEND_HPP
#define LOGFORGE_SPDLOG_BACKEND_HPP

#include <logforge/level.hpp>
#include <logforge/logger.hpp>

#include <cstddef>
#include <memory>
#include <string>

// spdlog is behind this file, not in it: forward declaring the logger keeps
// spdlog's headers (and its fmt) out of every translation unit that only wants to
// install a backend, and keeps the public surface free of a specific spdlog
// version.
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
        Config() : level(Level::info), console(true), max_file_bytes(10 * 1024 * 1024),
                   max_files(5), udp_port(0) {}

        Level level;

        // A colour sink on stdout.
        bool console;

        // An empty path means no file.  A non-zero max_file_bytes rotates; zero
        // writes one file and never rotates.
        std::string file_path;
        std::size_t max_file_bytes;
        std::size_t max_files;

        // An empty host means no UDP sink.  The datagram payload is the sink's
        // own pattern, which spdlog defaults to the date, level and message.
        std::string udp_host;
        unsigned short udp_port;

        // spdlog pattern applied to every sink; empty keeps the default of
        // "[date time] [level] message".
        std::string pattern;
    };

    SpdlogBackend();
    explicit SpdlogBackend(const Config &config);

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
    std::unique_ptr<Impl> impl_;
};

} // namespace logforge

#endif // LOGFORGE_SPDLOG_BACKEND_HPP
