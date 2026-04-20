#include "use_cases_impl.h"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save({AuthorId::New(), name});
}

std::optional<AuthorInfo> UseCasesImpl::AddBook(const std::string& author_name,
                                                 const std::string& title,
                                                 int publication_year,
                                                 const std::set<std::string>& tags) {
    // Try to find existing author
    auto author = authors_.FindByName(author_name);
    if (!author) {
        // Create new author
        author = Author{AuthorId::New(), author_name};
        authors_.Save(*author);
    }

    // Create and save the book
    Book book{BookId::New(), author->GetId(), title, publication_year};
    books_.Save(book);

    // Save tags
    books_.SetTags(book.GetId(), tags);

    return AuthorInfo{author->GetId().ToString(), author->GetName()};
}

std::vector<AuthorInfo> UseCasesImpl::GetAuthors() const {
    auto authors = authors_.FindAll();
    // Authors are already sorted by name in the database (ORDER BY name)

    std::vector<AuthorInfo> result;
    result.reserve(authors.size());
    for (const auto& author : authors) {
        result.push_back({author.GetId().ToString(), author.GetName()});
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::GetBooks() const {
    auto books = books_.FindAll();

    // Build a map of author_id -> author_name
    auto all_authors = authors_.FindAll();
    std::map<AuthorId, std::string> author_names;
    for (const auto& author : all_authors) {
        author_names[author.GetId()] = author.GetName();
    }

    // Build book info with author name
    std::vector<BookInfo> result;
    result.reserve(books.size());
    for (const auto& book : books) {
        auto tags = books_.GetTags(book.GetId());
        auto it = author_names.find(book.GetAuthorId());
        std::string author_name = (it != author_names.end()) ? it->second : "";
        result.push_back({book.GetId().ToString(), book.GetTitle(), author_name,
                          book.GetPublicationYear(), std::move(tags)});
    }

    // Sort by title, then author name, then publication year (matching DB ORDER BY)
    std::ranges::sort(result, [](const BookInfo& a, const BookInfo& b) {
        if (a.title != b.title) {
            return a.title < b.title;
        }
        if (a.author_name != b.author_name) {
            return a.author_name < b.author_name;
        }
        return a.publication_year < b.publication_year;
    });

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
        auto tags = books_.GetTags(book.GetId());
        result.push_back({book.GetId().ToString(), book.GetTitle(), "",
                          book.GetPublicationYear(), std::move(tags)});
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::GetBooksByTitle(const std::string& title) const {
    auto books = books_.FindByTitle(title);

    // Build a map of author_id -> author_name
    auto all_authors = authors_.FindAll();
    std::map<AuthorId, std::string> author_names;
    for (const auto& author : all_authors) {
        author_names[author.GetId()] = author.GetName();
    }

    std::vector<BookInfo> result;
    result.reserve(books.size());
    for (const auto& book : books) {
        auto tags = books_.GetTags(book.GetId());
        auto it = author_names.find(book.GetAuthorId());
        std::string author_name = (it != author_names.end()) ? it->second : "";
        result.push_back({book.GetId().ToString(), book.GetTitle(), author_name,
                          book.GetPublicationYear(), std::move(tags)});
    }
    return result;
}

std::optional<BookDetails> UseCasesImpl::GetBookDetails(const std::string& book_id) const {
    auto book = books_.FindById(BookId::FromString(book_id));
    if (!book) {
        return std::nullopt;
    }

    auto author = authors_.FindById(book->GetAuthorId());
    std::string author_name = author ? author->GetName() : "";

    auto tags = books_.GetTags(book->GetId());

    return BookDetails{book->GetId().ToString(), book->GetTitle(), author_name,
                       book->GetPublicationYear(), std::move(tags)};
}

bool UseCasesImpl::DeleteAuthor(const std::string& author_id) {
    return authors_.Delete(AuthorId::FromString(author_id));
}

bool UseCasesImpl::EditAuthor(const std::string& author_id, const std::string& new_name) {
    try {
        auto author = authors_.FindById(AuthorId::FromString(author_id));
        if (!author) {
            return false;
        }
        Author updated{author->GetId(), new_name};
        authors_.Update(updated);
        return true;
    } catch (...) {
        return false;
    }
}

bool UseCasesImpl::DeleteBook(const std::string& book_id) {
    return books_.Delete(BookId::FromString(book_id));
}

bool UseCasesImpl::EditBook(const std::string& book_id, const std::optional<std::string>& new_title,
                            const std::optional<int>& new_year,
                            const std::optional<std::set<std::string>>& new_tags) {
    auto book = books_.FindById(BookId::FromString(book_id));
    if (!book) {
        return false;
    }

    if (new_title) {
        book->SetTitle(*new_title);
    }
    if (new_year) {
        book->SetPublicationYear(*new_year);
    }
    books_.Update(*book);

    if (new_tags) {
        books_.SetTags(book->GetId(), *new_tags);
    }

    return true;
}

std::optional<AuthorInfo> UseCasesImpl::FindAuthorByName(const std::string& name) const {
    auto author = authors_.FindByName(name);
    if (!author) {
        return std::nullopt;
    }
    return AuthorInfo{author->GetId().ToString(), author->GetName()};
}

}  // namespace app
