#include <logforge/spdlog_backend.hpp>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/udp_sink.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <vector>

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

} // namespace

struct SpdlogBackend::Impl {
    std::vector<spdlog::sink_ptr> sinks;
    std::shared_ptr<spdlog::logger> logger;
    Level min_level;
};

SpdlogBackend::SpdlogBackend() : SpdlogBackend(Config()) {}

SpdlogBackend::SpdlogBackend(const Config &config) : impl_(new Impl()) {
    impl_->min_level = config.level;

    if (config.console) {
        impl_->sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    if (!config.file_path.empty()) {
        if (config.max_file_bytes > 0) {
            impl_->sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config.file_path, config.max_file_bytes,
                std::max<std::size_t>(config.max_files, 1)));
        } else {
            impl_->sinks.push_back(
                std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.file_path, false));
        }
    }

    if (!config.udp_host.empty() && config.udp_port != 0) {
        impl_->sinks.push_back(std::make_shared<spdlog::sinks::udp_sink_mt>(
            spdlog::sinks::udp_sink_config(config.udp_host, config.udp_port)));
    }

    // A backend with nothing to write to would accept records and lose them.  A
    // colour sink on stdout is the least surprising default.
    if (impl_->sinks.empty()) {
        impl_->sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    impl_->logger = std::make_shared<spdlog::logger>("logforge", impl_->sinks.begin(),
                                                    impl_->sinks.end());

    // Our own enabled() is the filter; spdlog's must not disagree with it, so it
    // is set to the same level.  A caller that changes it through native() gets
    // both filters applied, which is spdlog's documented behaviour and the
    // honest outcome of using the escape hatch.
    impl_->logger->set_level(to_spdlog_level(config.level));

    if (!config.pattern.empty()) {
        impl_->logger->set_pattern(config.pattern);
    }
}

SpdlogBackend::~SpdlogBackend() {}

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
                       to_spdlog_level(record.level),
                       spdlog::string_view_t(body, size));
}

void SpdlogBackend::flush() { impl_->logger->flush(); }

void SpdlogBackend::set_pattern(const std::string &pattern) {
    impl_->logger->set_pattern(pattern);
}

spdlog::logger *SpdlogBackend::native() const { return impl_->logger.get(); }

} // namespace logforge
