#pragma once
#include <optional>
#include <set>
#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/info.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books)
        : authors_{authors}
        , books_{books} {
    }

    void AddAuthor(const std::string& name) override;
    std::optional<domain::AuthorInfo> AddBook(const std::string& author_name,
                                               const std::string& title,
                                               int publication_year,
                                               const std::set<std::string>& tags) override;
    std::vector<domain::AuthorInfo> GetAuthors() const override;
    std::vector<domain::BookInfo> GetBooks() const override;
    std::vector<domain::BookInfo> GetAuthorBooks(const std::string& author_id) const override;
    std::vector<domain::BookInfo> GetBooksByTitle(const std::string& title) const override;
    std::optional<domain::BookDetails> GetBookDetails(const std::string& book_id) const override;

    bool DeleteAuthor(const std::string& author_id) override;
    bool EditAuthor(const std::string& author_id, const std::string& new_name) override;
    bool DeleteBook(const std::string& book_id) override;
    bool EditBook(const std::string& book_id, const std::optional<std::string>& new_title,
                  const std::optional<int>& new_year,
                  const std::optional<std::set<std::string>>& new_tags) override;
    std::optional<domain::AuthorInfo> FindAuthorByName(const std::string& name) const override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
};

}  // namespace app
