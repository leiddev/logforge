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
#include <thread>
#include <vector>

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

// A path that cannot be opened as a file, on either platform and without
// depending on the user's privileges: a regular file where a directory has to
// be. Creating the directory fails, and so does the sink that opens the path
// below it.
std::string blocked_path(const char *name) {
    const std::string blocker = fresh_file(name);
    std::FILE *file = std::fopen(blocker.c_str(), "wb");
    EXPECT_NE(static_cast<std::FILE *>(0), file);
    if (file != 0) {
        std::fputs("not a directory", file);
        std::fclose(file);
    }
    return blocker + "/nested/log.txt";
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

// An empty pattern is not "reset to the default one". spdlog compiles the empty
// string into a formatter with no fields, so the message body comes out as
// nothing: the sink writes the pattern's own newline and the line's text is
// gone. Nothing raises an error, so a program that called set_pattern("") to
// undo a pattern would keep logging and quietly lose every message. Config has
// always treated an empty pattern as "keep the default"; set_pattern() hands the
// string to spdlog unchanged and therefore does not. This test records the
// difference so it is visible in the suite rather than in a log that stopped
// being readable.
TEST(SpdlogBackendTest, ClearingThePatternDropsTheMessageBody) {
    const std::string path = fresh_file("logforge_setpattern_empty.txt");
    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::info);
    config.pattern = "%v|%l";

    logforge::SpdlogBackend backend(config);
    logforge::log_to(&backend, logforge::Level::info, "first");
    backend.set_pattern("");
    logforge::log_to(&backend, logforge::Level::info, "second");
    backend.flush();

    EXPECT_EQ("first|info\n\n", normalize_eol(read_file(path)));
}

TEST(SpdlogBackendTest, NativeHandsOutTheSpdlogLogger) {
    logforge::SpdlogBackend backend(
        file_config(fresh_file("logforge_native.txt"), logforge::Level::info));

    ASSERT_NE(static_cast<spdlog::logger *>(0), backend.native());
    EXPECT_EQ("logforge", backend.native()->name());
}

// The name is the consumer's, not ours: a program whose configuration carries one
// passes it in the struct. spdlog has no rename, so a backend built with the
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

// Two destinations at once is the ordinary configuration -- a console a person
// reads and a file that outlives the run -- and it has to put the same body in
// both. Everything else in this file uses one sink, or a file with the console
// explicitly off.
TEST(SpdlogBackendTest, OneRecordReachesEveryConfiguredSink) {
    const std::string path = fresh_file("logforge_both_sinks.txt");
    logforge::SpdlogBackend::Config config;
    config.level = logforge::Level::trace;
    config.console = true;
    config.file_path = path;
    config.max_file_bytes = 0;
    config.pattern = "%v";

    logforge::SpdlogBackend backend(config);
    logforge::log_to(&backend, logforge::Level::info, "both: here", 10, "udp");
    backend.flush();

    EXPECT_EQ("udp: both: here\n", normalize_eol(read_file(path)));
}

// The message body is what StderrLogger would write, so swapping the backend
// changes where a line goes and not what it says. Built here from a record with
// a component and from one without, in the same backend, because the prefixed
// path and the plain path share one thread-local buffer.
TEST(SpdlogBackendTest, TheComponentPrefixIsTheOnlyDifferenceInTheBody) {
    const std::string path = fresh_file("logforge_body.txt");
    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::trace);
    config.pattern = "%v";

    logforge::SpdlogBackend backend(config);
    logforge::log_to(&backend, logforge::Level::info, "plain");
    logforge::log_to(&backend, logforge::Level::info, "tagged", 6, "udp");
    logforge::log_to(&backend, logforge::Level::info, "plain again");
    logforge::log_to(&backend, logforge::Level::info, "empty tag", 9, "");
    backend.flush();

    // An empty component is not a component: no ": " is written for it.
    EXPECT_EQ("plain\nudp: tagged\nplain again\nempty tag\n", normalize_eol(read_file(path)));
}

