#ifndef LOGFORGE_FORMAT_HPP
#define LOGFORGE_FORMAT_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Rendering a log line: a format text with a brace where each argument goes.
//
//   format_message("pad {} refused {} message(s)", name, count);
//
// This lives in the front end, next to the macros, and not in a backend.  By the
// time a record reaches Logger::log() it has to be text anyway, because that is
// what every sink ultimately writes, and keeping the renderer here means one
// implementation is shared by every producer instead of one per backend.
//
// The accepted surface is the one the legacy call sites use:
//
//   {}          the argument as text
//   {:4}        at least four characters; a number is padded on the left, text
//               on the right, and a wider argument is never cut
//   {:x} {:X}   an integer in hexadecimal, lower or upper case
//   {:04x}      the same, padded with zeros to four characters
//   {:.2f}      a value with two digits after the point
//   {:6.2f}     both
//   {{  }}      a brace
//
// Two rules make this safe to call from anywhere:
//
//   * A specifier that does not apply to its argument is ignored, so {:x} on a
//     string prints the string and {:.2f} on an integer prints the integer.
//   * Anything else between braces -- a malformed specifier, a width above 512,
//     a precision above 64 -- is copied exactly as written, and a message with
//     fewer arguments than braces keeps the braces that have none.
//
// So a message always comes out whole: a mistake shows up in the text instead of
// throwing, which is what fmt would do in an element thread.
//
// Numbers go out the way jsonlens writes them: an integer as an integer, and a
// floating point value as the shortest of 15 and 17 significant digits that
// reads back as the same value.  fmt prints the shortest form of all, so the two
// agree on ordinary values and can differ in the last digit of a value that
// needs all 17.
//
// The argument types are the ones legacy call sites passed: text as const char*
// or std::string, char, bool, every integer width and every floating point type.
// A type with no log text is a compile error that asks for a cast, which is also
// what spdlog requires of an enum.

namespace logforge {
namespace detail {

// The widest padding a specifier may ask for, and the longest fraction.  Both
// exist so that a wrong specifier cannot make a message allocate without end.
const unsigned width_limit = 512;
const unsigned precision_limit = 64;

// What a placeholder asked for.  A field means something only for the argument
// kinds that can use it.
struct Spec {
    Spec() : width(0), precision(0), hex(false), upper(false), fixed(false), zero(false) {}

    unsigned width;
    unsigned precision;
    bool hex;
    bool upper;
    bool fixed;
    bool zero;
};

// Reads the text between the braces.  False means "not a specifier this
// formatter understands", which leaves the braces to be copied as they are.
inline bool parse_spec(const std::string &spec, Spec &out) {
    std::size_t index = 0;
    // The colon belongs to the specifier, and a colon with nothing after it is
    // not one: fmt says the same, and the legacy text never used a bare colon.
    if (index < spec.size() && spec[index] == ':') {
        ++index;
        if (index == spec.size()) {
            return false;
        }
    }
    // A leading zero asks for zero padding, as in {:04x}.
    if (index + 1 < spec.size() && spec[index] == '0' && spec[index + 1] >= '0' &&
        spec[index + 1] <= '9') {
        out.zero = true;
        ++index;
    }
    while (index < spec.size() && spec[index] >= '0' && spec[index] <= '9') {
        out.width = out.width * 10 + static_cast<unsigned>(spec[index] - '0');
        if (out.width > width_limit) {
            return false;
        }
        ++index;
    }
    if (index < spec.size() && spec[index] == '.') {
        ++index;
        const std::size_t digits = index;
        while (index < spec.size() && spec[index] >= '0' && spec[index] <= '9') {
            out.precision = out.precision * 10 + static_cast<unsigned>(spec[index] - '0');
            if (out.precision > precision_limit) {
                return false;
            }
            ++index;
        }
        if (index == digits) {
            return false;
        }
    }
    if (index < spec.size()) {
        if (spec[index] == 'x') {
            out.hex = true;
        } else if (spec[index] == 'X') {
            out.hex = true;
            out.upper = true;
        } else if (spec[index] == 'f') {
            out.fixed = true;
        } else {
            return false;
        }
        ++index;
    }
    return index == spec.size();
}

// Appends text padded to a width: on the left for numbers, on the right for
// everything else.
inline void append_padded(std::string &out, const std::string &text, unsigned width,
                          bool right_aligned) {
    if (text.size() >= width) {
        out += text;
        return;
    }
    const std::size_t padding = static_cast<std::size_t>(width) - text.size();
    if (right_aligned) {
        out.append(padding, ' ');
    }
    out += text;
    if (!right_aligned) {
        out.append(padding, ' ');
    }
}

// Pads a number with zeros after its sign, which is what the leading zero of a
// specifier asks for, as in {:04x}.
inline void append_zero_padded(std::string &out, const std::string &text, unsigned width) {
    std::size_t first = 0;
    if (!text.empty() && text[0] == '-') {
        out += '-';
        first = 1;
    }
    const std::size_t length = text.size() - first;
    if (length < width) {
        out.append(static_cast<std::size_t>(width) - length, '0');
    }
    out.append(text, first, std::string::npos);
}

// Text of a floating point value: the shortest of 15 and 17 significant digits
// that reads back as the same value.
inline void append_number_text(std::string &out, double value) {
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%.15g", value);
    if (std::strtod(buffer, 0) != value) {
        std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    }
    out += buffer;
}

inline void append_number_text(std::string &out, long long value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%lld", value);
    out += buffer;
}

inline void append_number_text(std::string &out, unsigned long long value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%llu", value);
    out += buffer;
}

inline void append_hex_text(std::string &out, unsigned long long value, bool upper) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), upper ? "%llX" : "%llx", value);
    out += buffer;
}

