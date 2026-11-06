#include <logforge/stderr_logger.hpp>

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <io.h>
#define LOGFORGE_DUP _dup
#define LOGFORGE_DUP2 _dup2
#define LOGFORGE_CLOSE _close
#else
#include <unistd.h>
#define LOGFORGE_DUP dup
#define LOGFORGE_DUP2 dup2
#define LOGFORGE_CLOSE close
#endif

namespace {

// stderr is a text-mode stream, so the Windows CRT turns every '\n' we write
// into "\r\n".  That is the platform's line ending convention, not something the
// logger should override, so the assertions normalise instead.
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

// Writing into the system temporary directory keeps the capture file out of the
// build tree and out of the source tree, whichever directory ctest runs us from.
std::string capture_path() {
    return ::testing::TempDir() + "logforge_stderr_capture.txt";
}

// Redirects fd 2 to a file for the lifetime of the object and puts the original
// descriptor back afterwards.
//
// The file descriptor dance rather than freopen() is deliberate: freopen()
// leaves the GoogleTest output pointing at the capture file for the rest of the
// run, so a failure in a later test would print into a file nobody reads.
class StderrCapture {
public:
    explicit StderrCapture(const char *path) : path_(path), saved_(-1) {
        saved_ = LOGFORGE_DUP(2);
        if (saved_ >= 0) {
            std::freopen(path, "w", stderr);
        }
    }

    ~StderrCapture() {
        if (saved_ >= 0) {
            std::fflush(stderr);
            LOGFORGE_DUP2(saved_, 2);
            LOGFORGE_CLOSE(saved_);
        }
    }

    std::string contents() {
        std::fflush(stderr);
        std::ifstream in(path_.c_str(), std::ios::binary);
        std::ostringstream all;
        all << in.rdbuf();
        return all.str();
    }

private:
    StderrCapture(const StderrCapture &);
    StderrCapture &operator=(const StderrCapture &);

    std::string path_;
    int saved_;
};

TEST(StderrLoggerTest, EnabledFollowsTheMinimumLevel) {
    const logforge::StderrLogger quiet(logforge::Level::warn);

    EXPECT_FALSE(quiet.enabled(logforge::Level::trace));
    EXPECT_FALSE(quiet.enabled(logforge::Level::info));
    EXPECT_TRUE(quiet.enabled(logforge::Level::warn));
    EXPECT_TRUE(quiet.enabled(logforge::Level::error));
    EXPECT_TRUE(quiet.enabled(logforge::Level::critical));
    // `off` is the top of the scale, so nothing below it -- not even critical --
    // reaches a logger configured as off.
    EXPECT_FALSE(quiet.enabled(logforge::Level::off));
}

TEST(StderrLoggerTest, WritesOneLinePerRecord) {
    logforge::StderrLogger logger(logforge::Level::trace);

    std::string text;
    {
        StderrCapture capture(capture_path().c_str());
        logforge::log_to(&logger, logforge::Level::warn, "hello", 5, "udp");
        logforge::log_to(&logger, logforge::Level::info, std::string("plain"));
        text = normalize_eol(capture.contents());
    }

    EXPECT_EQ("[warn] udp: hello\n[info] plain\n", text);
}

TEST(StderrLoggerTest, SuppressesRecordsBelowTheMinimum) {
    logforge::StderrLogger logger(logforge::Level::error);

    std::string text;
    {
        StderrCapture capture(capture_path().c_str());
        logforge::log_to(&logger, logforge::Level::info, "invisible");
        logforge::log_to(&logger, logforge::Level::error, "visible");
        text = normalize_eol(capture.contents());
    }

    EXPECT_EQ("[error] visible\n", text);
}

// The 512 byte stack buffer is an optimisation, not a limit: silently dropping
// or truncating a long line would be the kind of bug that only shows up in the
// one log line that mattered.
TEST(StderrLoggerTest, LongRecordsAreStillWrittenWhole) {
    logforge::StderrLogger logger(logforge::Level::trace);
    const std::string long_text(2000, 'x');

    std::string text;
    {
        StderrCapture capture(capture_path().c_str());
        logforge::log_to(&logger, logforge::Level::info, long_text);
        text = normalize_eol(capture.contents());
    }

    EXPECT_EQ("[info] " + long_text + "\n", text);
}

} // namespace
