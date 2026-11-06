#ifndef LOGFORGE_STDERR_LOGGER_HPP
#define LOGFORGE_STDERR_LOGGER_HPP

#include <logforge/logger.hpp>

#include <cstdio>
#include <cstring>
#include <string>

namespace logforge {

// A last-resort backend: no sinks, no rotation, no configuration.  It exists so
// that a program -- or a library's own tests -- can ask for the lines without
// pulling in spdlog, and it is what a caller installs while the real backend is
// still being configured.
//
// Each record is emitted with exactly one fwrite().  glibc and the MSVC CRT both
// hold the FILE* lock for the duration of an fwrite, so concurrent writers
// produce whole lines rather than interleaved halves.  That is the strongest
// guarantee available without dragging <mutex> -- and with it a threads
// dependency -- into a header-only core.
//
// stderr is unbuffered, so a line that was written survives a crash.  That is
// the other reason this logger exists.
class StderrLogger : public Logger {
public:
    explicit StderrLogger(Level min_level = Level::info) : min_level_(min_level) {}

    // `off` is not a severity that can be logged, it is the setting that turns
    // logging off, so a record at that level is never written -- not even when
    // the minimum is trace.
    bool enabled(Level level) const override { return level != Level::off && level >= min_level_; }

    void log(const Record &record) override {
        if (record.text == 0 || !enabled(record.level)) {
            return;
        }

        char line[512];
        const std::size_t used = render(line, sizeof line, record);
        if (used != 0) {
            std::fwrite(line, 1, used, stderr);
            return;
        }

        // Did not fit the stack buffer: build it exactly and still emit it in
        // one call.  Long lines are rare enough not to care about the allocation.
        std::string wide;
        wide.reserve(record.size + 32);
        wide.push_back('[');
        wide.append(to_string(record.level));
        wide.append("] ");
        if (record.component != 0 && record.component[0] != '\0') {
            wide.append(record.component);
            wide.append(": ");
        }
        wide.append(record.text, record.size);
        wide.push_back('\n');
        std::fwrite(wide.data(), 1, wide.size(), stderr);
    }

private:
    // Returns the number of bytes written to `out`, or 0 when the record does
    // not fit.  snprintf() reports the length it would have produced, which is
    // what makes "does not fit" detectable instead of silently truncated.
    static std::size_t render(char *out, std::size_t capacity, const Record &record) {
        int written;
        if (record.component != 0 && record.component[0] != '\0') {
            written = std::snprintf(out, capacity, "[%s] %s: ", to_string(record.level),
                                    record.component);
        } else {
            written = std::snprintf(out, capacity, "[%s] ", to_string(record.level));
        }
        if (written < 0) {
            return 0;
        }

        const std::size_t prefix = static_cast<std::size_t>(written);
        if (prefix + record.size + 1 > capacity) {
            return 0;
        }

        std::memcpy(out + prefix, record.text, record.size);
        out[prefix + record.size] = '\n';
        return prefix + record.size + 1;
    }

    Level min_level_;
};

} // namespace logforge

#endif // LOGFORGE_STDERR_LOGGER_HPP
