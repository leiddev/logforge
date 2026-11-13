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

// set_current() is the scope-less form: it has to write the same slot the macros
// read, and hand back what was there so the caller can put it back.
TEST_F(AmbientTest, SetCurrentBindsAndReturnsThePrevious) {
    SilentLogger fallback;
    SilentLogger first;
    SilentLogger second;
    logforge::set_default(&fallback);

    EXPECT_EQ(static_cast<logforge::Logger *>(0), logforge::set_current(&first));
    EXPECT_EQ(&first, logforge::current());

    EXPECT_EQ(&first, logforge::set_current(&second));
    EXPECT_EQ(&second, logforge::current());

    EXPECT_EQ(&second, logforge::set_current(0));
    EXPECT_EQ(&fallback, logforge::current());
}

// A ScopedBinding built while set_current() is in force restores what
// set_current() left behind, not the process default.
TEST_F(AmbientTest, ScopedBindingRestoresASetCurrentBinding) {
    SilentLogger outer;
    SilentLogger inner;

    logforge::set_current(&outer);
    {
        const logforge::ScopedBinding binding(&inner);
        EXPECT_EQ(&inner, logforge::current());
    }
    EXPECT_EQ(&outer, logforge::current());
    logforge::set_current(0);
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

// The documentation promises the binding is restored on every exit path,
// "including an exception". An unwind is exactly the path a scope guard written
// with a bare assignment instead of a destructor would get wrong, and it is the
// path a per-object logger is most likely to be used on -- inside a callback
// that throws.
TEST_F(AmbientTest, ABindingIsRestoredWhenTheScopeThrows) {
    SilentLogger fallback;
    SilentLogger bound;
    logforge::set_default(&fallback);

    try {
        const logforge::ScopedBinding binding(&bound);
        EXPECT_EQ(&bound, logforge::current());
        throw 7;
    } catch (int) {
    }

    EXPECT_EQ(&fallback, logforge::current());
}

// A binding on top of a binding that throws restores the outer one, not the
// default: each ScopedBinding remembers what was current when it was built, and
// both unwind in the order they were built.
TEST_F(AmbientTest, NestedBindingsUnwindInOrderWhenTheInnerThrows) {
    SilentLogger fallback;
    SilentLogger outer;
    SilentLogger inner;
    logforge::set_default(&fallback);

    try {
        const logforge::ScopedBinding first(&outer);
        const logforge::ScopedBinding second(&inner);
        EXPECT_EQ(&inner, logforge::current());
        throw 7;
    } catch (int) {
    }

    EXPECT_EQ(&fallback, logforge::current());
}

// set_default(nullptr) puts the process back to "nobody is listening", which is
// the state the destructor of a backend restores. After the thread binding is
// gone too, current() has to be null rather than the last logger that was set.
TEST_F(AmbientTest, ClearingTheDefaultLeavesNothingCurrent) {
    SilentLogger logger;
    logforge::set_default(&logger);
    {
        const logforge::ScopedBinding binding(&logger);
        logforge::set_default(0);
    }
    EXPECT_EQ(static_cast<logforge::Logger *>(0), logforge::current());
}

} // namespace
