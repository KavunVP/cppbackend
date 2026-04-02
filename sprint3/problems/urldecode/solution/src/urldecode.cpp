#include "urldecode.h"

#include <charconv>
#include <stdexcept>

std::string UrlDecode(std::string_view str) {
    std::string res;
    res.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '+') {
            res += ' ';
        } else if (str[i] == '%' && i + 2 < str.size()) {
            int hex_val = 0;
            for (int j = 1; j <= 2; ++j) {
                char c = str[i + j];
                int digit;
                if (c >= '0' && c <= '9') digit = c - '0';
                else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
                else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
                else throw std::invalid_argument("Invalid hex digit");
                hex_val = hex_val * 16 + digit;
            }
            res += static_cast<char>(hex_val);
            i += 2;
        } else if (str[i] == '%') {
            throw std::invalid_argument("Incomplete percent sequence");
        } else {
            res += str[i];
        }
    }
    return res;
}
