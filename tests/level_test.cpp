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

} // namespace
