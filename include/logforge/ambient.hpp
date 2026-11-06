#ifndef LOGFORGE_AMBIENT_HPP
#define LOGFORGE_AMBIENT_HPP

#include <logforge/logger.hpp>

#include <atomic>

namespace logforge {

// Where a log statement goes when it does not name a logger itself.
//
// Resolution order, per thread:
//   1. the innermost ScopedBinding active on this thread;
//   2. the process-wide default installed with set_default();
//   3. nullptr, which every caller must treat as "nobody is listening".
//
// That third case is why `current()` returns a pointer rather than a reference
// to a null logger: an owner may want to skip building a message when there is
// no destination at all, and a null check is honest about it.
//
// Step 1 is what makes per-object routing work.  A runtime that hands each of
// its objects a logger can bind that logger for the duration of a callback, and
// code inside the callback that knows nothing about the runtime -- a macro in
// some third-party header, say -- ends up writing to the right place.

namespace detail {

// Function-local statics (not namespace-scope `thread_local`) so this stays
// header-only under C++11: only a function may be inline-defined without
// requiring an out-of-line definition somewhere.
inline Logger *&current_slot() {
    static thread_local Logger *slot = 0;
    return slot;
}

inline std::atomic<Logger *> &default_slot() {
    static std::atomic<Logger *> slot(static_cast<Logger *>(0));
    return slot;
}

} // namespace detail

// The logger bound to this thread, or the process default, or nullptr.
inline Logger *current() {
    Logger *const bound = detail::current_slot();
    return bound != 0 ? bound : detail::default_slot().load(std::memory_order_acquire);
}

// The process-wide fallback, ignoring any thread binding.
inline Logger *default_logger() {
    return detail::default_slot().load(std::memory_order_acquire);
}

// Installs the process-wide fallback.  Passing nullptr removes it.
inline void set_default(Logger *logger) {
    detail::default_slot().store(logger, std::memory_order_release);
}

// Binds a logger to the current thread until something else replaces it, and
// returns whichever logger was bound before.
//
// ScopedBinding is the spelling to prefer: it restores the previous binding on
// every exit path, including an exception.  This exists for the two cases that
// cannot use a scope -- a compatibility shim keeping a "set and hand back the
// previous one" signature, and a test that wants to leave the thread unbound.
inline Logger *set_current(Logger *logger) {
    Logger *const previous = detail::current_slot();
    detail::current_slot() = logger;
    return previous;
}

// Binds a logger to the current thread for the lifetime of the object.
//
// Nesting is supported: each binding restores whichever logger was current when
// it was constructed.  Binding nullptr does not mean "silence", it means
// "no thread-specific logger", so `current()` will fall back to the process
// default; bind a NullLogger to silence a scope on purpose.
class ScopedBinding {
public:
    explicit ScopedBinding(Logger *logger) : previous_(set_current(logger)) {}

    ~ScopedBinding() { detail::current_slot() = previous_; }

private:
    ScopedBinding(const ScopedBinding &);
    ScopedBinding &operator=(const ScopedBinding &);

    Logger *previous_;
};

} // namespace logforge

#endif // LOGFORGE_AMBIENT_HPP
