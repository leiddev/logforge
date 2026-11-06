// The spdlog backend: a rotating file, a colour console and, when a host is
// given on the command line, a UDP forward.
//
//   logforge_backend_configuration                 # console + file
//   logforge_backend_configuration 10.0.0.7 5140   # ... and UDP
//
// The file lands next to the executable:
//
//   [2026-11-06 12:00:00.000] [info] [backend_configuration.cpp:37] pipeline started
//   [2026-11-06 12:00:00.001] [warning] [backend_configuration.cpp:38] latency 12.5 ms
//
// One line goes to all three sinks; only the pattern decides how it reads.

#include <logforge/macros.hpp>
#include <logforge/spdlog_backend.hpp>

#include <cstdlib>
#include <cstring>

int main(int argc, char **argv) {
    logforge::SpdlogBackend::Config config;
    config.level = logforge::Level::debug;
    config.console = true;
    config.file_path = "logforge_example.log";
    config.max_file_bytes = 1024 * 1024;
    config.max_files = 3;
    config.pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v";

    if (argc > 2) {
        config.udp_host = argv[1];
        config.udp_port = static_cast<unsigned short>(std::atoi(argv[2]));
    }

    logforge::SpdlogBackend backend(config);
    logforge::set_default(&backend);

    LOGFORGE_LOG_INFO("pipeline started");
    LOGFORGE_LOG_WARN("latency {:.1f} ms", 12.5);
    LOGFORGE_LOG_TAGGED("udp", logforge::Level::debug, "forwarded {} bytes", 512);

    // The backend owns its sinks; flushing before the object goes away is only
    // strictly needed because this program exits right after.
    backend.flush();
    logforge::set_default(0);

    return 0;
}
