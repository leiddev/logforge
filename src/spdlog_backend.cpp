#include <logforge/spdlog_backend.hpp>

#include <logforge/ambient.hpp>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/udp_sink.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace logforge {
namespace {

spdlog::level::level_enum to_spdlog_level(Level level) {
    switch (level) {
    case Level::trace:
        return spdlog::level::trace;
    case Level::debug:
        return spdlog::level::debug;
    case Level::info:
        return spdlog::level::info;
    case Level::warn:
        return spdlog::level::warn;
    case Level::error:
        return spdlog::level::err;
    case Level::critical:
        return spdlog::level::critical;
    case Level::off:
        break;
    }
    return spdlog::level::off;
}

// The message body is built here rather than in the renderer so that the
// component prefix stays a backend concern: a sink's pattern decides how (and
// whether) a level appears, but "udp: " is part of what the producer said.
//
// Thread local, so dispatching on two threads does not fight over one buffer and
// so an untagged line -- the common case -- allocates nothing at all.
thread_local std::string tls_body;

// One destination at a time, so that the strict constructor and open() share the
// code that decides what a destination is.

spdlog::sink_ptr make_console_sink() {
    return std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
}

// Creates the directories that contain a file path, outermost first.  No spdlog
// sink does this; a program that ships "logs/pipeline.log" in its configuration
// either creates "logs" itself or dies on its first start.  Best effort on
// purpose: a directory that cannot be made is reported by the file sink, which is
// the one that then cannot open the file.
void create_directories_of(const std::string &file_path) {
    const std::size_t last_separator = file_path.find_last_of("/\\");
    if (last_separator == std::string::npos) {
        return; // a bare file name: the working directory is the directory
    }
    for (std::size_t i = 1; i <= last_separator; ++i) {
        if (file_path[i] != '/' && file_path[i] != '\\') {
            continue;
        }
        if (file_path[i - 1] == ':') {
            continue; // the slash of a drive prefix, "C:/..."
        }
        const std::string directory = file_path.substr(0, i);
#ifdef _WIN32
        (void)_mkdir(directory.c_str());
#else
        (void)mkdir(directory.c_str(), 0755);
#endif
    }
}

spdlog::sink_ptr make_file_sink(const SpdlogBackend::Config &config) {
    if (config.create_directories) {
        create_directories_of(config.file_path);
    }
    if (config.max_file_bytes > 0) {
        return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            config.file_path, config.max_file_bytes, std::max<std::size_t>(config.max_files, 1));
    }
    return std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.file_path, false);
}

spdlog::sink_ptr make_udp_sink(const SpdlogBackend::Config &config) {
    return std::make_shared<spdlog::sinks::udp_sink_mt>(
        spdlog::sinks::udp_sink_config(config.udp_host, config.udp_port));
}

std::string logger_name(const SpdlogBackend::Config &config) {
    return config.name.empty() ? std::string("logforge") : config.name;
}

// The strict path: the first destination that fails takes the whole backend with
// it, which is what an exception at startup is for.
std::vector<spdlog::sink_ptr> make_sinks(const SpdlogBackend::Config &config) {
    std::vector<spdlog::sink_ptr> sinks;
    if (config.console) {
        sinks.push_back(make_console_sink());
    }
    if (!config.file_path.empty()) {
        sinks.push_back(make_file_sink(config));
    }
    if (!config.udp_host.empty() && config.udp_port != 0) {
        sinks.push_back(make_udp_sink(config));
    }
    // A backend with nothing to write to would accept records and lose them.  A
    // colour sink on stdout is the least surprising default.
    if (sinks.empty()) {
        sinks.push_back(make_console_sink());
    }
    return sinks;
}

} // namespace

