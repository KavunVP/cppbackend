#include <catch2/catch_test_macros.hpp>

#include "../src/htmldecode.h"

using namespace std::literals;

TEST_CASE("Empty string", "[HtmlDecode]") {
    CHECK(HtmlDecode(""sv) == ""s);
}

TEST_CASE("Text without mnemonics", "[HtmlDecode]") {
    CHECK(HtmlDecode("hello"sv) == "hello"s);
    CHECK(HtmlDecode("Hello World!"sv) == "Hello World!"s);
    CHECK(HtmlDecode("12345"sv) == "12345"s);
    CHECK(HtmlDecode("Johnson&Johnson"sv) == "Johnson&Johnson"s);
}

TEST_CASE("Lowercase mnemonics without semicolon", "[HtmlDecode]") {
    CHECK(HtmlDecode("&lt"sv) == "<"s);
    CHECK(HtmlDecode("&gt"sv) == ">"s);
    CHECK(HtmlDecode("&amp"sv) == "&"s);
    CHECK(HtmlDecode("&apos"sv) == "'"s);
    CHECK(HtmlDecode("&quot"sv) == "\""s);
}

TEST_CASE("Lowercase mnemonics with semicolon", "[HtmlDecode]") {
    CHECK(HtmlDecode("&lt;"sv) == "<"s);
    CHECK(HtmlDecode("&gt;"sv) == ">"s);
    CHECK(HtmlDecode("&amp;"sv) == "&"s);
    CHECK(HtmlDecode("&apos;"sv) == "'"s);
    CHECK(HtmlDecode("&quot;"sv) == "\""s);
}

TEST_CASE("Uppercase mnemonics without semicolon", "[HtmlDecode]") {
    CHECK(HtmlDecode("&LT"sv) == "<"s);
    CHECK(HtmlDecode("&GT"sv) == ">"s);
    CHECK(HtmlDecode("&AMP"sv) == "&"s);
    CHECK(HtmlDecode("&APOS"sv) == "'"s);
    CHECK(HtmlDecode("&QUOT"sv) == "\""s);
}

TEST_CASE("Uppercase mnemonics with semicolon", "[HtmlDecode]") {
    CHECK(HtmlDecode("&LT;"sv) == "<"s);
    CHECK(HtmlDecode("&GT;"sv) == ">"s);
    CHECK(HtmlDecode("&AMP;"sv) == "&"s);
    CHECK(HtmlDecode("&APOS;"sv) == "'"s);
    CHECK(HtmlDecode("&QUOT;"sv) == "\""s);
}

TEST_CASE("Mixed case mnemonics are not decoded", "[HtmlDecode]") {
    CHECK(HtmlDecode("&lT"sv) == "&lT"s);
    CHECK(HtmlDecode("&Lt"sv) == "&Lt"s);
    CHECK(HtmlDecode("&aPos"sv) == "&aPos"s);
    CHECK(HtmlDecode("&aPOS"sv) == "&aPOS"s);
    CHECK(HtmlDecode("&Apos"sv) == "&Apos"s);
    CHECK(HtmlDecode("&APOS"sv) == "'"s);  // Все заглавные - это мнемоника
}

TEST_CASE("Mnemonics in the beginning, middle and end", "[HtmlDecode]") {
    CHECK(HtmlDecode("&lt;hello"sv) == "<hello"s);
    CHECK(HtmlDecode("hello&lt;world"sv) == "hello<world"s);
    CHECK(HtmlDecode("hello&gt;"sv) == "hello>"s);
    CHECK(HtmlDecode("&amp;lt;"sv) == "&lt;"s);
}

TEST_CASE("Multiple mnemonics in sequence", "[HtmlDecode]") {
    CHECK(HtmlDecode("M&amp;M&APOS;s"sv) == "M&M's"s);
    CHECK(HtmlDecode("&lt;&gt;&amp;"sv) == "<>&"s);
    CHECK(HtmlDecode("&quot;&apos;&quot;"sv) == "\"'\""s);
}

TEST_CASE("Invalid mnemonics", "[HtmlDecode]") {
    CHECK(HtmlDecode("&abracadabra"sv) == "&abracadabra"s);
    CHECK(HtmlDecode("&unknown;"sv) == "&unknown;"s);
    CHECK(HtmlDecode("&;"sv) == "&;"s);
    CHECK(HtmlDecode("&"sv) == "&"s);
}

TEST_CASE("Incomplete mnemonics", "[HtmlDecode]") {
    CHECK(HtmlDecode("&l"sv) == "&l"s);
    CHECK(HtmlDecode("&a"sv) == "&a"s);
    CHECK(HtmlDecode("&am"sv) == "&am"s);
}

TEST_CASE("Complex real-world examples", "[HtmlDecode]") {
    CHECK(HtmlDecode("Johnson&amp;Johnson"sv) == "Johnson&Johnson"s);
    CHECK(HtmlDecode("Johnson&amp;Johnson"sv) == "Johnson&Johnson"s);
    CHECK(HtmlDecode("Johnson&AMP;Johnson"sv) == "Johnson&Johnson"s);
    CHECK(HtmlDecode("Johnson&AMPJohnson"sv) == "Johnson&Johnson"s);
    CHECK(HtmlDecode("M&amp;M&APOS;s"sv) == "M&M's"s);
    CHECK(HtmlDecode("5 &lt; 10 &amp;&amp; 10 &gt; 5"sv) == "5 < 10 && 10 > 5"s);
}

TEST_CASE("Text with no mnemonic after ampersand", "[HtmlDecode]") {
    CHECK(HtmlDecode("R&D"sv) == "R&D"s);
    CHECK(HtmlDecode("AT&T"sv) == "AT&T"s);
}

TEST_CASE("Ampersand followed by space", "[HtmlDecode]") {
    CHECK(HtmlDecode("hello & world"sv) == "hello & world"s);
}
