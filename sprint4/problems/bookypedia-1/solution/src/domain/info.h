#pragma once
#include <string>
#include <vector>

namespace domain {

struct AuthorInfo {
    std::string id;
    std::string name;
};

struct BookInfo {
    std::string title;
    int publication_year;
};

}  // namespace domain
