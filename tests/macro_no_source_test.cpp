// Built with LOGFORGE_SOURCE_LOCATION=0 by the test target that includes it.
//
// A build that wants __FILE__ and __LINE__ out of the binary -- a small embedded
// release, where the paths are long -- has to get records with no location
// rather than records pointing at the wrong place, so the macros must pass a
// literal null through the same parameter.

#include <logforge/ambient.hpp>
#include <logforge/macros.hpp>

#include <gtest/gtest.h>

#include <string>

namespace {

class RecordingLogger : public logforge::Logger {
public:
    bool enabled(logforge::Level level) const override { return level != logforge::Level::off; }

    void log(const logforge::Record &record) override {
        file = record.file;
        line = record.line;
        text.assign(record.text, record.size);
    }

    const char *file;
    int line;
    std::string text;
};

TEST(MacroNoSourceTest, LocationIsCompiledOut) {
    RecordingLogger recorder;
    logforge::set_default(&recorder);

    LOGFORGE_LOG_INFO("no location {}", 1);

    EXPECT_EQ(static_cast<const char *>(0), recorder.file);
    EXPECT_EQ(0, recorder.line);
    EXPECT_EQ("no location 1", recorder.text);
}

} // namespace