inline void append_argument(std::string &out, const Spec &spec, const char *value) {
    append_padded(out, value != 0 ? std::string(value) : std::string("(null)"), spec.width, false);
}

// A text written out literally goes through the same path whether it arrives as
// a pointer or as an array.
inline void append_argument(std::string &out, const Spec &spec, char *value) {
    append_argument(out, spec, static_cast<const char *>(value));
}

template <std::size_t N> void append_argument(std::string &out, const Spec &spec, const char (&value)[N]) {
    append_argument(out, spec, static_cast<const char *>(value));
}

template <std::size_t N> void append_argument(std::string &out, const Spec &spec, char (&value)[N]) {
    append_argument(out, spec, static_cast<const char *>(value));
}

inline void append_argument(std::string &out, const Spec &spec, const std::string &value) {
    append_padded(out, value, spec.width, false);
}

inline void append_argument(std::string &out, const Spec &spec, char value) {
    append_padded(out, std::string(1, value), spec.width, false);
}

inline void append_argument(std::string &out, const Spec &spec, bool value) {
    append_padded(out, value ? std::string("true") : std::string("false"), spec.width, false);
}

inline void append_argument(std::string &out, const Spec &spec, double value) {
    std::string text;
    if (spec.fixed) {
        // Worst case for %f is a sign, 309 integer digits (the largest double is
        // 1.8e308), a point, the precision and a terminator.  A buffer sized for
        // the precision alone would silently cut a large value in half.
        char buffer[400];
        std::snprintf(buffer, sizeof(buffer), "%.*f", static_cast<int>(spec.precision), value);
        text = buffer;
    } else {
        append_number_text(text, value);
    }
    append_padded(out, text, spec.width, true);
}

inline void append_argument(std::string &out, const Spec &spec, float value) {
    append_argument(out, spec, static_cast<double>(value));
}

inline void append_argument(std::string &out, const Spec &spec, long long value) {
    std::string text;
    if (spec.hex) {
        append_hex_text(text, static_cast<unsigned long long>(value), spec.upper);
    } else {
        append_number_text(text, value);
    }
    if (spec.zero && spec.width > 0) {
        append_zero_padded(out, text, spec.width);
    } else {
        append_padded(out, text, spec.width, true);
    }
}

inline void append_argument(std::string &out, const Spec &spec, unsigned long long value) {
    std::string text;
    if (spec.hex) {
        append_hex_text(text, value, spec.upper);
    } else {
        append_number_text(text, value);
    }
    if (spec.zero && spec.width > 0) {
        append_zero_padded(out, text, spec.width);
    } else {
        append_padded(out, text, spec.width, true);
    }
}