// The message is handed to spdlog as (pointer, size), so an embedded zero is a
// byte of the message and not the end of it.
TEST(SpdlogBackendTest, AnEmbeddedZeroSurvivesTheBackend) {
    const std::string path = fresh_file("logforge_embedded_zero.txt");
    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::info);
    config.pattern = "%v";

    logforge::SpdlogBackend backend(config);
    logforge::log_to(&backend, logforge::Level::info, "a\0b", 3);
    logforge::log_to(&backend, logforge::Level::info, "a\0b", 3, "udp");
    backend.flush();

    const std::string written = normalize_eol(read_file(path));
    EXPECT_EQ(std::string("a\0b\n", 4) + std::string("udp: a\0b\n", 9), written);
}

// A null text is a record a buggy producer can build; dropping it is the only
// thing a backend may do with it.
TEST(SpdlogBackendTest, ANullTextIsDropped) {
    const std::string path = fresh_file("logforge_null_text.txt");
    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::info);
    config.pattern = "%v";

    logforge::SpdlogBackend backend(config);
    logforge::log_to(&backend, logforge::Level::info, static_cast<const char *>(0), 0);
    logforge::log_to(&backend, logforge::Level::info, "after");
    backend.flush();

    EXPECT_EQ("after\n", normalize_eol(read_file(path)));
}

void write_text_file(const std::string &path, const char *text) {
    std::FILE *file = std::fopen(path.c_str(), "wb");
    ASSERT_NE(static_cast<std::FILE *>(0), file);
    std::fputs(text, file);
    std::fclose(file);
}

// The reason open() exists: a daemon that cannot write its file still comes up and
// says so. The console is kept, the exception is left for the caller to report --
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

// The constructor is the strict path: the same configuration that open() reports
// and survives is an exception here. A program whose configuration is supposed
// to be right stops at start-up, which is the whole point of the two entry
// points existing side by side.
TEST(SpdlogBackendTest, TheConstructorThrowsWhereOpenReports) {
    logforge::SpdlogBackend::Config config;
    config.level = logforge::Level::info;
    config.console = false;
    config.file_path = blocked_path("logforge_strict_blocker");
    config.max_file_bytes = 0;

    // The same input, both ways: the strict path throws spdlog's own exception
    // -- the one Report would have carried -- and open() hands back a backend.
    EXPECT_THROW(logforge::SpdlogBackend backend(config), spdlog::spdlog_ex);

    logforge::SpdlogBackend::Report report;
    EXPECT_NE(static_cast<logforge::SpdlogBackend *>(0),
              logforge::SpdlogBackend::open(config, &report).get());
    EXPECT_FALSE(report.file.empty());
}

// A UDP destination that cannot be built is one more destination that is
// reported rather than thrown. spdlog rejects a host it cannot resolve when the
// sink is constructed, so open() has to catch that too.
TEST(SpdlogBackendTest, OpenReportsAUdpSinkItCannotBuild) {
    logforge::SpdlogBackend::Config config;
    config.level = logforge::Level::info;
    config.console = false;
    config.udp_host = "256.256.256.256"; // not an address any resolver accepts
    config.udp_port = 5140;

    logforge::SpdlogBackend::Report report;
    const std::unique_ptr<logforge::SpdlogBackend> backend =
        logforge::SpdlogBackend::open(config, &report);

    // The configured console fallback is what keeps the backend alive; the sink
    // that failed is named and nothing else is.
    ASSERT_NE(static_cast<logforge::SpdlogBackend *>(0), backend.get());
    EXPECT_FALSE(report.udp.empty());
    EXPECT_TRUE(report.file.empty());
    EXPECT_FALSE(report.empty());
}

// The same condition on the strict path is an exception, like every other
// destination that cannot be built.
TEST(SpdlogBackendTest, TheConstructorThrowsOnAUdpSinkItCannotBuild) {
    logforge::SpdlogBackend::Config config;
    config.level = logforge::Level::info;
    config.console = false;
    config.udp_host = "256.256.256.256";
    config.udp_port = 5140;

    EXPECT_THROW(logforge::SpdlogBackend backend(config), spdlog::spdlog_ex);
}

