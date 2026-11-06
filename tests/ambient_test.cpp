#include <logforge/ambient.hpp>
#include <logforge/null_logger.hpp>

#include <gtest/gtest.h>

#include <thread>

namespace {

class AmbientTest : public ::testing::Test {
protected:
    // The default logger is process-wide state; leaving it behind would make
    // the result depend on test order.
    void SetUp() override { logforge::set_default(0); }
    void TearDown() override { logforge::set_default(0); }
};

class SilentLogger : public logforge::Logger {
public:
    bool enabled(logforge::Level) const override { return false; }
    void log(const logforge::Record &) override {}
};

TEST_F(AmbientTest, NothingIsCurrentUntilSomethingIsInstalled) {
    EXPECT_EQ(static_cast<logforge::Logger *>(0), logforge::default_logger());
    EXPECT_EQ(static_cast<logforge::Logger *>(0), logforge::current());
}

TEST_F(AmbientTest, DefaultIsWhatCurrentReturns) {
    SilentLogger logger;
    logforge::set_default(&logger);

    EXPECT_EQ(&logger, logforge::default_logger());
    EXPECT_EQ(&logger, logforge::current());
}

TEST_F(AmbientTest, BindingOverridesTheDefault) {
    SilentLogger fallback;
    SilentLogger bound;
    logforge::set_default(&fallback);

    {
        const logforge::ScopedBinding binding(&bound);
        EXPECT_EQ(&bound, logforge::current());
        // The process-wide slot is untouched, not replaced.
        EXPECT_EQ(&fallback, logforge::default_logger());
    }

    EXPECT_EQ(&fallback, logforge::current());
}

TEST_F(AmbientTest, BindingsNestAndUnwind) {
    SilentLogger outer;
    SilentLogger inner;

    const logforge::ScopedBinding first(&outer);
    {
        const logforge::ScopedBinding second(&inner);
        EXPECT_EQ(&inner, logforge::current());
    }
    EXPECT_EQ(&outer, logforge::current());
}

// Binding nothing means "no thread-specific logger", which is not the same as
// "silence": the process default still applies.
TEST_F(AmbientTest, BindingNullFallsBackToTheDefault) {
    SilentLogger fallback;
    logforge::set_default(&fallback);

    const logforge::ScopedBinding binding(0);
    EXPECT_EQ(&fallback, logforge::current());
}

TEST_F(AmbientTest, NullLoggerSilencesAScopeOnPurpose) {
    logforge::NullLogger silence;
    SilentLogger fallback;
    logforge::set_default(&fallback);

    const logforge::ScopedBinding binding(&silence);
    ASSERT_EQ(&silence, logforge::current());
    EXPECT_FALSE(logforge::current()->enabled(logforge::Level::critical));
}

// The whole point of the binding being thread_local: one object's callback must
// not redirect another thread's log lines.
TEST_F(AmbientTest, BindingIsInvisibleToOtherThreads) {
    SilentLogger fallback;
    SilentLogger bound;
    logforge::set_default(&fallback);

    const logforge::ScopedBinding binding(&bound);

    logforge::Logger *seen = 0;
    std::thread worker([&seen]() { seen = logforge::current(); });
    worker.join();

    EXPECT_EQ(&fallback, seen);
    EXPECT_EQ(&bound, logforge::current());
}

} // namespace
