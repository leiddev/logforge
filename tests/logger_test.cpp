#include <logforge/logger.hpp>
#include <logforge/null_logger.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

struct Captured {
    logforge::Level level;
    std::string text;
    std::string component;
    std::string file;
    int line;
};

// A backend that keeps what it is given, so the tests can assert on the exact
// bytes and metadata a producer produced.
class RecordingLogger : public logforge::Logger {
public:
    bool enabled(logforge::Level) const override { return true; }

    void log(const logforge::Record &record) override {
        Captured entry;
        entry.level = record.level;
        entry.text.assign(record.text, record.size);
        entry.component = record.component != 0 ? record.component : "";
        entry.file = record.file != 0 ? record.file : "";
        entry.line = record.line;
        captured.push_back(entry);
    }

    std::vector<Captured> captured;
};

TEST(RecordTest, OptionalFieldsDefaultToNothing) {
    const logforge::Record record = logforge::make_record(logforge::Level::warn, "x", 1);
    EXPECT_EQ(logforge::Level::warn, record.level);
    EXPECT_EQ(1u, record.size);
    EXPECT_EQ(static_cast<const char *>(0), record.component);
    EXPECT_EQ(static_cast<const char *>(0), record.file);
    EXPECT_EQ(0, record.line);
}

TEST(LoggerTest, LogToStringKeepsTheLevel) {
    RecordingLogger logger;
    logforge::log_to(&logger, logforge::Level::error, "boom");

    ASSERT_EQ(1u, logger.captured.size());
    EXPECT_EQ(logforge::Level::error, logger.captured[0].level);
    EXPECT_EQ("boom", logger.captured[0].text);
}

// The interface carries an explicit length, so an embedded zero is a normal
// byte and not the end of the message.  A std::string convenience overload that
// went through c_str() would silently truncate here.
TEST(LoggerTest, EmbeddedZeroIsPreserved) {
    RecordingLogger logger;
    logforge::log_to(&logger, logforge::Level::info, "a\0b", 3);

    ASSERT_EQ(1u, logger.captured.size());
    EXPECT_EQ(3u, logger.captured[0].text.size());
    EXPECT_EQ(std::string("a\0b", 3), logger.captured[0].text);
}

TEST(LoggerTest, MetadataReachesTheBackend) {
    RecordingLogger logger;
    logforge::log_to(&logger, logforge::Level::debug, "tick", 4, "udp", "element.cpp", 42);

    ASSERT_EQ(1u, logger.captured.size());
    EXPECT_EQ("udp", logger.captured[0].component);
    EXPECT_EQ("element.cpp", logger.captured[0].file);
    EXPECT_EQ(42, logger.captured[0].line);
}

TEST(NullLoggerTest, IsDisabledAtEveryLevel) {
    logforge::NullLogger logger;
    EXPECT_FALSE(logger.enabled(logforge::Level::trace));
    EXPECT_FALSE(logger.enabled(logforge::Level::info));
    EXPECT_FALSE(logger.enabled(logforge::Level::critical));
    EXPECT_FALSE(logger.enabled(logforge::Level::off));
}

TEST(NullLoggerTest, AcceptsRecordsWithoutSideEffects) {
    logforge::NullLogger logger;
    logforge::log_to(&logger, logforge::Level::critical, "nothing happens");
    SUCCEED();
}

// Backends are held polymorphically; a non-virtual destructor here would leak
// whatever a real backend owns.
TEST(LoggerTest, DestructorIsVirtual) {
    bool destroyed = false;

    class Probe : public logforge::Logger {
    public:
        explicit Probe(bool &flag) : flag_(flag) {}
        ~Probe() override { flag_ = true; }
        bool enabled(logforge::Level) const override { return false; }
        void log(const logforge::Record &) override {}

    private:
        bool &flag_;
    };

    logforge::Logger *logger = new Probe(destroyed);
    delete logger;
    EXPECT_TRUE(destroyed);
}

} // namespace
