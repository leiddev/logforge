#include <logforge/ambient.hpp>
#include <logforge/spdlog_backend.hpp>

#include <gtest/gtest.h>

#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <fstream>
#include <memory>
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
    const logforge::SpdlogBackend backend(
        file_config(fresh_file("logforge_enabled.txt"), logforge::Level::warn));

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

TEST(SpdlogBackendTest, NativeSharedKeepsRawSpdlogCallsOnTheseSinks) {
    // The bridge for a program that still logs through spdlog directly: a file
    // that has not been ported, or a library that calls spdlog::info(). Without
    // it those lines go to spdlog's own default logger and vanish from the file
    // this backend configured -- a silent regression the day a dependency starts
    // logging behind the caller's back.
    const std::string path = fresh_file("logforge_native_shared.txt");
    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::info);
    config.pattern = "%l|%v";

    logforge::SpdlogBackend backend(config);

    const std::shared_ptr<spdlog::logger> previous = spdlog::default_logger();
    spdlog::set_default_logger(backend.native_shared());
    spdlog::info("raw {}", 7);
    backend.flush();
    spdlog::set_default_logger(previous);

    EXPECT_EQ("info|raw 7\n", normalize_eol(read_file(path)));
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
    logforge::SpdlogBackend backend(
        file_config(fresh_file("logforge_native.txt"), logforge::Level::info));

    ASSERT_NE(static_cast<spdlog::logger *>(0), backend.native());
    EXPECT_EQ("logforge", backend.native()->name());
}

// The name is the consumer's, not ours: a program whose configuration carries one
// passes it in the struct.  spdlog has no rename, so a backend built with the
// wrong name cannot be corrected afterwards.
TEST(SpdlogBackendTest, TheNameComesFromTheConfiguration) {
    const std::string path = fresh_file("logforge_name.txt");
    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::info);
    config.name = "xec";
    config.pattern = "%n|%v";

    logforge::SpdlogBackend backend(config);
    logforge::log_to(&backend, logforge::Level::info, "started");
    backend.flush();

    ASSERT_NE(static_cast<spdlog::logger *>(0), backend.native());
    EXPECT_EQ("xec", backend.native()->name());
    EXPECT_EQ("xec|started\n", normalize_eol(read_file(path)));
}

// An empty name is a configuration slip, not a reason to build a logger spdlog
// cannot identify.
TEST(SpdlogBackendTest, AnEmptyNameFallsBackToTheDefault) {
    logforge::SpdlogBackend::Config config =
        file_config(fresh_file("logforge_name_empty.txt"), logforge::Level::info);
    config.name.clear();

    logforge::SpdlogBackend backend(config);
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

void write_text_file(const std::string &path, const char *text) {
    std::FILE *file = std::fopen(path.c_str(), "wb");
    ASSERT_NE(static_cast<std::FILE *>(0), file);
    std::fputs(text, file);
    std::fclose(file);
}

// The reason open() exists: a daemon that cannot write its file still comes up and
// says so.  The console is kept, the exception is left for the caller to report --
// running on without the file is the one outcome worse than stopping, so it must
// never be silent.
TEST(SpdlogBackendTest, OpenKeepsWhatItCanAndNamesWhatItCannot) {
    // A regular file where a directory has to be: creating the directory fails,
    // and the sink cannot open the path below it.  That is the shape of a missing
    // or read-only directory without needing one.
    const std::string blocker = fresh_file("logforge_blocker");
    write_text_file(blocker, "not a directory");

    logforge::SpdlogBackend::Config config;
    config.level = logforge::Level::info;
    config.pattern = "%v";
    config.file_path = blocker + "/nested/log.txt";
    config.max_file_bytes = 0;

    logforge::SpdlogBackend::Report report;
    const std::unique_ptr<logforge::SpdlogBackend> backend =
        logforge::SpdlogBackend::open(config, &report);

    ASSERT_NE(static_cast<logforge::SpdlogBackend *>(0), backend.get());
    EXPECT_TRUE(report.console.empty());
    EXPECT_TRUE(report.udp.empty());
    EXPECT_FALSE(report.file.empty());
    EXPECT_FALSE(report.empty());
    EXPECT_TRUE(backend->enabled(logforge::Level::info));
}

// The flag a program that ships "logs/pipeline.log" needs: the directory that
// contains the file is created before the sink opens it, on either platform.
TEST(SpdlogBackendTest, OpenCreatesTheDirectoriesItWasToldTo) {
    const std::string path = ::testing::TempDir() + "logforge_mkdir/a/b/log.txt";
    std::remove(path.c_str());

    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::info);
    config.pattern = "%v";
    config.create_directories = true;

    logforge::SpdlogBackend::Report report;
    const std::unique_ptr<logforge::SpdlogBackend> backend =
        logforge::SpdlogBackend::open(config, &report);

    ASSERT_NE(static_cast<logforge::SpdlogBackend *>(0), backend.get());
    EXPECT_TRUE(report.empty());

    logforge::log_to(backend.get(), logforge::Level::info, "nested");
    backend->flush();
    EXPECT_EQ("nested\n", normalize_eol(read_file(path)));
}

// Same default as the constructor: a configuration that names no destination gets
// a console, not a logger whose records disappear.
TEST(SpdlogBackendTest, OpenWithNoDestinationFallsBackToTheConsole) {
    logforge::SpdlogBackend::Config config;
    config.console = false;
    config.level = logforge::Level::off;

    logforge::SpdlogBackend::Report report;
    const std::unique_ptr<logforge::SpdlogBackend> backend =
        logforge::SpdlogBackend::open(config, &report);

    ASSERT_NE(static_cast<logforge::SpdlogBackend *>(0), backend.get());
    EXPECT_TRUE(report.empty());
}

// One call, both defaults: logforge's (macros, ambient) and spdlog's (its own free
// functions, which libraries that log behind the caller's back use).  Forgetting
// the second one sends those lines to spdlog's built-in console logger.
TEST(SpdlogBackendTest, InstallAsDefaultServesBothLoggers) {
    const std::string path = fresh_file("logforge_install.txt");
    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::info);
    config.pattern = "%v";

    logforge::Logger *const previous_logforge = logforge::default_logger();
    const std::shared_ptr<spdlog::logger> previous_spdlog = spdlog::default_logger();

    logforge::SpdlogBackend backend(config);
    backend.install_as_default();

    EXPECT_EQ(&backend, logforge::current());
    EXPECT_EQ(backend.native(), spdlog::default_logger().get());

    logforge::log_to(logforge::current(), logforge::Level::info, "via logforge");
    spdlog::default_logger()->info("via spdlog");
    backend.flush();

    EXPECT_EQ("via logforge\nvia spdlog\n", normalize_eol(read_file(path)));

    logforge::set_default(previous_logforge);
    spdlog::set_default_logger(previous_spdlog);
}

// A backend that was installed and then destroyed must not leave the process
// default pointing at itself: programs destroy theirs on the way out and keep
// logging while they do.
TEST(SpdlogBackendTest, ADestroyedBackendDoesNotStayTheDefault) {
    logforge::Logger *const before = logforge::default_logger();
    const std::shared_ptr<spdlog::logger> previous_spdlog = spdlog::default_logger();
    {
        logforge::SpdlogBackend backend(
            file_config(fresh_file("logforge_destroyed.txt"), logforge::Level::info));
        backend.install_as_default();
        EXPECT_EQ(&backend, logforge::default_logger());
    }
    EXPECT_EQ(before, logforge::default_logger());
    spdlog::set_default_logger(previous_spdlog);
}

} // namespace
