#include "view.h"

#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <iostream>
#include <sstream>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

#include "../app/use_cases.h"
#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {
namespace detail {

std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
    return out;
}

std::ostream& operator<<(std::ostream& out, const BookInfo& book) {
    out << book.title << " by " << book.author_name << ", " << book.publication_year;
    return out;
}

}  // namespace detail

void PrintAuthors(std::ostream& out, const std::vector<detail::AuthorInfo>& authors) {
    int i = 1;
    for (const auto& author : authors) {
        out << i++ << " " << author << std::endl;
    }
}

void PrintBooks(std::ostream& out, const std::vector<detail::BookInfo>& books) {
    int i = 1;
    for (const auto& book : books) {
        out << i++ << " " << book << std::endl;
    }
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction(  //
        "AddAuthor"s, "name"s, "Adds author"s, std::bind(&View::AddAuthor, this, ph::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s,
                    std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s,
                    std::bind(&View::ShowAuthorBooks, this));
    menu_.AddAction("DeleteAuthor"s, "[name]"s, "Delete author"s,
                    std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "[name]"s, "Edit author"s,
                    std::bind(&View::EditAuthor, this, ph::_1));
    menu_.AddAction("ShowBook"s, "[title]"s, "Show book details"s,
                    std::bind(&View::ShowBook, this, ph::_1));
    menu_.AddAction("DeleteBook"s, "[title]"s, "Delete book"s,
                    std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "[title]"s, "Edit book"s,
                    std::bind(&View::EditBook, this, ph::_1));
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        if (name.empty()) {
            throw std::runtime_error("Empty author name");
        }
        // Check if author already exists
        auto existing = use_cases_.FindAuthorByName(name);
        if (existing) {
            throw std::runtime_error("Author already exists");
        }
        use_cases_.AddAuthor(std::move(name));
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        int publication_year;
        cmd_input >> publication_year;
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        if (title.empty()) {
            throw std::runtime_error("Empty title");
        }

        // Get author - either from list or enter name
        output_ << "Enter author name or empty line to select from list:" << std::endl;
        std::string author_input;
        if (!std::getline(input_, author_input)) {
            throw std::runtime_error("Failed to read author");
        }
        boost::algorithm::trim(author_input);

        std::string author_id;
        std::string author_name;
        bool author_valid = false;
        bool author_prompted = false;  // Track if we prompted for y/n

        if (author_input.empty()) {
            // Select from list
            auto author_opt = SelectAuthor();
            if (author_opt) {
                author_id = *author_opt;
                // Get author name from the info
                auto authors = use_cases_.GetAuthors();
                for (const auto& a : authors) {
                    if (a.id == author_id) {
                        author_name = a.name;
                        break;
                    }
                }
                author_valid = true;
            }
        } else {
            // Check if author exists
            auto existing_author = use_cases_.FindAuthorByName(author_input);
            if (existing_author) {
                author_id = existing_author->id;
                author_name = existing_author->name;
                author_valid = true;
            } else {
                // Ask to add new author
                output_ << "No author found. Do you want to add " << author_input << " (y/n)?"
                        << std::endl;
                author_prompted = true;
                std::string answer;
                if (std::getline(input_, answer) && (answer == "y" || answer == "Y")) {
                    use_cases_.AddAuthor(author_input);
                    author_name = author_input;
                    // Re-fetch to get the ID
                    auto new_author = use_cases_.FindAuthorByName(author_input);
                    if (new_author) {
                        author_id = new_author->id;
                        author_valid = true;
                    }
                }
            }
        }

        if (!author_valid) {
            if (author_prompted) {
                // User was asked about adding author but said no
                output_ << "Failed to add book"sv << std::endl;
            }
            // Use poll to check if stdin has data available before reading tags
            struct pollfd pfd;
            pfd.fd = STDIN_FILENO;
            pfd.events = POLLIN;
            int ret = poll(&pfd, 1, 50);  // 50ms timeout
            if (ret > 0 && (pfd.revents & POLLIN)) {
                // Input available - read tags to prevent them being interpreted as commands
                std::string tags_str;
                if (!std::getline(input_, tags_str)) {
                    tags_str = "";
                }
            }
            return true;
        }

        // Get tags
        output_ << "Enter tags (comma separated):" << std::endl;
        std::string tags_str;
        if (!std::getline(input_, tags_str)) {
            tags_str = "";
        }

        auto tags = ParseTags(tags_str);
        use_cases_.AddBook(author_name, title, publication_year, tags);
    } catch (const std::exception& e) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors() const {
    PrintAuthors(output_, GetAuthors());
    return true;
}

bool View::ShowBooks() const {
    PrintBooks(output_, GetBooks());
    return true;
}

bool View::ShowAuthorBooks() const {
    try {
        auto authors = GetAuthors();
        if (authors.empty()) {
            return true;
        }
        output_ << "Select author:" << std::endl;
        PrintAuthors(output_, authors);
        output_ << "Enter author # or empty line to cancel" << std::endl;

        std::string str;
        if (!std::getline(input_, str) || str.empty()) {
            return true;
        }

        int author_idx;
        try {
            author_idx = std::stoi(str);
        } catch (std::exception const&) {
            throw std::runtime_error("Invalid author num");
        }

        --author_idx;
        if (author_idx < 0 || author_idx >= static_cast<int>(authors.size())) {
            throw std::runtime_error("Invalid author num");
        }

        auto author_id = authors[author_idx].id;
        PrintBooks(output_, GetAuthorBooks(author_id));
    } catch (const std::exception&) {
        output_ << "Failed to Show Books" << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        std::string author_id;

        if (name.empty()) {
            // Select from list
            auto author_opt = SelectAuthor();
            if (!author_opt) {
                output_ << "Failed to delete author"sv << std::endl;
                return true;
            }
            author_id = *author_opt;
        } else {
            // Find by name
            auto author = use_cases_.FindAuthorByName(name);
            if (!author) {
                output_ << "Failed to delete author"sv << std::endl;
                return true;
            }
            author_id = author->id;
        }

        if (!use_cases_.DeleteAuthor(author_id)) {
            output_ << "Failed to delete author"sv << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Failed to delete author"sv << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        std::string author_id;

        if (name.empty()) {
            // Select from list
            auto authors = GetAuthors();
            if (authors.empty()) {
                return true;
            }
            output_ << "Select author:" << std::endl;
            PrintAuthors(output_, authors);
            output_ << "Enter author # or empty line to cancel" << std::endl;

            std::string str;
            if (!std::getline(input_, str) || str.empty()) {
                return true;
            }

            int author_idx;
            try {
                author_idx = std::stoi(str);
            } catch (std::exception const&) {
                throw std::runtime_error("Invalid author num");
            }

            --author_idx;
            if (author_idx < 0 || author_idx >= static_cast<int>(authors.size())) {
                throw std::runtime_error("Invalid author num");
            }

            author_id = authors[author_idx].id;
        } else {
            // Find by name
            auto author = use_cases_.FindAuthorByName(name);
            if (!author) {
                output_ << "Failed to edit author"sv << std::endl;
                return true;
            }
            author_id = author->id;
        }

        output_ << "Enter new name:" << std::endl;
        std::string new_name;
        if (!std::getline(input_, new_name)) {
            output_ << "Failed to edit author"sv << std::endl;
            return true;
        }
        boost::algorithm::trim(new_name);

        if (new_name.empty()) {
            return true;
        }

        if (!use_cases_.EditAuthor(author_id, new_name)) {
            output_ << "Failed to edit author"sv << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Failed to edit author"sv << std::endl;
    }
    return true;
}

bool View::ShowBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::string book_id;

        if (title.empty()) {
            // Show all books and select
            auto books = GetBooks();
            if (books.empty()) {
                return true;
            }
            PrintBooks(output_, books);
            output_ << "Enter the book # or empty line to cancel:" << std::endl;
            std::string str;
            if (!std::getline(input_, str) || str.empty()) {
                return true;
            }
            int idx = std::stoi(str) - 1;
            if (idx < 0 || idx >= static_cast<int>(books.size())) {
                return true;
            }
            book_id = books[idx].id;
        } else {
            // Find by title
            auto books = GetBooksByTitle(title);
            if (books.empty()) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            if (books.size() == 1) {
                book_id = books[0].id;
            } else {
                // Multiple books - show list and select
                PrintBooks(output_, books);
                output_ << "Enter the book # or empty line to cancel:" << std::endl;
                std::string str;
                if (!std::getline(input_, str) || str.empty()) {
                    return true;
                }
                int idx = std::stoi(str) - 1;
                if (idx < 0 || idx >= static_cast<int>(books.size())) {
                    return true;
                }
                book_id = books[idx].id;
            }
        }

        auto details = GetBookDetails(book_id);
        if (details) {
            output_ << "Title: " << details->title << std::endl;
            output_ << "Author: " << details->author_name << std::endl;
            output_ << "Publication year: " << details->publication_year << std::endl;
            if (!details->tags.empty()) {
                output_ << "Tags: " << FormatTags(details->tags) << std::endl;
            }
        }
    } catch (const std::exception&) {
        // Silently handle
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::string book_id;

        if (title.empty()) {
            // Show all books and select
            auto books = GetBooks();
            if (books.empty()) {
                return true;
            }
            PrintBooks(output_, books);
            output_ << "Enter the book # or empty line to cancel:" << std::endl;
            std::string str;
            if (!std::getline(input_, str) || str.empty()) {
                return true;
            }
            int idx = std::stoi(str) - 1;
            if (idx < 0 || idx >= static_cast<int>(books.size())) {
                return true;
            }
            book_id = books[idx].id;
        } else {
            // Find by title
            auto books = GetBooksByTitle(title);
            if (books.empty()) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            if (books.size() == 1) {
                book_id = books[0].id;
            } else {
                // Multiple books - show list and select
                PrintBooks(output_, books);
                output_ << "Enter the book # or empty line to cancel:" << std::endl;
                std::string str;
                if (!std::getline(input_, str) || str.empty()) {
                    return true;
                }
                int idx = std::stoi(str) - 1;
                if (idx < 0 || idx >= static_cast<int>(books.size())) {
                    return true;
                }
                book_id = books[idx].id;
            }
        }

        if (!use_cases_.DeleteBook(book_id)) {
            output_ << "Failed to delete book"sv << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Failed to delete book"sv << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::string book_id;

        if (title.empty()) {
            // Show all books and select
            auto books = GetBooks();
            if (books.empty()) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            PrintBooks(output_, books);
            output_ << "Enter the book # or empty line to cancel:" << std::endl;
            std::string str;
            if (!std::getline(input_, str) || str.empty()) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            int idx = std::stoi(str) - 1;
            if (idx < 0 || idx >= static_cast<int>(books.size())) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            book_id = books[idx].id;
        } else {
            // Find by title
            auto books = GetBooksByTitle(title);
            if (books.empty()) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            if (books.size() == 1) {
                book_id = books[0].id;
            } else {
                // Multiple books - show list and select
                PrintBooks(output_, books);
                output_ << "Enter the book # or empty line to cancel:" << std::endl;
                std::string str;
                if (!std::getline(input_, str) || str.empty()) {
                    output_ << "Book not found"sv << std::endl;
                    return true;
                }
                int idx = std::stoi(str) - 1;
                if (idx < 0 || idx >= static_cast<int>(books.size())) {
                    output_ << "Book not found"sv << std::endl;
                    return true;
                }
                book_id = books[idx].id;
            }
        }

        // Get current details
        auto details = GetBookDetails(book_id);
        if (!details) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }

        // Enter new title
        output_ << "Enter new title or empty line to use the current one (" << details->title
                << "):" << std::endl;
        std::string new_title;
        if (!std::getline(input_, new_title)) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }
        boost::algorithm::trim(new_title);

        std::optional<std::string> title_opt;
        if (!new_title.empty()) {
            title_opt = new_title;
        }

        // Enter new year
        output_ << "Enter publication year or empty line to use the current one ("
                << details->publication_year << "):" << std::endl;
        std::string year_str;
        if (!std::getline(input_, year_str)) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }
        boost::algorithm::trim(year_str);

        std::optional<int> year_opt;
        if (!year_str.empty()) {
            year_opt = std::stoi(year_str);
        }

        // Enter new tags
        std::string current_tags_str = FormatTags(details->tags);
        output_ << "Enter tags (current tags: " << current_tags_str << "):" << std::endl;
        std::string tags_str;
        if (!std::getline(input_, tags_str)) {
            tags_str = "";
        }

        std::optional<std::set<std::string>> tags_opt;
        if (!tags_str.empty()) {
            tags_opt = ParseTags(tags_str);
        } else {
            // Empty tags string means clear all tags
            tags_opt = std::set<std::string>{};
        }

        if (!use_cases_.EditBook(book_id, title_opt, year_opt, tags_opt)) {
            output_ << "Failed to edit book"sv << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Book not found"sv << std::endl;
    }
    return true;
}

