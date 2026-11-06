// The text of one message: a format text and its arguments.
//
// The syntax is the one the legacy macros used through spdlog, so the cases
// below are the shapes that appear in the legacy call sites, plus the rules this
// library adds: a specifier that does not apply to its argument is ignored, a
// malformed one stays as it is written, and a message with fewer arguments than
// braces keeps the braces that have none.

#include <logforge/format.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

namespace {

TEST(FormatTest, TextWithoutABraceIsItsOwnMessage) {
    EXPECT_EQ("pad stopped", logforge::format_message("pad stopped"));
}

TEST(FormatTest, AnArgumentTakesThePlaceOfABrace) {
    EXPECT_EQ("pad in stopped", logforge::format_message("pad {} stopped", "in"));
    EXPECT_EQ("in, out", logforge::format_message("{}, {}", "in", "out"));

    const std::string name = "dynamic";
    EXPECT_EQ("dynamic: 7", logforge::format_message("{}: {}", name, 7));

    // The same text whether it arrives as a std::string, a literal or a buffer.
    char buffer[] = "buffer";
    EXPECT_EQ("buffer buffer buffer",
              logforge::format_message("{} {} {}", "buffer", buffer, std::string("buffer")));
}

TEST(FormatTest, AnIntegerPrintsAsAnInteger) {
    EXPECT_EQ("0", logforge::format_message("{}", 0));
    EXPECT_EQ("-42", logforge::format_message("{}", -42));
    EXPECT_EQ("4294967295", logforge::format_message("{}", 4294967295U));

    const long long big = 9007199254740993LL;
    EXPECT_EQ("9007199254740993", logforge::format_message("{}", big));

    // An unsigned char is a number; a char is a character.
    EXPECT_EQ("66", logforge::format_message("{}", static_cast<unsigned char>(66)));
    EXPECT_EQ("B", logforge::format_message("{}", 'B'));
    EXPECT_EQ("true false", logforge::format_message("{} {}", true, false));
}

TEST(FormatTest, AWidthPadsANumberOnTheLeftAndTextOnTheRight) {
    // The shape the legacy code used to line up a table of pads.
    EXPECT_EQ("id:  3, interval:   60", logforge::format_message("id: {:2}, interval: {:4}", 3, 60));
    EXPECT_EQ("|in  |", logforge::format_message("|{:4}|", "in"));

    // A wider argument is never cut.
    EXPECT_EQ("|scenario|", logforge::format_message("|{:4}|", "scenario"));
    EXPECT_EQ("|  -7|", logforge::format_message("|{:4}|", -7));
}

TEST(FormatTest, ANumberPrintsTheWayTheJsonWriterPrintsIt) {
    EXPECT_EQ("12", logforge::format_message("{}", 12.0));
    EXPECT_EQ("0.1", logforge::format_message("{}", 0.1));
    EXPECT_EQ("-2.5", logforge::format_message("{}", -2.5));

    // The shortest of 15 and 17 digits that reads back as the same value.
    EXPECT_EQ("0.33333333333333331", logforge::format_message("{}", 1.0 / 3.0));
    EXPECT_EQ("2.5 1", logforge::format_message("{} {}", 2.5f, 1.0f));

    // A fixed precision, which is what a legacy %.2f was trying to say.
    EXPECT_EQ("2.50", logforge::format_message("{:.2f}", 2.5));
    EXPECT_EQ("|   2.50|", logforge::format_message("|{:7.2f}|", 2.5));

    // The most digits a double can need: a fixed-precision buffer sized for the
    // precision alone would cut this in half, so assert the whole expansion is
    // there rather than just its length.
    const std::string largest = logforge::format_message("{:.2f}", 1e308);
    EXPECT_EQ(312u, largest.size());
    EXPECT_EQ(".00", largest.substr(largest.size() - 3));
}

TEST(FormatTest, AnIntegerPrintsInHexadecimalWhenAsked) {
    EXPECT_EQ("ff", logforge::format_message("{:x}", 255));
    EXPECT_EQ("FF", logforge::format_message("{:X}", 255));
    EXPECT_EQ("0x00ff", logforge::format_message("0x{:04x}", 255));

    // A negative value prints as the bits it has, which is what fmt does too.
    EXPECT_EQ("ffffffffffffffff", logforge::format_message("{:x}", -1));
}

TEST(FormatTest, ASpecifierThatDoesNotApplyIsIgnored) {
    // Hex on text, a precision on an integer: the argument prints, the specifier
    // does not throw and does not eat the message.
    EXPECT_EQ("in", logforge::format_message("{:x}", "in"));
    EXPECT_EQ("7", logforge::format_message("{:.2f}", 7));
}

TEST(FormatTest, ABraceThatIsNotASpecifierStaysAsItIsWritten) {
    EXPECT_EQ("{a}", logforge::format_message("{a}", 1));
    EXPECT_EQ("{:}", logforge::format_message("{:}", 1));
    EXPECT_EQ("{:.}", logforge::format_message("{:.}", 1.5));
    EXPECT_EQ("{:q}", logforge::format_message("{:q}", 1));

    // A width or a precision beyond what a message may allocate.
    EXPECT_EQ("{:513}", logforge::format_message("{:513}", 1));
    EXPECT_EQ("{:.65f}", logforge::format_message("{:.65f}", 1.5));

    // A brace without its partner.
    EXPECT_EQ("a { b", logforge::format_message("a { b"));
}

TEST(FormatTest, ADoubleBraceIsOneBrace) {
    EXPECT_EQ("{count}", logforge::format_message("{{count}}"));
    EXPECT_EQ("{}", logforge::format_message("{{}}"));
    EXPECT_EQ("{7}", logforge::format_message("{{{}}}", 7));
}

TEST(FormatTest, AMessageWithMoreBracesThanArgumentsKeepsTheBraces) {
    EXPECT_EQ("in and {}", logforge::format_message("{} and {}", "in"));
    EXPECT_EQ("3 of {}", logforge::format_message("{} of {}", 3));
}

TEST(FormatTest, AnArgumentWithoutABraceIsIgnored) {
    EXPECT_EQ("in", logforge::format_message("{}", "in", 7, 8));
}

TEST(FormatTest, ANullTextIsAnAddressAndANullTextPointerIsEmpty) {
    EXPECT_EQ("(null)", logforge::format_message("{}", static_cast<const char *>(0)));
    EXPECT_EQ("", logforge::format_message(static_cast<const char *>(0)));

    // A pointer that is not text is an address, spelled the same on every
    // platform.
    const int value = 7;
    const std::string address = logforge::format_message("{}", &value);
    EXPECT_GT(address.size(), 2u);
    EXPECT_EQ('0', address[0]);
    EXPECT_EQ('x', address[1]);
}

} // namespace
