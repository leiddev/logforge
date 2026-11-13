#include <logforge/ambient.hpp>
#include <logforge/compat/legacy_names.hpp>
#include <logforge/macros.hpp>
#include <logforge/null_logger.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

struct Captured {
    logforge::Level level;
    std::string text;
    std::string component;
    const char *file;
    int line;
};

class RecordingLogger : public logforge::Logger {
public:
    bool enabled(logforge::Level level) const override { return level != logforge::Level::off; }

    void log(const logforge::Record &record) override {
        Captured entry;
        entry.level = record.level;
        entry.text.assign(record.text, record.size);
        entry.component = record.component != 0 ? record.component : "";
        entry.file = record.file;
        entry.line = record.line;
        captured.push_back(entry);
    }

    std::vector<Captured> captured;
};

class MacroTest : public ::testing::Test {
protected:
    void SetUp() override { logforge::set_default(0); }
    void TearDown() override { logforge::set_default(0); }
};

TEST_F(MacroTest, CurrentLoggerReceivesTheRenderedMessage) {
    RecordingLogger recorder;
    logforge::set_default(&recorder);

    LOGFORGE_LOG_INFO("pad {} refused {} message(s)", "in", 3);

    ASSERT_EQ(1u, recorder.captured.size());
    EXPECT_EQ(logforge::Level::info, recorder.captured[0].level);
    EXPECT_EQ("pad in refused 3 message(s)", recorder.captured[0].text);
    EXPECT_EQ("", recorder.captured[0].component);

    // Source location is on by default, and it has to point at this file.
    ASSERT_NE(static_cast<const char *>(0), recorder.captured[0].file);
    EXPECT_NE(std::string::npos, std::string(recorder.captured[0].file).find("macro_test"));
    EXPECT_GT(recorder.captured[0].line, 0);
}

TEST_F(MacroTest, EachLevelMacroReportsItsOwnLevel) {
    RecordingLogger recorder;
    logforge::set_default(&recorder);

    LOGFORGE_LOG_TRACE("t");
    LOGFORGE_LOG_DEBUG("d");
    LOGFORGE_LOG_INFO("i");
    LOGFORGE_LOG_WARN("w");
    LOGFORGE_LOG_ERROR("e");
    LOGFORGE_LOG_CRITICAL("c");

    ASSERT_EQ(6u, recorder.captured.size());
    EXPECT_EQ(logforge::Level::trace, recorder.captured[0].level);
    EXPECT_EQ(logforge::Level::debug, recorder.captured[1].level);
    EXPECT_EQ(logforge::Level::info, recorder.captured[2].level);
    EXPECT_EQ(logforge::Level::warn, recorder.captured[3].level);
    EXPECT_EQ(logforge::Level::error, recorder.captured[4].level);
    EXPECT_EQ(logforge::Level::critical, recorder.captured[5].level);
}

// The point of checking the level before rendering: a call site can pass an
// expensive expression to a statement that is turned off, and nothing happens.
TEST_F(MacroTest, DisabledStatementDoesNotEvaluateItsArguments) {
    int evaluations = 0;
    const auto counted = [&evaluations]() {
        ++evaluations;
        return 7;
    };

    logforge::NullLogger silence;
    LOGFORGE_LOG_AT(&silence, logforge::Level::critical, "value {}", counted());
    EXPECT_EQ(0, evaluations);

    RecordingLogger recorder;
    LOGFORGE_LOG_AT(&recorder, logforge::Level::info, "value {}", counted());
    EXPECT_EQ(1, evaluations);
    ASSERT_EQ(1u, recorder.captured.size());
    EXPECT_EQ("value 7", recorder.captured[0].text);
}

TEST_F(MacroTest, AnExplicitLoggerIgnoresTheThreadBinding) {
    RecordingLogger owned;
    RecordingLogger bound;
    logforge::set_default(&bound);

    {
        const logforge::ScopedBinding binding(&bound);
        LOGFORGE_LOG_AT(&owned, logforge::Level::warn, "straight to the owner");
        EXPECT_EQ(0u, bound.captured.size());
    }

    ASSERT_EQ(1u, owned.captured.size());
    EXPECT_EQ("straight to the owner", owned.captured[0].text);
}

TEST_F(MacroTest, ATagReachesTheBackendAsAComponent) {
    RecordingLogger recorder;
    logforge::set_default(&recorder);

    LOGFORGE_LOG_TAGGED("udp", logforge::Level::warn, "socket closed");

    ASSERT_EQ(1u, recorder.captured.size());
    EXPECT_EQ("udp", recorder.captured[0].component);
    EXPECT_EQ("socket closed", recorder.captured[0].text);
}