struct SpdlogBackend::Impl {
    Impl(const Config &config, const std::vector<spdlog::sink_ptr> &sink_list)
        : sinks(sink_list), min_level(config.level) {
        logger = std::make_shared<spdlog::logger>(logger_name(config), sinks.begin(), sinks.end());

        // Our own enabled() is the filter; spdlog's must not disagree with it, so
        // it is set to the same level.  A caller that changes it through native()
        // gets both filters applied, which is spdlog's documented behaviour and the
        // honest outcome of using the escape hatch.
        logger->set_level(to_spdlog_level(config.level));

        if (!config.pattern.empty()) {
            logger->set_pattern(config.pattern);
        }
    }

    std::vector<spdlog::sink_ptr> sinks;
    std::shared_ptr<spdlog::logger> logger;
    Level min_level;
};

SpdlogBackend::SpdlogBackend() : SpdlogBackend(Config()) {
}

SpdlogBackend::SpdlogBackend(const Config &config) : impl_(new Impl(config, make_sinks(config))) {
}

SpdlogBackend::SpdlogBackend(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {
}

std::unique_ptr<SpdlogBackend> SpdlogBackend::open(const Config &config, Report *report) {
    Report local;
    std::vector<spdlog::sink_ptr> sinks;

    if (config.console) {
        try {
            sinks.push_back(make_console_sink());
        } catch (const std::exception &error) {
            local.console = error.what();
        }
    }
    if (!config.file_path.empty()) {
        try {
            sinks.push_back(make_file_sink(config));
        } catch (const std::exception &error) {
            local.file = error.what();
        }
    }
    if (!config.udp_host.empty() && config.udp_port != 0) {
        try {
            sinks.push_back(make_udp_sink(config));
        } catch (const std::exception &error) {
            local.udp = error.what();
        }
    }
    // The constructor's default, kept here too: a configuration that names no
    // destination still gets a console, not a logger that silently drops records.
    if (sinks.empty() && !config.console) {
        try {
            sinks.push_back(make_console_sink());
        } catch (const std::exception &error) {
            local.console = error.what();
        }
    }

    if (sinks.empty()) {
        if (report != 0) {
            *report = local;
        }
        return std::unique_ptr<SpdlogBackend>();
    }

    std::unique_ptr<SpdlogBackend> backend(
        new SpdlogBackend(std::unique_ptr<Impl>(new Impl(config, sinks))));

    if (report != 0) {
        *report = local;
    }
    return backend;
}

void SpdlogBackend::install_as_default() {
    logforge::set_default(this);
    spdlog::set_default_logger(impl_->logger);
}

SpdlogBackend::~SpdlogBackend() {
    // Never leave the process default pointing at a destroyed backend: a program
    // destroys its logger on the way out and keeps logging while it does.
    if (logforge::default_logger() == this) {
        logforge::set_default(0);
    }
}

bool SpdlogBackend::enabled(Level level) const {
    return level != Level::off && level >= impl_->min_level;
}

void SpdlogBackend::log(const Record &record) {
    if (record.text == 0 || !enabled(record.level)) {
        return;
    }

    const char *body = record.text;
    std::size_t size = record.size;
    if (record.component != 0 && record.component[0] != '\0') {
        tls_body.clear();
        tls_body.append(record.component);
        tls_body.append(": ");
        tls_body.append(record.text, record.size);
        body = tls_body.data();
        size = tls_body.size();
    }

    // The string_view overload on purpose: it skips fmt entirely, so a message
    // that came out of the renderer is never parsed a second time.
    impl_->logger->log(spdlog::source_loc(record.file, record.line, 0),
                       to_spdlog_level(record.level), spdlog::string_view_t(body, size));
}

void SpdlogBackend::flush() {
    impl_->logger->flush();
}

void SpdlogBackend::set_pattern(const std::string &pattern) {
    impl_->logger->set_pattern(pattern);
}

spdlog::logger *SpdlogBackend::native() const {
    return impl_->logger.get();
}

std::shared_ptr<spdlog::logger> SpdlogBackend::native_shared() const {
    return impl_->logger;
}

} // namespace logforge
