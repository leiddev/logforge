// The promise <logforge/spdlog_backend.hpp> makes is about compiling, not about
// calling: a producer's translation unit that never touches native() must not
// need a single spdlog or fmt header. That is what the forward declaration of
// spdlog::logger buys, and it is invisible to every other test in this
// directory, because spdlog_backend_test.cpp includes spdlog itself.
//
// So this file includes only the logforge header a start-up file needs, and
// does nothing with it that needs spdlog's definition. If the forward
// declaration ever becomes an #include, the build breaks here even though every
// other test keeps passing.

#include <logforge/spdlog_backend.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>

namespace {

// Config is a plain value type a start-up file fills in from its own
// configuration; none of its fields names an spdlog type.
TEST(SpdlogForwardDeclTest, TheConfigNeedsNoSpdlogHeader) {
    logforge::SpdlogBackend::Config config;
    config.level = logforge::Level::debug;
    config.name = "forward";
    config.console = false;
    config.file_path = "unused.log";
    config.max_file_bytes = 1024;
    config.max_files = 2;
    config.create_directories = true;
    config.udp_host = "127.0.0.1";
    config.udp_port = 5140;
    config.pattern = "%v";

    EXPECT_EQ(logforge::Level::debug, config.level);
    EXPECT_EQ(std::string("forward"), config.name);
    EXPECT_TRUE(config.create_directories);
}

// Report is a plain value type too: three strings and a predicate.
TEST(SpdlogForwardDeclTest, TheReportNeedsNoSpdlogHeader) {
    logforge::SpdlogBackend::Report report;
    EXPECT_TRUE(report.empty());
    report.file = "cannot open";
    EXPECT_FALSE(report.empty());
    report.file.clear();
    report.udp = "invalid address";
    EXPECT_FALSE(report.empty());
}

// The class is complete without spdlog, and it is a Logger, so a start-up file
// can hold one by pointer, install it and destroy it knowing only this header.
TEST(SpdlogForwardDeclTest, TheBackendIsACompleteLoggerType) {
    EXPECT_TRUE((std::is_class<logforge::SpdlogBackend>::value));
    EXPECT_TRUE((std::is_base_of<logforge::Logger, logforge::SpdlogBackend>::value));
    EXPECT_FALSE((std::is_copy_constructible<logforge::SpdlogBackend>::value));
    EXPECT_FALSE((std::is_copy_assignable<logforge::SpdlogBackend>::value));

    // The functions that produce one are declared here, which is all a header
    // that never builds one needs.
    typedef std::unique_ptr<logforge::SpdlogBackend> (*OpenSignature)(
        const logforge::SpdlogBackend::Config &, logforge::SpdlogBackend::Report *);
    EXPECT_TRUE((std::is_same<OpenSignature, decltype(&logforge::SpdlogBackend::open)>::value));
}

} // namespace
