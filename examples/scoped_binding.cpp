// Two objects, one process, one logger each -- without either object knowing
// anything about the other's.
//
// This is the pattern a runtime uses when it hands every element its own logger:
// bind it for the duration of the callback, and code inside the callback that
// knows nothing about the runtime still writes to the right place.
//
// Expected output (stderr):
//
//   [debug] collector: tick 1
//   [debug] emitter: tick 1
//   [warn] collector: buffer full

#include <logforge/ambient.hpp>
#include <logforge/macros.hpp>
#include <logforge/stderr_logger.hpp>

#include <string>

namespace {

class Pad {
public:
    Pad(const std::string &name, logforge::Logger *logger) : name_(name), logger_(logger) {}

    // A callback: the binding makes every log statement in this scope -- here and
    // in anything this calls -- go to this pad's logger.
    void tick(int count) {
        const logforge::ScopedBinding binding(logger_);
        LOGFORGE_LOG_TAGGED(name_.c_str(), logforge::Level::debug, "tick {}", count);
        if (count > 0) {
            LOGFORGE_LOG_TAGGED(name_.c_str(), logforge::Level::warn, "buffer full");
        }
    }

private:
    std::string name_;
    logforge::Logger *logger_;
};

} // namespace

int main() {
    // A process default, for anything that is not inside a callback.
    logforge::StderrLogger fallback(logforge::Level::info);
    logforge::set_default(&fallback);

    // One backend per object, all of them writing to stderr in this example; in a
    // real program each could be a file, or all of them the same SpdlogBackend.
    logforge::StderrLogger collector_logger(logforge::Level::debug);
    logforge::StderrLogger emitter_logger(logforge::Level::debug);

    Pad collector("collector", &collector_logger);
    Pad emitter("emitter", &emitter_logger);

    collector.tick(1);
    emitter.tick(0);
    collector.tick(2);

    return 0;
}
