#include <logforge/level.hpp>
#include <logforge/version.hpp>

#include <cstdio>
#include <cstring>

// Uses only what an installed logforge exposes.  If this compiles and runs, the
// package config, the exported target, the include layout and the generated
// version header are all usable from outside the source tree.
int main() {
    const logforge::Level levels[] = {logforge::Level::trace, logforge::Level::debug,
                                      logforge::Level::info,  logforge::Level::warn,
                                      logforge::Level::error, logforge::Level::critical,
                                      logforge::Level::off};

    int failures = 0;
    for (const logforge::Level level : levels) {
        const char *text = logforge::to_string(level);
        if (text == 0 || std::strlen(text) == 0) {
            std::printf("level %d has no name\n", static_cast<int>(level));
            ++failures;
        }
    }

    if (failures != 0) {
        return 1;
    }

    std::printf("logforge %s: 7 levels, all named\n", LOGFORGE_VERSION_STRING);
    return 0;
}
