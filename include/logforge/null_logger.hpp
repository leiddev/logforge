#ifndef LOGFORGE_NULL_LOGGER_HPP
#define LOGFORGE_NULL_LOGGER_HPP

#include <logforge/logger.hpp>

namespace logforge {

// Swallows everything.  Two uses: as the default before a real backend is
// installed, and as an explicit "silence this scope" binding.
//
// It is intentionally not the default logger -- see ambient.hpp.  A null
// default would hide the difference between "nobody installed a backend" and
// "the operator turned logging off".
class NullLogger : public Logger {
public:
    bool enabled(Level) const override { return false; }
    void log(const Record &) override {}
};

} // namespace logforge

#endif // LOGFORGE_NULL_LOGGER_HPP
