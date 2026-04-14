#include "use_cases_impl.h"

#include <algorithm>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save({AuthorId::New(), name});
}

void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title,
                           int publication_year) {
    books_.Save({BookId::New(), AuthorId::FromString(author_id), title, publication_year});
}

std::vector<AuthorInfo> UseCasesImpl::GetAuthors() const {
    auto authors = authors_.FindAll();
    std::ranges::sort(authors, [](const Author& a, const Author& b) {
        return a.GetName() < b.GetName();
    });

    std::vector<AuthorInfo> result;
    result.reserve(authors.size());
    for (const auto& author : authors) {
        result.push_back({author.GetId().ToString(), author.GetName()});
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::GetBooks() const {
    auto books = books_.FindAll();
    std::ranges::sort(books, [](const Book& a, const Book& b) {
        return a.GetTitle() < b.GetTitle();
    });

    std::vector<BookInfo> result;
    result.reserve(books.size());
    for (const auto& book : books) {
        result.push_back({book.GetTitle(), book.GetPublicationYear()});
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) const {
    auto books = books_.FindByAuthor(AuthorId::FromString(author_id));
    std::ranges::sort(books, [](const Book& a, const Book& b) {
        if (a.GetPublicationYear() != b.GetPublicationYear()) {
            return a.GetPublicationYear() < b.GetPublicationYear();
        }
        return a.GetTitle() < b.GetTitle();
    });

    std::vector<BookInfo> result;
    result.reserve(books.size());
    for (const auto& book : books) {
        result.push_back({book.GetTitle(), book.GetPublicationYear()});
    }
    return result;
}

}  // namespace app
