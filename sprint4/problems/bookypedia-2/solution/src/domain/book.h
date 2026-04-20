#pragma once
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "../util/tagged_uuid.h"

namespace domain {

namespace detail {
struct BookTag {};
}  // namespace detail

using BookId = util::TaggedUUID<detail::BookTag>;

class Book {
public:
    Book(BookId id, AuthorId author_id, std::string title, int publication_year)
        : id_(std::move(id))
        , author_id_(std::move(author_id))
        , title_(std::move(title))
        , publication_year_(publication_year) {
    }

    const BookId& GetId() const noexcept {
        return id_;
    }

    const AuthorId& GetAuthorId() const noexcept {
        return author_id_;
    }

    const std::string& GetTitle() const noexcept {
        return title_;
    }

    int GetPublicationYear() const noexcept {
        return publication_year_;
    }

    void SetTitle(std::string title) { title_ = std::move(title); }
    void SetPublicationYear(int year) { publication_year_ = year; }
    void SetAuthorId(AuthorId author_id) { author_id_ = std::move(author_id); }

private:
    BookId id_;
    AuthorId author_id_;
    std::string title_;
    int publication_year_;
};

class BookRepository {
public:
    virtual void Save(const Book& book) = 0;
    virtual std::vector<Book> FindAll() const = 0;
    virtual std::vector<Book> FindByAuthor(const AuthorId& author_id) const = 0;
    virtual std::vector<Book> FindByTitle(const std::string& title) const = 0;
    virtual std::optional<Book> FindById(const BookId& id) const = 0;
    virtual bool Delete(const BookId& id) = 0;
    virtual void Update(const Book& book) = 0;

    // Tag operations
    virtual std::set<std::string> GetTags(const BookId& book_id) const = 0;
    virtual void SetTags(const BookId& book_id, const std::set<std::string>& tags) = 0;

protected:
    ~BookRepository() = default;
};

}  // namespace domain