// A host without a port, or a port without a host, is not a UDP destination:
// both are required, and a half configured one must not become a sink that
// sends somewhere unintended.
TEST(SpdlogBackendTest, AUdpDestinationNeedsBothHostAndPort) {
    logforge::SpdlogBackend::Config no_port;
    no_port.level = logforge::Level::info;
    no_port.console = false;
    no_port.udp_host = "127.0.0.1";
    no_port.udp_port = 0;

    logforge::SpdlogBackend::Report report;
    const std::unique_ptr<logforge::SpdlogBackend> backend =
        logforge::SpdlogBackend::open(no_port, &report);
    ASSERT_NE(static_cast<logforge::SpdlogBackend *>(0), backend.get());
    EXPECT_TRUE(report.empty());

    logforge::SpdlogBackend::Config no_host;
    no_host.level = logforge::Level::info;
    no_host.console = false;
    no_host.udp_host.clear();
    no_host.udp_port = 5140;

    logforge::SpdlogBackend::Report second;
    EXPECT_NE(static_cast<logforge::SpdlogBackend *>(0),
              logforge::SpdlogBackend::open(no_host, &second).get());
    EXPECT_TRUE(second.empty());
}

// A report is optional: a caller that does not want to know passes null, and the
// backend still comes back.
TEST(SpdlogBackendTest, OpenWorksWithoutAReport) {
    logforge::SpdlogBackend::Config config;
    config.level = logforge::Level::info;
    config.console = false;
    config.file_path = blocked_path("logforge_noreport_blocker");
    config.max_file_bytes = 0;

    const std::unique_ptr<logforge::SpdlogBackend> backend =
        logforge::SpdlogBackend::open(config, 0);
    EXPECT_NE(static_cast<logforge::SpdlogBackend *>(0), backend.get());
}

// Opens a backend that creates the directories above `path`, logs one line
// through it and returns what the file holds. The path is what each of the
// create_directories tests varies.
std::string log_through_created_directories(const std::string &path, const char *text) {
    std::remove(path.c_str());

    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::info);
    config.pattern = "%v";
    config.create_directories = true;

    logforge::SpdlogBackend::Report report;
    const std::unique_ptr<logforge::SpdlogBackend> backend =
        logforge::SpdlogBackend::open(config, &report);
    EXPECT_NE(static_cast<logforge::SpdlogBackend *>(0), backend.get());
    EXPECT_TRUE(report.empty());
    if (backend) {
        logforge::log_to(backend.get(), logforge::Level::info, text);
        backend->flush();
    }

    const std::string written = normalize_eol(read_file(path));
    std::remove(path.c_str());
    return written;
}

// The flag a program that ships "logs/pipeline.log" needs: the directory that
// contains the file is created before the sink opens it, on either platform.
TEST(SpdlogBackendTest, OpenCreatesTheDirectoriesItWasToldTo) {
    EXPECT_EQ("nested\n",
              log_through_created_directories(::testing::TempDir() + "logforge_mkdir/a/b/log.txt",
                                              "nested"));
}

// A bare file name has no directories to create: the working directory is the
// directory. The path walk must not try to make a directory out of the file name
// itself, which is the mistake a loop over the whole string makes.
TEST(SpdlogBackendTest, CreateDirectoriesLeavesABareFileNameAlone) {
    EXPECT_EQ("bare\n", log_through_created_directories("logforge_bare_file_name.log", "bare"));
}

// The relative path from the documentation -- "logs/pipeline.log" -- is the one
// a test directory does not otherwise produce, and it runs the shortest form of
// the walk: one separator, no drive prefix, no absolute root.
TEST(SpdlogBackendTest, CreateDirectoriesHandlesARelativePath) {
    EXPECT_EQ("relative\n",
              log_through_created_directories("logforge_relative_dir/logforge_relative.txt",
                                              "relative"));
}