// Every integer width goes through the two forms above, so a value is never
// printed as a character by accident and signed char is a number.
inline void append_argument(std::string &out, const Spec &spec, signed char value) {
    append_argument(out, spec, static_cast<long long>(value));
}

inline void append_argument(std::string &out, const Spec &spec, unsigned char value) {
    append_argument(out, spec, static_cast<unsigned long long>(value));
}

inline void append_argument(std::string &out, const Spec &spec, short value) {
    append_argument(out, spec, static_cast<long long>(value));
}

inline void append_argument(std::string &out, const Spec &spec, unsigned short value) {
    append_argument(out, spec, static_cast<unsigned long long>(value));
}

inline void append_argument(std::string &out, const Spec &spec, int value) {
    append_argument(out, spec, static_cast<long long>(value));
}

inline void append_argument(std::string &out, const Spec &spec, unsigned int value) {
    append_argument(out, spec, static_cast<unsigned long long>(value));
}

inline void append_argument(std::string &out, const Spec &spec, long value) {
    append_argument(out, spec, static_cast<long long>(value));
}

inline void append_argument(std::string &out, const Spec &spec, unsigned long value) {
    append_argument(out, spec, static_cast<unsigned long long>(value));
}

// A pointer that is not text prints as an address, spelled the same way on every
// platform, which %p does not do.
template <typename T> void append_argument(std::string &out, const Spec &spec, T *value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%llx",
                  static_cast<unsigned long long>(reinterpret_cast<std::size_t>(value)));
    append_padded(out, buffer, spec.width, true);
}

// A type this formatter has no text for.  Saying so at compile time is the only
// way to keep a call site honest; spdlog rejects an enum the same way.
template <typename T> void append_argument(std::string &out, const Spec &spec, const T &value) {
    (void)out;
    (void)spec;
    (void)value;
    static_assert(sizeof(T) == 0, "logforge: this type has no log text; cast it to an integer, a "
                                  "floating point value or a string at the call site");
}

// Appends argument number `index` of the pack.  Returns false when the pack has
// no argument at that position, which the caller reports by keeping the braces.
inline bool append_argument_at(std::string &out, std::size_t index, const Spec &spec) {
    (void)out;
    (void)index;
    (void)spec;
    return false;
}

template <typename T, typename... Rest>
bool append_argument_at(std::string &out, std::size_t index, const Spec &spec, const T &value,
                        const Rest &...rest) {
    if (index == 0) {
        append_argument(out, spec, value);
        return true;
    }
    return append_argument_at(out, index - 1, spec, rest...);
}

} // namespace detail

// Renders one log message.  A null text gives an empty message rather than a
// crash: a line lost to a null pointer is worse than a line lost to a bad
// format.
template <typename... Args> std::string format_message(const char *text, const Args &...args) {
    std::string out;
    if (text == 0) {
        return out;
    }
    const std::size_t length = std::strlen(text);
    std::size_t next = 0;
    std::size_t index = 0;
    while (index < length) {
        const char character = text[index];
        if (character == '{' && index + 1 < length && text[index + 1] == '{') {
            out.push_back('{');
            index += 2;
            continue;
        }
        if (character == '}' && index + 1 < length && text[index + 1] == '}') {
            out.push_back('}');
            index += 2;
            continue;
        }
        if (character != '{') {
            out.push_back(character);
            ++index;
            continue;
        }
        std::size_t close = index + 1;
        while (close < length && text[close] != '}') {
            ++close;
        }
        if (close >= length) {
            // No closing brace: the rest of the text is text.
            out.push_back(character);
            ++index;
            continue;
        }
        detail::Spec spec;
        const std::string written(text + index + 1, close - index - 1);
        if (!detail::parse_spec(written, spec)) {
            out.append(text + index, close - index + 1);
            index = close + 1;
            continue;
        }
        if (!detail::append_argument_at(out, next, spec, args...)) {
            // More braces than arguments: the braces stay, so the message shows
            // what was missing instead of dropping it.
            out.append(text + index, close - index + 1);
        } else {
            ++next;
        }
        index = close + 1;
    }
    return out;
}

} // namespace logforge

#endif // LOGFORGE_FORMAT_HPP
