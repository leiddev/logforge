#include <logforge/level.hpp>
#include <logforge/version.hpp>

#include <gtest/gtest.h>

#include <string>

namespace {

TEST(LevelTest, ToStringCoversEveryLevel) {
    EXPECT_STREQ("trace", logforge::to_string(logforge::Level::trace));
    EXPECT_STREQ("debug", logforge::to_string(logforge::Level::debug));
    EXPECT_STREQ("info", logforge::to_string(logforge::Level::info));
    EXPECT_STREQ("warn", logforge::to_string(logforge::Level::warn));
    EXPECT_STREQ("error", logforge::to_string(logforge::Level::error));
    EXPECT_STREQ("critical", logforge::to_string(logforge::Level::critical));
    EXPECT_STREQ("off", logforge::to_string(logforge::Level::off));
}

// The ordering is part of the contract: filters are implemented as a single
// comparison, so it has to survive future edits.
TEST(LevelTest, OrderingIsAscendingBySeverity) {
    EXPECT_LT(logforge::Level::trace, logforge::Level::debug);
    EXPECT_LT(logforge::Level::debug, logforge::Level::info);
    EXPECT_LT(logforge::Level::info, logforge::Level::warn);
    EXPECT_LT(logforge::Level::warn, logforge::Level::error);
    EXPECT_LT(logforge::Level::error, logforge::Level::critical);
    EXPECT_LT(logforge::Level::critical, logforge::Level::off);
}

// Guards the generated header against a configure_file() that silently does
// nothing, which would otherwise surface as a link error much later.
TEST(VersionTest, HeaderIsGenerated) {
    EXPECT_GT(LOGFORGE_VERSION_MAJOR, -1);
    EXPECT_FALSE(std::string(LOGFORGE_VERSION_STRING).empty());
}

TEST(LevelTest, ParseLevelAcceptsEveryCanonicalName) {
    logforge::Level level = logforge::Level::off;

    EXPECT_TRUE(logforge::parse_level("trace", level));
    EXPECT_EQ(logforge::Level::trace, level);
    EXPECT_TRUE(logforge::parse_level("debug", level));
    EXPECT_EQ(logforge::Level::debug, level);
    EXPECT_TRUE(logforge::parse_level("info", level));
    EXPECT_EQ(logforge::Level::info, level);
    EXPECT_TRUE(logforge::parse_level("warn", level));
    EXPECT_EQ(logforge::Level::warn, level);
    EXPECT_TRUE(logforge::parse_level("error", level));
    EXPECT_EQ(logforge::Level::error, level);
    EXPECT_TRUE(logforge::parse_level("critical", level));
    EXPECT_EQ(logforge::Level::critical, level);
    EXPECT_TRUE(logforge::parse_level("off", level));
    EXPECT_EQ(logforge::Level::off, level);
}

// The aliases are the ones that appear in configuration files that grew up on
// other frameworks.
TEST(LevelTest, ParseLevelAcceptsTheAliases) {
    logforge::Level level = logforge::Level::info;

    EXPECT_TRUE(logforge::parse_level("warning", level));
    EXPECT_EQ(logforge::Level::warn, level);
    EXPECT_TRUE(logforge::parse_level("err", level));
    EXPECT_EQ(logforge::Level::error, level);
    EXPECT_TRUE(logforge::parse_level("fatal", level));
    EXPECT_EQ(logforge::Level::critical, level);
}

TEST(LevelTest, ParseLevelIsCaseInsensitiveAndStrict) {
    logforge::Level level = logforge::Level::info;

    EXPECT_TRUE(logforge::parse_level("INFO", level));
    EXPECT_EQ(logforge::Level::info, level);
    EXPECT_TRUE(logforge::parse_level("Warning", level));
    EXPECT_EQ(logforge::Level::warn, level);

    // A prefix is not a level, and a failure leaves the caller's default alone.
    EXPECT_FALSE(logforge::parse_level("infoo", level));
    EXPECT_FALSE(logforge::parse_level("inf", level));
    EXPECT_FALSE(logforge::parse_level("", level));
    EXPECT_FALSE(logforge::parse_level(static_cast<const char *>(0), level));
    EXPECT_EQ(logforge::Level::warn, level);
}

} // namespace
