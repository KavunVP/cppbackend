#include "postgres.h"

#include <pqxx/pqxx>
#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::FindAll() const {
    pqxx::work work{connection_};
    pqxx::result result = work.exec(
        R"(
SELECT id, name FROM authors ORDER BY name;
)"_zv);

    std::vector<domain::Author> authors;
    for (const auto& row : result) {
        authors.emplace_back(domain::AuthorId::FromString(row[0].as<std::string>()),
                             row[1].as<std::string>());
    }
    return authors;
}

std::optional<domain::Author> AuthorRepositoryImpl::FindByName(const std::string& name) const {
    pqxx::work work{connection_};
    pqxx::result result = work.exec_params(
        R"(
SELECT id, name FROM authors WHERE name = $1;
)"_zv,
        name);

    if (result.empty()) {
        return std::nullopt;
    }
    return domain::Author{domain::AuthorId::FromString(result[0][0].as<std::string>()),
                          result[0][1].as<std::string>()};
}

std::optional<domain::Author> AuthorRepositoryImpl::FindById(const domain::AuthorId& id) const {
    pqxx::work work{connection_};
    pqxx::result result = work.exec_params(
        R"(
SELECT id, name FROM authors WHERE id = $1;
)"_zv,
        id.ToString());

    if (result.empty()) {
        return std::nullopt;
    }
    return domain::Author{domain::AuthorId::FromString(result[0][0].as<std::string>()),
                          result[0][1].as<std::string>()};
}

bool AuthorRepositoryImpl::Delete(const domain::AuthorId& id) {
    pqxx::work work{connection_};
    // Delete book tags for all books by this author
    work.exec_params(
        R"(
DELETE FROM book_tags WHERE book_id IN (SELECT id FROM books WHERE author_id = $1);
)"_zv,
        id.ToString());
    // Delete books by this author
    work.exec_params(
        R"(
DELETE FROM books WHERE author_id = $1;
)"_zv,
        id.ToString());
    // Delete the author
    auto result = work.exec_params(
        R"(
DELETE FROM authors WHERE id = $1;
)"_zv,
        id.ToString());
    work.commit();
    return result.affected_rows() > 0;
}

void AuthorRepositoryImpl::Update(const domain::Author& author) {
    pqxx::work work{connection_};
    auto result = work.exec_params(
        R"(
UPDATE authors SET name = $2 WHERE id = $1;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
    if (result.affected_rows() == 0) {
        throw std::runtime_error("Author not found");
    }
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4);
)"_zv,
        book.GetId().ToString(), book.GetAuthorId().ToString(), book.GetTitle(),
        book.GetPublicationYear());
    work.commit();
}

std::vector<domain::Book> BookRepositoryImpl::FindAll() const {
    pqxx::work work{connection_};
    pqxx::result result = work.exec(
        R"(
SELECT id, author_id, title, publication_year FROM books;
)"_zv);

    std::vector<domain::Book> books;
    for (const auto& row : result) {
        books.emplace_back(
            domain::BookId::FromString(row[0].as<std::string>()),
            domain::AuthorId::FromString(row[1].as<std::string>()), row[2].as<std::string>(),
            row[3].as<int>());
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::FindByAuthor(const domain::AuthorId& author_id) const {
    pqxx::work work{connection_};
    pqxx::result result = work.exec_params(
        R"(
SELECT id, author_id, title, publication_year FROM books WHERE author_id = $1;
)"_zv,
        author_id.ToString());

    std::vector<domain::Book> books;
    for (const auto& row : result) {
        books.emplace_back(
            domain::BookId::FromString(row[0].as<std::string>()),
            domain::AuthorId::FromString(row[1].as<std::string>()), row[2].as<std::string>(),
            row[3].as<int>());
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::FindByTitle(const std::string& title) const {
    pqxx::work work{connection_};
    pqxx::result result = work.exec_params(
        R"(
SELECT id, author_id, title, publication_year FROM books WHERE title = $1 ORDER BY title, author_id, publication_year;
)"_zv,
        title);

    std::vector<domain::Book> books;
    for (const auto& row : result) {
        books.emplace_back(
            domain::BookId::FromString(row[0].as<std::string>()),
            domain::AuthorId::FromString(row[1].as<std::string>()), row[2].as<std::string>(),
            row[3].as<int>());
    }
    return books;
}

std::optional<domain::Book> BookRepositoryImpl::FindById(const domain::BookId& id) const {
    pqxx::work work{connection_};
    pqxx::result result = work.exec_params(
        R"(
SELECT id, author_id, title, publication_year FROM books WHERE id = $1;
)"_zv,
        id.ToString());

    if (result.empty()) {
        return std::nullopt;
    }
    return domain::Book{
        domain::BookId::FromString(result[0][0].as<std::string>()),
        domain::AuthorId::FromString(result[0][1].as<std::string>()),
        result[0][2].as<std::string>(),
        result[0][3].as<int>()};
}

bool BookRepositoryImpl::Delete(const domain::BookId& id) {
    pqxx::work work{connection_};
    // Delete tags first
    work.exec_params(
        R"(
DELETE FROM book_tags WHERE book_id = $1;
)"_zv,
        id.ToString());
    // Delete the book
    auto result = work.exec_params(
        R"(
DELETE FROM books WHERE id = $1;
)"_zv,
        id.ToString());
    work.commit();
    return result.affected_rows() > 0;
}

void BookRepositoryImpl::Update(const domain::Book& book) {
    pqxx::work work{connection_};
    auto result = work.exec_params(
        R"(
UPDATE books SET title = $2, author_id = $3, publication_year = $4 WHERE id = $1;
)"_zv,
        book.GetId().ToString(), book.GetTitle(), book.GetAuthorId().ToString(),
        book.GetPublicationYear());
    work.commit();
    if (result.affected_rows() == 0) {
        throw std::runtime_error("Book not found");
    }
}

std::set<std::string> BookRepositoryImpl::GetTags(const domain::BookId& book_id) const {
    pqxx::work work{connection_};
    pqxx::result result = work.exec_params(
        R"(
SELECT tag FROM book_tags WHERE book_id = $1;
)"_zv,
        book_id.ToString());

    std::set<std::string> tags;
    for (const auto& row : result) {
        tags.insert(row[0].as<std::string>());
    }
    return tags;
}

void BookRepositoryImpl::SetTags(const domain::BookId& book_id, const std::set<std::string>& tags) {
    pqxx::work work{connection_};
    // Delete existing tags
    work.exec_params(
        R"(
DELETE FROM book_tags WHERE book_id = $1;
)"_zv,
        book_id.ToString());
    // Insert new tags
    for (const auto& tag : tags) {
        work.exec_params(
            R"(
INSERT INTO book_tags (book_id, tag) VALUES ($1, $2);
)"_zv,
            book_id.ToString(), tag);
    }
    work.commit();
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL,
    title varchar(100) NOT NULL,
    publication_year INTEGER NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL,
    tag varchar(30) NOT NULL,
    CONSTRAINT fk_books
        FOREIGN KEY(book_id)
        REFERENCES books(id)
        ON DELETE CASCADE
);
)"_zv);
    work.commit();
}

}  // namespace postgres
