#include <gtest/gtest.h>

#include "../src/urlencode.h"

using namespace std::literals;

// Пустая строка
TEST(UrlEncodeTestSuite, EmptyString) {
    EXPECT_EQ(UrlEncode(""sv), ""s);
}

// Строка без служебных символов (буквы, цифры, -._~)
TEST(UrlEncodeTestSuite, OrdinaryCharsAreNotEncoded) {
    EXPECT_EQ(UrlEncode("hello"sv), "hello"s);
    EXPECT_EQ(UrlEncode("Hello123"sv), "Hello123"s);
    EXPECT_EQ(UrlEncode("test-case.example_123~"sv), "test-case.example_123~"s);
}

// Строка с пробелами
TEST(UrlEncodeTestSuite, SpacesEncodedAsPlus) {
    EXPECT_EQ(UrlEncode("Hello World"sv), "Hello+World"s);
    EXPECT_EQ(UrlEncode("a b c"sv), "a+b+c"s);
    EXPECT_EQ(UrlEncode(" "sv), "+"s);
    EXPECT_EQ(UrlEncode("  "sv), "++"s);
}

// Строка с зарезервированными символами !#$&'()*+,/:;=?@[]
TEST(UrlEncodeTestSuite, ReservedSymbolsEncoded) {
    EXPECT_EQ(UrlEncode("!"sv), "%21"s);
    EXPECT_EQ(UrlEncode("#"sv), "%23"s);
    EXPECT_EQ(UrlEncode("$"sv), "%24"s);
    EXPECT_EQ(UrlEncode("&"sv), "%26"s);
    EXPECT_EQ(UrlEncode("'"sv), "%27"s);
    EXPECT_EQ(UrlEncode("("sv), "%28"s);
    EXPECT_EQ(UrlEncode(")"sv), "%29"s);
    EXPECT_EQ(UrlEncode("*"sv), "%2A"s);
    EXPECT_EQ(UrlEncode("+"sv), "%2B"s);
    EXPECT_EQ(UrlEncode(","sv), "%2C"s);
    EXPECT_EQ(UrlEncode("/"sv), "%2F"s);
    EXPECT_EQ(UrlEncode(":"sv), "%3A"s);
    EXPECT_EQ(UrlEncode(";"sv), "%3B"s);
    EXPECT_EQ(UrlEncode("="sv), "%3D"s);
    EXPECT_EQ(UrlEncode("?"sv), "%3F"s);
    EXPECT_EQ(UrlEncode("@"sv), "%40"s);
    EXPECT_EQ(UrlEncode("["sv), "%5B"s);
    EXPECT_EQ(UrlEncode("]"sv), "%5D"s);
}

// Строка со смешанными символами
TEST(UrlEncodeTestSuite, MixedString) {
    EXPECT_EQ(UrlEncode("Hello World!"sv), "Hello+World%21"s);
    EXPECT_EQ(UrlEncode("abc*"sv), "abc%2A"s);
    EXPECT_EQ(UrlEncode("test@example.com"sv), "test%40example.com"s);
    EXPECT_EQ(UrlEncode("a+b=c"sv), "a%2Bb%3Dc"s);
}

// Символы с кодами меньше 32
TEST(UrlEncodeTestSuite, ControlCharactersEncoded) {
    EXPECT_EQ(UrlEncode("\x00"sv), "%00"s);
    EXPECT_EQ(UrlEncode("\x01"sv), "%01"s);
    EXPECT_EQ(UrlEncode("\x1F"sv), "%1F"s);
    EXPECT_EQ(UrlEncode("\n"sv), "%0A"s);
    EXPECT_EQ(UrlEncode("\r"sv), "%0D"s);
    EXPECT_EQ(UrlEncode("\t"sv), "%09"s);
}

// Символы с кодами >= 128
TEST(UrlEncodeTestSuite, HighAsciiCharactersEncoded) {
    EXPECT_EQ(UrlEncode("\x80"sv), "%80"s);
    EXPECT_EQ(UrlEncode("\xFF"sv), "%FF"s);
    EXPECT_EQ(UrlEncode("Привет"sv), "%D0%9F%D1%80%D0%B8%D0%B2%D0%B5%D1%82"s);
}

// Комбинированный тест
TEST(UrlEncodeTestSuite, CombinedTest) {
    EXPECT_EQ(UrlEncode("Hello, World! 123"sv), "Hello%2C+World%21+123"s);
}
