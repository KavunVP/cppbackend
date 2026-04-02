#include "htmldecode.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace {

// Проверяет, состоит ли строка только из заглавных или только из строчных букв
bool IsUniformCase(std::string_view s) {
    if (s.empty()) {
        return false;
    }
    bool all_upper = true;
    bool all_lower = true;
    for (char c : s) {
        if (c >= 'a' && c <= 'z') {
            all_upper = false;
        } else if (c >= 'A' && c <= 'Z') {
            all_lower = false;
        } else {
            return false;
        }
    }
    return all_upper || all_lower;
}

// Пытается найти и декодировать мнемонику, начинающуюся с позиции start
// Возвращает длину потребленной части (включая & и опционально ;) и decoded символ
// Если мнемоника не найдена, возвращает 0
std::pair<size_t, char> TryDecodeAt(std::string_view str, size_t start) {
    static const std::string_view kMnemonics[] = {
        "lt", "gt", "amp", "apos", "quot"
    };
    static const char kDecoded[] = {'<', '>', '&', '\'', '"'};
    
    for (size_t m = 0; m < 5; ++m) {
        std::string_view mnemonic = kMnemonics[m];
        
        // Проверяем вариант с ;
        if (start + 1 + mnemonic.size() < str.size() && str[start + 1 + mnemonic.size()] == ';') {
            std::string_view candidate = str.substr(start + 1, mnemonic.size());
            if (candidate == mnemonic || (IsUniformCase(candidate) && 
                std::equal(candidate.begin(), candidate.end(), mnemonic.begin(),
                    [](char a, char b) { return std::tolower(a) == std::tolower(b); }))) {
                if (IsUniformCase(candidate)) {
                    return {mnemonic.size() + 2, kDecoded[m]};  // +2 для & и ;
                }
            }
        }
        
        // Проверяем вариант без ;
        if (start + 1 + mnemonic.size() <= str.size()) {
            std::string_view candidate = str.substr(start + 1, mnemonic.size());
            if (IsUniformCase(candidate) &&
                std::equal(candidate.begin(), candidate.end(), mnemonic.begin(),
                    [](char a, char b) { return std::tolower(a) == std::tolower(b); })) {
                // Убеждаемся, что после мнемоники не идет буква (чтобы &AMPJohnson не ломалось)
                // На самом деле по заданию &AMPJohnson должно декодироваться в &Johnson
                // То есть мнемоника должна быть ровно известной длины
                return {mnemonic.size() + 1, kDecoded[m]};  // +1 только для &
            }
        }
    }
    
    return {0, 0};
}

}  // namespace

std::string HtmlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    
    size_t i = 0;
    while (i < str.size()) {
        if (str[i] == '&') {
            auto [len, decoded] = TryDecodeAt(str, i);
            if (len > 0) {
                result += decoded;
                i += len;
                continue;
            }
        }
        result += str[i];
        i++;
    }
    
    return result;
}
