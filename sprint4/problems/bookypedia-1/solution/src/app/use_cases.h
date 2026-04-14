#pragma once

#include <string>
#include <vector>

#include "../domain/info.h"

namespace app {

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual void AddBook(const std::string& author_id, const std::string& title,
                         int publication_year) = 0;
    virtual std::vector<domain::AuthorInfo> GetAuthors() const = 0;
    virtual std::vector<domain::BookInfo> GetBooks() const = 0;
    virtual std::vector<domain::BookInfo> GetAuthorBooks(const std::string& author_id) const = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
