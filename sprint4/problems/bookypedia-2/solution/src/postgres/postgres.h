#pragma once
#include <optional>
#include <pqxx/connection>
#include <pqxx/transaction>
#include <set>
#include <string>

#include "../domain/author.h"
#include "../domain/book.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Author& author) override;
    std::vector<domain::Author> FindAll() const override;
    std::optional<domain::Author> FindByName(const std::string& name) const override;
    std::optional<domain::Author> FindById(const domain::AuthorId& id) const override;
    bool Delete(const domain::AuthorId& id) override;
    void Update(const domain::Author& author) override;

private:
    pqxx::connection& connection_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Book& book) override;
    std::vector<domain::Book> FindAll() const override;
    std::vector<domain::Book> FindByAuthor(const domain::AuthorId& author_id) const override;
    std::vector<domain::Book> FindByTitle(const std::string& title) const override;
    std::optional<domain::Book> FindById(const domain::BookId& id) const override;
    bool Delete(const domain::BookId& id) override;
    void Update(const domain::Book& book) override;

    // Tag operations
    std::set<std::string> GetTags(const domain::BookId& book_id) const override;
    void SetTags(const domain::BookId& book_id, const std::set<std::string>& tags) override;

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    AuthorRepositoryImpl& GetAuthors() & {
        return authors_;
    }

    BookRepositoryImpl& GetBooks() & {
        return books_;
    }

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
};

}  // namespace postgres