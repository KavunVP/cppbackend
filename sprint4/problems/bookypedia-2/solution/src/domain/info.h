#pragma once
#include <set>
#include <string>
#include <vector>

namespace domain {

struct AuthorInfo {
    std::string id;
    std::string name;
};

struct BookInfo {
    std::string id;
    std::string title;
    std::string author_name;
    int publication_year;
    std::set<std::string> tags;
};

struct BookDetails {
    std::string id;
    std::string title;
    std::string author_name;
    int publication_year;
    std::set<std::string> tags;
};

}  // namespace domain