std::optional<detail::AddBookParams> View::GetBookParams(std::istream& cmd_input) const {
    detail::AddBookParams params;

    cmd_input >> params.publication_year;
    std::getline(cmd_input, params.title);
    boost::algorithm::trim(params.title);

    auto author_id = SelectAuthor();
    if (not author_id.has_value())
        return std::nullopt;
    else {
        params.author_id = author_id.value();
        return params;
    }
}

std::optional<std::string> View::SelectAuthor() const {
    output_ << "Select author:" << std::endl;
    auto authors = GetAuthors();
    PrintAuthors(output_, authors);
    output_ << "Enter author # or empty line to cancel" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int author_idx;
    try {
        author_idx = std::stoi(str);
    } catch (std::exception const&) {
        throw std::runtime_error("Invalid author num");
    }

    --author_idx;
    if (author_idx < 0 || author_idx >= static_cast<int>(authors.size())) {
        throw std::runtime_error("Invalid author num");
    }

    return authors[author_idx].id;
}

std::optional<std::string> View::SelectAuthorOrEnter() const {
    output_ << "Enter author name or empty line to select from list:" << std::endl;
    std::string author_input;
    if (!std::getline(input_, author_input)) {
        return std::nullopt;
    }
    boost::algorithm::trim(author_input);

    if (author_input.empty()) {
        return SelectAuthor();
    }

    // Check if author exists
    auto existing = use_cases_.FindAuthorByName(author_input);
    if (existing) {
        return existing->id;
    }

    // Ask to add
    output_ << "No author found. Do you want to add " << author_input << " (y/n)?" << std::endl;
    std::string answer;
    if (!std::getline(input_, answer) || (answer != "y" && answer != "Y")) {
        return std::nullopt;
    }

    use_cases_.AddAuthor(author_input);
    auto new_author = use_cases_.FindAuthorByName(author_input);
    if (!new_author) {
        return std::nullopt;
    }
    return new_author->id;
}

