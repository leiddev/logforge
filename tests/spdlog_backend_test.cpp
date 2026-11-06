#include <logforge/spdlog_backend.hpp>

#include <gtest/gtest.h>

#include <spdlog/logger.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

// A fresh file per test: spdlog's basic_file_sink appends unless told to
// truncate, so a leftover from a previous run would make the assertion depend on
// history.
std::string fresh_file(const char *name) {
    const std::string path = ::testing::TempDir() + name;
    std::remove(path.c_str());
    return path;
}

std::string read_file(const std::string &path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
}

std::string normalize_eol(std::string text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
            continue;
        }
        out.push_back(text[i]);
    }
    return out;
}

logforge::SpdlogBackend::Config file_config(const std::string &path, logforge::Level level) {
    logforge::SpdlogBackend::Config config;
    config.level = level;
    config.console = false;
    config.file_path = path;
    config.max_file_bytes = 0; // one file, no rotation
    return config;
}

TEST(SpdlogBackendTest, EnabledFollowsLevelAndTreatsOffAsSilence) {
    const logforge::SpdlogBackend backend(file_config(fresh_file("logforge_enabled.txt"),
                                                      logforge::Level::warn));

    EXPECT_FALSE(backend.enabled(logforge::Level::info));
    EXPECT_TRUE(backend.enabled(logforge::Level::warn));
    EXPECT_TRUE(backend.enabled(logforge::Level::critical));
    EXPECT_FALSE(backend.enabled(logforge::Level::off));
}

TEST(SpdlogBackendTest, EveryLevelReachesItsOwnSpdlogLevel) {
    // The table that decides both the %l field and the filters spdlog applies on
    // its own sinks. It is the one thing that can go wrong without anything
    // failing: a level mapped one step off still writes a line, just labelled as
    // another level, and no caller is ever told.
    const std::string path = fresh_file("logforge_levels.txt");
    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::trace);
    config.pattern = "%l|%v";

    logforge::SpdlogBackend backend(config);
    logforge::log_to(&backend, logforge::Level::trace, "a", 1);
    logforge::log_to(&backend, logforge::Level::debug, "b", 1);
    logforge::log_to(&backend, logforge::Level::info, "c", 1);
    logforge::log_to(&backend, logforge::Level::warn, "d", 1);
    logforge::log_to(&backend, logforge::Level::error, "e", 1);
    logforge::log_to(&backend, logforge::Level::critical, "f", 1);
    backend.flush();

    // spdlog spells warn and error its own way; the body is still ours.
    EXPECT_EQ("trace|a\ndebug|b\ninfo|c\nwarning|d\nerror|e\ncritical|f\n",
              normalize_eol(read_file(path)));
}

TEST(SpdlogBackendTest, WritesThroughTheConfiguredPattern) {
    const std::string path = fresh_file("logforge_pattern.txt");
    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::trace);
    config.pattern = "%l|%v";

    logforge::SpdlogBackend backend(config);
    logforge::log_to(&backend, logforge::Level::info, "hello", 5, "udp");
    logforge::log_to(&backend, logforge::Level::warn, std::string("plain"));
    backend.flush();

    // spdlog's own level name for warn is "warning"; the pattern is spdlog's to
    // spell, and the body is ours ("component: text").
    EXPECT_EQ("info|udp: hello\nwarning|plain\n", normalize_eol(read_file(path)));
}

TEST(SpdlogBackendTest, DropsRecordsBelowTheConfiguredLevel) {
    const std::string path = fresh_file("logforge_level.txt");
    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::error);
    config.pattern = "%v";

    logforge::SpdlogBackend backend(config);
    logforge::log_to(&backend, logforge::Level::info, "below");
    logforge::log_to(&backend, logforge::Level::error, "at the level");
    logforge::log_to(&backend, logforge::Level::off, "off");
    backend.flush();

    EXPECT_EQ("at the level\n", normalize_eol(read_file(path)));
}

// The source location captured by the macros has to survive the trip through the
// record and into spdlog's pattern, otherwise a log line cannot be traced back.
TEST(SpdlogBackendTest, SourceLocationReachesThePattern) {
    const std::string path = fresh_file("logforge_location.txt");
    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::trace);
    config.pattern = "%g:%#|%v";

    logforge::SpdlogBackend backend(config);
    logforge::log_to(&backend, logforge::Level::debug, "tick", 4, 0, "element.cpp", 42);
    backend.flush();

    EXPECT_EQ("element.cpp:42|tick\n", normalize_eol(read_file(path)));
}

TEST(SpdlogBackendTest, PatternCanBeChangedAfterConstruction) {
    const std::string path = fresh_file("logforge_setpattern.txt");
    logforge::SpdlogBackend backend(file_config(path, logforge::Level::trace));

    backend.set_pattern("%v");
    logforge::log_to(&backend, logforge::Level::info, "bare");
    backend.flush();

    EXPECT_EQ("bare\n", normalize_eol(read_file(path)));
}

TEST(SpdlogBackendTest, NativeHandsOutTheSpdlogLogger) {
    logforge::SpdlogBackend backend(file_config(fresh_file("logforge_native.txt"),
                                                logforge::Level::info));

    ASSERT_NE(static_cast<spdlog::logger *>(0), backend.native());
    EXPECT_EQ("logforge", backend.native()->name());
}

// A backend with no sink would accept records and drop them; the constructor
// falls back to the console instead.
TEST(SpdlogBackendTest, AnEmptyConfigurationStillHasASink) {
    logforge::SpdlogBackend::Config config;
    config.console = false;
    config.level = logforge::Level::off;

    logforge::SpdlogBackend backend(config);
    EXPECT_FALSE(backend.enabled(logforge::Level::critical));
    ASSERT_NE(static_cast<spdlog::logger *>(0), backend.native());
    SUCCEED();
}

} // namespace
