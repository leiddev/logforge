// The compat header exists to give a ported file the bare LOG_* names, and it
// exists as a separate header rather than in <logforge/macros.hpp> because those
// names belong to glog, syslog and several embedded frameworks. Its contract is
// therefore two-sided: every name is defined when nobody else defined it, and no
// name is redefined when somebody did.
//
// A definition of LOG_INFO here, before the include, stands in for the project
// that already had one. The header's #ifndef guard has to leave it alone; a
// header that defined it unconditionally would break that project silently,
// because the conflict would be a macro redefinition warning at best.

#define LOG_INFO(...) "the project's own LOG_INFO"
#define LOG_WARN(...) "the project's own LOG_WARN"

#include <logforge/compat/legacy_names.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

struct Captured {
    logforge::Level level;
    std::string text;
};

class RecordingLogger : public logforge::Logger {
public:
    bool enabled(logforge::Level level) const override { return level != logforge::Level::off; }

    void log(const logforge::Record &record) override {
        Captured entry;
        entry.level = record.level;
        entry.text.assign(record.text, record.size);
        captured.push_back(entry);
    }

    std::vector<Captured> captured;
};

TEST(LegacyNamesTest, ANameTheCallerAlreadyDefinedIsLeftAlone) {
    EXPECT_STREQ("the project's own LOG_INFO", LOG_INFO("ignored"));
    EXPECT_STREQ("the project's own LOG_WARN", LOG_WARN("ignored"));
}

// LOG_ERR is the legacy spelling for error and LOG_CRITICAL for critical: two
// different levels, which is the mistake a ported file makes when it assumes
// they are synonyms.
TEST(LegacyNamesTest, ErrAndCriticalAreDifferentLevels) {
    RecordingLogger recorder;
    logforge::set_default(&recorder);

    LOG_ERR("e");
    LOG_CRITICAL("c");

    ASSERT_EQ(2u, recorder.captured.size());
    EXPECT_EQ(logforge::Level::error, recorder.captured[0].level);
    EXPECT_EQ(logforge::Level::critical, recorder.captured[1].level);

    logforge::set_default(0);
}

// LOG_DEBUG goes through the same expansion as the LOGFORGE_* names, so a
// format string with arguments renders the same way.
TEST(LegacyNamesTest, TheLegacyNamesFormatLikeThePrefixedOnes) {
    RecordingLogger recorder;
    logforge::set_default(&recorder);

    LOG_DEBUG("dropped {:04x} frames", 255);

    ASSERT_EQ(1u, recorder.captured.size());
    EXPECT_EQ("dropped 00ff frames", recorder.captured[0].text);

    logforge::set_default(0);
}

} // namespace
