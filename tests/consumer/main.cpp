#include <logforge/macros.hpp>
#include <logforge/spdlog_backend.hpp>
#include <logforge/version.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

// Uses only what an installed logforge exposes: the macros, the spdlog backend
// and the generated version header.  If this compiles, links and runs, then the
// package config, the exported targets, the transitive spdlog dependency, the
// include layout and the install rules are all usable from outside the source
// tree.
int main() {
    const std::string path = "logforge_consumer.log";
    std::remove(path.c_str());

    logforge::SpdlogBackend::Config config;
    config.console = false;
    config.file_path = path;
    config.max_file_bytes = 0;
    config.pattern = "%l|%v";

    logforge::SpdlogBackend backend(config);
    logforge::set_default(&backend);

    LOGFORGE_LOG_INFO("hello from the consumer, {} levels named", 7);
    backend.flush();
    logforge::set_default(0);

    std::ifstream in(path.c_str(), std::ios::binary);
    std::ostringstream all;
    all << in.rdbuf();
    const std::string written = all.str();

    if (written.find("hello from the consumer, 7 levels named") == std::string::npos) {
        std::printf("the backend did not write the message; got: %s\n", written.c_str());
        return 1;
    }

    std::printf("logforge %s: core + spdlog backend, %u bytes written\n",
                LOGFORGE_VERSION_STRING, static_cast<unsigned>(written.size()));
    return 0;
}