// The macro uses the logger and the level more than once -- a null check, a
// level check and the dispatch -- so a call site may pass an expression with a
// side effect in either position only because each is evaluated exactly once.
TEST_F(MacroTest, TheLoggerAndTheLevelAreEvaluatedOnce) {
    RecordingLogger recorder;
    int logger_evaluations = 0;
    int level_evaluations = 0;

    LOGFORGE_LOG_AT((++logger_evaluations, static_cast<logforge::Logger *>(&recorder)),
                    (++level_evaluations, logforge::Level::info), "once");

    EXPECT_EQ(1, logger_evaluations);
    EXPECT_EQ(1, level_evaluations);
    ASSERT_EQ(1u, recorder.captured.size());
    EXPECT_EQ(logforge::Level::info, recorder.captured[0].level);
}

// The same for LOGFORGE_LOG, whose logger is logforge::current() rather than
// the caller's expression: the level alone is the caller's.
TEST_F(MacroTest, TheLevelOfTheAmbientFormIsEvaluatedOnce) {
    RecordingLogger recorder;
    logforge::set_default(&recorder);
    int level_evaluations = 0;

    LOGFORGE_LOG((++level_evaluations, logforge::Level::warn), "ambient");

    EXPECT_EQ(1, level_evaluations);
    ASSERT_EQ(1u, recorder.captured.size());
    EXPECT_EQ(logforge::Level::warn, recorder.captured[0].level);
}

// The tagged form with an explicit logger: the one combination of the two
// dimensions the table in the documentation lists and no test had exercised.
TEST_F(MacroTest, AnExplicitLoggerAndATagTogether) {
    RecordingLogger owned;
    RecordingLogger bound;
    logforge::set_default(&bound);

    const logforge::ScopedBinding binding(&bound);
    LOGFORGE_LOG_AT_TAGGED(&owned, "udp", logforge::Level::error, "send failed: {}", 9);

    EXPECT_EQ(0u, bound.captured.size());
    ASSERT_EQ(1u, owned.captured.size());
    EXPECT_EQ(logforge::Level::error, owned.captured[0].level);
    EXPECT_EQ("udp", owned.captured[0].component);
    EXPECT_EQ("send failed: 9", owned.captured[0].text);
}

// A tag is an optional field, and a null one has to reach the backend as null
// rather than as a crash or an empty prefix.
TEST_F(MacroTest, ANullTagReachesTheBackendAsNull) {
    RecordingLogger recorder;
    logforge::set_default(&recorder);

    LOGFORGE_LOG_TAGGED(static_cast<const char *>(0), logforge::Level::info, "untagged");

    ASSERT_EQ(1u, recorder.captured.size());
    EXPECT_EQ("", recorder.captured[0].component);
    EXPECT_EQ("untagged", recorder.captured[0].text);
}

// The level is checked before the arguments are rendered, so a statement that is
// disabled costs one call to enabled() and nothing else. That was already
// proved by a counting argument; this fixes the other half of the promise: the
// message is not rendered either, which is the only other work a disabled
// statement could be doing.
TEST_F(MacroTest, ADisabledStatementDoesNotCallLog) {
    class PickyLogger : public logforge::Logger {
    public:
        bool enabled(logforge::Level) const override { return false; }
        void log(const logforge::Record &) override { ++calls; }
        int calls = 0;
    };

    PickyLogger picky;
    LOGFORGE_LOG_AT(&picky, logforge::Level::info, "value {:512}", 7);
    EXPECT_EQ(0, picky.calls);
}

// A message with more braces than arguments keeps the braces that have none;
// the macro still delivers exactly one record.
TEST_F(MacroTest, AFewerArgumentsThanBracesStillLogsOneRecord) {
    RecordingLogger recorder;
    logforge::set_default(&recorder);

    LOGFORGE_LOG_INFO("{} of {} pads", 3);

    ASSERT_EQ(1u, recorder.captured.size());
    EXPECT_EQ("3 of {} pads", recorder.captured[0].text);
}

TEST_F(MacroTest, NoLoggerIsANoOp) {
    EXPECT_EQ(static_cast<logforge::Logger *>(0), logforge::current());
    LOGFORGE_LOG_ERROR("nobody is listening {}", 1);
    SUCCEED();
}

// The legacy names come from an opt-in header, so this file includes it; a
// project that does not include it never sees these macros at all.
TEST_F(MacroTest, LegacyNamesReportTheSameLevels) {
    RecordingLogger recorder;
    logforge::set_default(&recorder);

    LOG_TRACE("t");
    LOG_DEBUG("d");
    LOG_INFO("i");
    LOG_WARN("w");
    LOG_ERR("e");
    LOG_CRITICAL("c");

    ASSERT_EQ(6u, recorder.captured.size());
    EXPECT_EQ(logforge::Level::trace, recorder.captured[0].level);
    EXPECT_EQ(logforge::Level::info, recorder.captured[2].level);
    EXPECT_EQ(logforge::Level::error, recorder.captured[4].level);
    EXPECT_EQ(logforge::Level::critical, recorder.captured[5].level);
    EXPECT_EQ("i", recorder.captured[2].text);
}

} // namespace
