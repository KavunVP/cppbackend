#define BOOST_TEST_MODULE urlencode tests
#include <boost/test/unit_test.hpp>

#include "../src/urldecode.h"

BOOST_AUTO_TEST_CASE(empty_string) {
    using namespace std::literals;
    BOOST_TEST(UrlDecode(""sv) == ""s);
}

BOOST_AUTO_TEST_CASE(no_percent_sequences) {
    using namespace std::literals;
    BOOST_TEST(UrlDecode("Hello World"sv) == "Hello World"s);
    BOOST_TEST(UrlDecode("123abc"sv) == "123abc"s);
}

BOOST_AUTO_TEST_CASE(valid_percent_sequences_mixed_case) {
    using namespace std::literals;
    // Строчные буквы
    BOOST_TEST(UrlDecode("%20"sv) == " "s);
    BOOST_TEST(UrlDecode("%21"sv) == "!"s);
    // Заглавные буквы
    BOOST_TEST(UrlDecode("%48%65%6C%6C%6F"sv) == "Hello"s);
    // Смешанный регистр
    BOOST_TEST(UrlDecode("%48%65%6c%6C%6f"sv) == "Hello"s);
    BOOST_TEST(UrlDecode("Hello%20World%21"sv) == "Hello World!"s);
}

BOOST_AUTO_TEST_CASE(plus_symbol) {
    using namespace std::literals;
    BOOST_TEST(UrlDecode("Hello+World"sv) == "Hello World"s);
    BOOST_TEST(UrlDecode("a+b+c"sv) == "a b c"s);
    BOOST_TEST(UrlDecode("+"sv) == " "s);
}

BOOST_AUTO_TEST_CASE(incomplete_percent_sequences) {
    using namespace std::literals;
    // % в конце строки
    BOOST_CHECK_THROW(UrlDecode("Hello%"sv), std::invalid_argument);
    // % и один символ после
    BOOST_CHECK_THROW(UrlDecode("Hello%2"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("Hello%G"sv), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(invalid_percent_sequences) {
    using namespace std::literals;
    // Невалидные hex-символы
    BOOST_CHECK_THROW(UrlDecode("Hello%GG"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%XY"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("test%2G0"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%2!"sv), std::invalid_argument);
}