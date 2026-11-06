// The smallest useful program: install a backend, log at several levels.
//
// Build it with the library and run it:
//
//   logforge_quickstart
//
// Expected output (stderr, one line per statement that passed the filter):
//
//   [info] service started on port 8080
//   [warn] in: pad in dropped 00ff frames
//   [error] link down after 1.50s
//
// The "in: " on the second line is the component tag; the body is the rendered
// message.  The debug line never appears: the logger's minimum level is info, and
// the argument is not even rendered.

#include <logforge/macros.hpp>
#include <logforge/stderr_logger.hpp>

int main() {
    logforge::StderrLogger logger(logforge::Level::info);
    logforge::set_default(&logger);

    LOGFORGE_LOG_INFO("service started on port {}", 8080);
    LOGFORGE_LOG_DEBUG("this line is filtered out before it is rendered");
    LOGFORGE_LOG_TAGGED("in", logforge::Level::warn, "pad {} dropped {:04x} frames", "in", 255);
    LOGFORGE_LOG_ERROR("link down after {:.2f}s", 1.5);

    return 0;
}
