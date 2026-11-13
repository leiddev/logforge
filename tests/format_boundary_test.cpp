// The edges of the renderer: the limits a specifier may ask for, the braces a
// message may leave open, and the combinations of width, padding and type that
// format_test does not already cover.
//
// The rule every case here serves is the one format.hpp states: a message always
// comes out whole. Nothing throws, nothing is cut, and anything the renderer
// cannot fill is copied as it was written.

#include <logforge/format.hpp>

#include <gtest/gtest.h>

#include <string>

namespace {

// A width of exactly 512 is the limit, so it is accepted; 513 is not and the
// braces survive. Both sides of the constant, because an off-by-one here is a
// message that silently changes shape.
TEST(FormatBoundaryTest, TheWidthLimitIsInclusive) {
    EXPECT_EQ(std::string(511, ' ') + "0", logforge::format_message("{:512}", 0));
    EXPECT_EQ("{:513}", logforge::format_message("{:513}", 0));

    // Once the width is already over the limit, the rest of the specifier does
    // not get a second chance: the braces stay even though "x" would parse.
    EXPECT_EQ("{:513x}", logforge::format_message("{:513x}", 255));

    // Text is padded on the right, so the limit is observable on either side.
    EXPECT_EQ(std::string("x") + std::string(511, ' '), logforge::format_message("{:512}", "x"));
}

TEST(FormatBoundaryTest, ThePrecisionLimitIsInclusive) {
    const std::string sixty_four = logforge::format_message("{:.64f}", 0.0);
    EXPECT_EQ("0." + std::string(64, '0'), sixty_four);
    EXPECT_EQ("{:.65f}", logforge::format_message("{:.65f}", 0.0));
}

// A precision without a width, and both together: the two fields are parsed
// independently and a call site lines a column up with either.
TEST(FormatBoundaryTest, PrecisionAndWidthAreIndependent) {
    EXPECT_EQ("2.50", logforge::format_message("{:.2f}", 2.5));
    EXPECT_EQ("2.50", logforge::format_message("{:4.2f}", 2.5));
    EXPECT_EQ("    2.50", logforge::format_message("{:8.2f}", 2.5));
    // A float is a number, so a width narrower than its text never cuts it and
    // never pads it on the right either.
    EXPECT_EQ("2.5", logforge::format_message("{:1.1f}", 2.5));
}

// Hex with a width, with and without the zero flag. A number is padded on the
// left, so a fixed-width hex field is what a call site that lines up a table
// relies on.
TEST(FormatBoundaryTest, HexAndWidthTogether) {
    EXPECT_EQ("|  ff|", logforge::format_message("|{:4x}|", 255));
    EXPECT_EQ("|00ff|", logforge::format_message("|{:04x}|", 255));
    EXPECT_EQ("|  FF|", logforge::format_message("|{:4X}|", 255));
    // A negative value prints as its bits, and a width narrower than those bits
    // never cuts them.
    EXPECT_EQ("FFFFFFFFFFFFFFFF", logforge::format_message("{:4X}", -1));
}

TEST(FormatBoundaryTest, ZeroPaddingFollowsTheSign) {
    EXPECT_EQ("-0007", logforge::format_message("{:04}", -7));
    EXPECT_EQ("-00007", logforge::format_message("{:05}", -7));
    EXPECT_EQ("0007", logforge::format_message("{:04}", 7));
    // Without the flag the same width pads with spaces instead.
    EXPECT_EQ("   7", logforge::format_message("{:4}", 7));
    EXPECT_EQ("  -7", logforge::format_message("{:4}", -7));
    // A zero flag without a width has nothing to pad to.
    EXPECT_EQ("7", logforge::format_message("{:0}", 7));
    EXPECT_EQ("-7", logforge::format_message("{:0}", -7));
    // Zero padding never cuts a value that is already wider.
    EXPECT_EQ("12345", logforge::format_message("{:02}", 12345));
}

TEST(FormatBoundaryTest, AnUnclosedDoubleBraceIsCopiedAsItIs) {
    // Treating "{{" as an escape always leaves one brace, even when the second
    // character that would close the escape is not there.
    EXPECT_EQ("{", logforge::format_message("{{"));
    EXPECT_EQ("}", logforge::format_message("}}"));
    EXPECT_EQ("{", logforge::format_message("{"));

    // The escape runs first, so "{{7}}" is a literal "7" between braces and not
    // a placeholder: no argument is consumed by it.
    EXPECT_EQ("{7}", logforge::format_message("{{7}}"));

    // Empty braces are a placeholder too, even with nothing between them: a
    // "{}" with no argument to fill it keeps its braces, and one with an
    // argument takes it.
    EXPECT_EQ("{}", logforge::format_message("{}"));
    EXPECT_EQ("7", logforge::format_message("{}", 7));
}

// A closing brace on its own is text. A pair around a number is not: an empty
// specifier and a width-only specifier are both valid, so "{7}" is "the
// argument, padded to seven", not a literal.
TEST(FormatBoundaryTest, AStrayClosingBraceIsText) {
    EXPECT_EQ("a } b", logforge::format_message("a } b"));
    EXPECT_EQ("a } 3", logforge::format_message("a } {}", 3));
    EXPECT_EQ("} 0", logforge::format_message("} {}", 0));
    EXPECT_EQ("      1", logforge::format_message("{7}", 1));
}

// Arguments are consumed in order, and a brace the renderer could not parse does
// not consume one: "{1}" is not a placeholder in this syntax, so the argument
// that would have gone to it goes to the next "{}".
TEST(FormatBoundaryTest, ArgumentsAreConsumedInOrderAcrossAdjacentBraces) {
    EXPECT_EQ("123", logforge::format_message("{}{}{}", 1, 2, 3));
    EXPECT_EQ("a1b2c", logforge::format_message("a{}b{}c", 1, 2));
    EXPECT_EQ("0{1}{}{2}", logforge::format_message("{}{1}{}{2}", 0));
}

// A specifier is only a specifier when the whole text between the braces parses;
// a prefix that parses and a suffix that does not is still malformed.
TEST(FormatBoundaryTest, APartiallyValidSpecifierIsMalformed) {
    EXPECT_EQ("{:4z}", logforge::format_message("{:4z}", 1));
    EXPECT_EQ("{:x4}", logforge::format_message("{:x4}", 1));
    EXPECT_EQ("{: 4}", logforge::format_message("{: 4}", 1));
    // A point with no digits after it is rejected: the precision branch has to
    // see at least one digit before it accepts the specifier.
    EXPECT_EQ("{:.f}", logforge::format_message("{:.f}", 1.5));
    // Width and precision with no type letter is a valid specifier: the fields
    // parse, and a float simply keeps its shortest form.
    EXPECT_EQ(" 1.5", logforge::format_message("{:4.2}", 1.5));
    // A colon with only a width and no type is valid, and pads.
    EXPECT_EQ("   1", logforge::format_message("{:4}", 1));
}

} // namespace
