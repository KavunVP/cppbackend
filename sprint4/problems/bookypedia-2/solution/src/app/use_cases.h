#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "../domain/info.h"

namespace app {

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual std::optional<domain::AuthorInfo> AddBook(const std::string& author_name,
                                                       const std::string& title,
                                                       int publication_year,
                                                       const std::set<std::string>& tags) = 0;
    virtual std::vector<domain::AuthorInfo> GetAuthors() const = 0;
    virtual std::vector<domain::BookInfo> GetBooks() const = 0;
    virtual std::vector<domain::BookInfo> GetAuthorBooks(const std::string& author_id) const = 0;
    virtual std::vector<domain::BookInfo> GetBooksByTitle(const std::string& title) const = 0;
    virtual std::optional<domain::BookDetails> GetBookDetails(const std::string& book_id) const = 0;

    virtual bool DeleteAuthor(const std::string& author_id) = 0;
    virtual bool EditAuthor(const std::string& author_id, const std::string& new_name) = 0;
    virtual bool DeleteBook(const std::string& book_id) = 0;
    virtual bool EditBook(const std::string& book_id, const std::optional<std::string>& new_title,
                          const std::optional<int>& new_year,
                          const std::optional<std::set<std::string>>& new_tags) = 0;
    virtual std::optional<domain::AuthorInfo> FindAuthorByName(const std::string& name) const = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