// Creating a directory that already exists is not a failure: the flag is on in
// every run after the first one, and the mkdir that returns EEXIST is ignored on
// purpose so that a restart does not lose its file.
TEST(SpdlogBackendTest, CreateDirectoriesToleratesWhatAlreadyExists) {
    EXPECT_EQ("first\n",
              log_through_created_directories("logforge_existing_dir/first.txt", "first"));
    EXPECT_EQ("second\n",
              log_through_created_directories("logforge_existing_dir/second.txt", "second"));
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

// The destructor clears logforge's default only when it is the one installed. A
// backend that was never installed, or one that was replaced by another before
// it died, must leave the surviving default exactly where it is.
TEST(SpdlogBackendTest, ADestroyedBackendLeavesSomeoneElsesDefaultAlone) {
    logforge::SpdlogBackend survivor(
        file_config(fresh_file("logforge_survivor.txt"), logforge::Level::info));
    {
        logforge::SpdlogBackend transient(
            file_config(fresh_file("logforge_transient.txt"), logforge::Level::info));
        logforge::set_default(&survivor);
    }
    EXPECT_EQ(&survivor, logforge::default_logger());
    logforge::set_default(0);
}

// spdlog's registry keeps its own reference to the logger, so a backend that was
// installed as spdlog's default and then destroyed leaves a logger that is still
// safe to call -- and the destructor deliberately does not unregister it. The
// spdlog-level default is what a dependency logging behind the caller's back
// uses, so it must not become a dangling pointer when our object goes away.
TEST(SpdlogBackendTest, SpdlogKeepsTheLoggerAliveAfterTheBackendIsDestroyed) {
    const std::string path = fresh_file("logforge_after_destroy.txt");
    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::info);
    config.pattern = "%v";

    const std::shared_ptr<spdlog::logger> previous_spdlog = spdlog::default_logger();
    {
        logforge::SpdlogBackend backend(config);
        backend.install_as_default();
        EXPECT_EQ(backend.native(), spdlog::default_logger().get());
    }

    // The object is gone; the logger spdlog holds is not, and a line through it
    // still reaches the file the backend configured.
    ASSERT_NE(static_cast<spdlog::logger *>(0), spdlog::default_logger().get());
    spdlog::default_logger()->info("after the backend");
    spdlog::default_logger()->flush();
    EXPECT_EQ("after the backend\n", normalize_eol(read_file(path)));

    spdlog::set_default_logger(previous_spdlog);
}

// The backend is shared with the rest of the process, and the contract in
// LOGGING.md says log() may be called from several threads at once. The body
// with a component is built in a thread-local buffer, so this is the test that
// fails if that buffer ever becomes a member or a static.
TEST(SpdlogBackendTest, ConcurrentRecordsWithComponentsDoNotMix) {
    const std::string path = fresh_file("logforge_concurrent.txt");
    logforge::SpdlogBackend::Config config = file_config(path, logforge::Level::info);
    config.pattern = "%v";

    logforge::SpdlogBackend backend(config);
    const int threads = 4;
    const int per_thread = 200;
    std::vector<std::thread> workers;
    for (int t = 0; t < threads; ++t) {
        workers.push_back(std::thread([&backend, t, per_thread]() {
            const std::string component = "t" + std::to_string(t);
            for (int i = 0; i < per_thread; ++i) {
                const std::string text = component + "-" + std::to_string(i);
                logforge::log_to(&backend, logforge::Level::info, text.data(), text.size(),
                                 component.c_str());
            }
        }));
    }
    for (std::size_t i = 0; i < workers.size(); ++i) {
        workers[i].join();
    }
    backend.flush();

    // Every line has to be "t<n>: t<n>-<i>", with the two numbers equal. A shared
    // buffer would produce lines whose prefix and body disagree.
    const std::string written = normalize_eol(read_file(path));
    std::istringstream lines(written);
    std::string line;
    int seen = 0;
    while (std::getline(lines, line)) {
        ++seen;
        ASSERT_GE(line.size(), 6u) << line;
        EXPECT_EQ(line[0], line[4]) << line;
        EXPECT_EQ(line[1], line[5]) << line;
        EXPECT_EQ(':', line[2]) << line;
        EXPECT_EQ(' ', line[3]) << line;
    }
    EXPECT_EQ(threads * per_thread, seen);
}

} // namespace
