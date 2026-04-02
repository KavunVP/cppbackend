#include "urlencode.h"
#include <sstream>
#include <iomanip>

std::string UrlEncode(std::string_view str) {
    std::ostringstream encoded;
    encoded << std::hex << std::uppercase;

    for (unsigned char c : str) {
        // Пробел кодируется как +
        if (c == ' ') {
            encoded << '+';
        }
        // Буквы, цифры и -._~ не кодируются
        else if ((c >= 'a' && c <= 'z') ||
                 (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') ||
                 c == '-' || c == '.' || c == '_' || c == '~') {
            encoded << c;
        }
        // Зарезервированные символы и символы с кодами < 32 или >= 128
        else {
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }

    return encoded.str();
}