std::vector<detail::AuthorInfo> View::GetAuthors() const {
    std::vector<detail::AuthorInfo> result;
    auto authors = use_cases_.GetAuthors();
    for (const auto& a : authors) {
        result.push_back({a.id, a.name});
    }
    return result;
}

std::vector<detail::BookInfo> View::GetBooks() const {
    std::vector<detail::BookInfo> result;
    auto books = use_cases_.GetBooks();
    for (const auto& b : books) {
        result.push_back({b.id, b.title, b.author_name, b.publication_year, b.tags});
    }
    return result;
}

std::vector<detail::BookInfo> View::GetAuthorBooks(const std::string& author_id) const {
    std::vector<detail::BookInfo> result;
    auto books = use_cases_.GetAuthorBooks(author_id);
    for (const auto& b : books) {
        result.push_back({b.id, b.title, "", b.publication_year, b.tags});
    }
    return result;
}

std::vector<detail::BookInfo> View::GetBooksByTitle(const std::string& title) const {
    std::vector<detail::BookInfo> result;
    auto books = use_cases_.GetBooksByTitle(title);
    for (const auto& b : books) {
        result.push_back({b.id, b.title, b.author_name, b.publication_year, b.tags});
    }
    return result;
}

std::optional<detail::BookDetails> View::GetBookDetails(const std::string& book_id) const {
    auto details = use_cases_.GetBookDetails(book_id);
    if (!details) {
        return std::nullopt;
    }
    return detail::BookDetails{details->id, details->title, details->author_name,
                               details->publication_year, details->tags};
}

std::set<std::string> View::ParseTags(const std::string& tags_str) const {
    std::set<std::string> tags;
    std::istringstream ss(tags_str);
    std::string token;

    while (std::getline(ss, token, ',')) {
        boost::algorithm::trim(token);
        // Normalize internal spaces - collapse multiple spaces to single
        std::string normalized;
        bool last_was_space = false;
        for (char c : token) {
            if (c == ' ') {
                if (!last_was_space) {
                    normalized += c;
                }
                last_was_space = true;
            } else {
                normalized += c;
                last_was_space = false;
            }
        }
        // Trim trailing space
        if (!normalized.empty() && normalized.back() == ' ') {
            normalized.pop_back();
        }
        if (!normalized.empty()) {
            tags.insert(std::move(normalized));
        }
    }

    return tags;
}

std::string View::FormatTags(const std::set<std::string>& tags) const {
    std::string result;
    bool first = true;
    for (const auto& tag : tags) {
        if (!first) {
            result += ", ";
        }
        result += tag;
        first = false;
    }
    return result;
}

}  // namespace ui
