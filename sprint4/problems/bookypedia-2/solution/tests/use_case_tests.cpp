#include <catch2/catch_test_macros.hpp>
#include <map>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;

    void Save(const domain::Author& author) override {
        saved_authors.emplace_back(author);
    }

    std::vector<domain::Author> FindAll() const override {
        return saved_authors;
    }

    std::optional<domain::Author> FindByName(const std::string& name) const override {
        for (const auto& a : saved_authors) {
            if (a.GetName() == name) {
                return a;
            }
        }
        return std::nullopt;
    }

    std::optional<domain::Author> FindById(const domain::AuthorId& id) const override {
        for (const auto& a : saved_authors) {
            if (a.GetId() == id) {
                return a;
            }
        }
        return std::nullopt;
    }

    bool Delete(const domain::AuthorId& id) override {
        auto it = std::remove_if(saved_authors.begin(), saved_authors.end(),
                                 [&id](const domain::Author& a) { return a.GetId() == id; });
        bool removed = it != saved_authors.end();
        saved_authors.erase(it, saved_authors.end());
        return removed;
    }

    void Update(const domain::Author& author) override {
        for (auto& a : saved_authors) {
            if (a.GetId() == author.GetId()) {
                a = author;
                return;
            }
        }
        throw std::runtime_error("Author not found");
    }
};

struct MockBookRepository : domain::BookRepository {
    std::vector<domain::Book> saved_books;
    std::map<domain::BookId, std::set<std::string>> saved_tags;

    void Save(const domain::Book& book) override {
        saved_books.emplace_back(book);
    }

    std::vector<domain::Book> FindAll() const override {
        return saved_books;
    }

    std::vector<domain::Book> FindByAuthor(const domain::AuthorId& author_id) const override {
        std::vector<domain::Book> result;
        for (const auto& b : saved_books) {
            if (b.GetAuthorId() == author_id) {
                result.push_back(b);
            }
        }
        return result;
    }

    std::vector<domain::Book> FindByTitle(const std::string& title) const override {
        std::vector<domain::Book> result;
        for (const auto& b : saved_books) {
            if (b.GetTitle() == title) {
                result.push_back(b);
            }
        }
        return result;
    }

    std::optional<domain::Book> FindById(const domain::BookId& id) const override {
        for (const auto& b : saved_books) {
            if (b.GetId() == id) {
                return b;
            }
        }
        return std::nullopt;
    }

    bool Delete(const domain::BookId& id) override {
        auto book_it = std::remove_if(saved_books.begin(), saved_books.end(),
                                      [&id](const domain::Book& b) { return b.GetId() == id; });
        bool removed = book_it != saved_books.end();
        saved_books.erase(book_it, saved_books.end());
        saved_tags.erase(id);
        return removed;
    }

    void Update(const domain::Book& book) override {
        for (auto& b : saved_books) {
            if (b.GetId() == book.GetId()) {
                b = book;
                return;
            }
        }
        throw std::runtime_error("Book not found");
    }

    std::set<std::string> GetTags(const domain::BookId& book_id) const override {
        auto it = saved_tags.find(book_id);
        if (it != saved_tags.end()) {
            return it->second;
        }
        return {};
    }

    void SetTags(const domain::BookId& book_id, const std::set<std::string>& tags) override {
        saved_tags[book_id] = tags;
    }
};

struct Fixture {
    MockAuthorRepository authors;
    MockBookRepository books;
};

}  // namespace

SCENARIO_METHOD(Fixture, "Book Adding") {
    GIVEN("Use cases") {
        app::UseCasesImpl use_cases{authors, books};

        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            use_cases.AddAuthor(author_name);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(authors.saved_authors.size() == 1);
                CHECK(authors.saved_authors.at(0).GetName() == author_name);
                CHECK(authors.saved_authors.at(0).GetId() != domain::AuthorId{});
            }
        }
    }
}