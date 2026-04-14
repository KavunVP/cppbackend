#include <iostream>
#include <pqxx/pqxx>
#include <boost/json.hpp>

using namespace std::literals;
using pqxx::operator"" _zv;

namespace json = boost::json;

static json::value books_to_json(const pqxx::result& rows) {
    json::array arr;
    for (const auto& row : rows) {
        json::object obj;
        obj["id"] = row["id"].as<int>();
        obj["title"] = row["title"].as<std::string>();
        obj["author"] = row["author"].as<std::string>();
        obj["year"] = row["year"].as<int>();
        if (row["isbn"].is_null()) {
            obj["ISBN"] = json::value{};
        } else {
            obj["ISBN"] = row["isbn"].as<std::string>();
        }
        arr.emplace_back(std::move(obj));
    }
    return arr;
}

int main(int argc, const char* argv[]) {

    if (argc != 2) {
        std::cerr << "Usage: book_manager <conn-string>"sv << std::endl;
        return EXIT_FAILURE;
    }

    try {
        pqxx::connection conn{argv[1]};

        // Создаём таблицу, если её нет
        {
            pqxx::work w(conn);
            w.exec(
                "CREATE TABLE IF NOT EXISTS books ("
                "id SERIAL PRIMARY KEY, "
                "title varchar(100) NOT NULL, "
                "author varchar(100) NOT NULL, "
                "year integer NOT NULL, "
                "ISBN char(13) UNIQUE"
                ");"_zv
            );
            w.commit();
        }

        // Подготавливаем запросы
        constexpr auto tag_ins_book = "ins_book"_zv;
        constexpr auto tag_sel_books = "sel_books"_zv;
        conn.prepare(tag_ins_book,
            "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4)"_zv);
        conn.prepare(tag_sel_books,
            "SELECT id, title, author, year, ISBN FROM books ORDER BY year DESC, title ASC, author ASC, ISBN ASC"_zv);

        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            json::value value = json::parse(line);
            const auto& root = value.as_object();
            auto action = root.at("action").as_string();

            if (action == "add_book") {
                const auto& payload = root.at("payload").as_object();
                std::string title = payload.at("title").as_string().c_str();
                std::string author = payload.at("author").as_string().c_str();
                int64_t year = payload.at("year").as_int64();

                pqxx::work w(conn);
                try {
                    pqxx::result res;
                    if (payload.contains("ISBN") && !payload.at("ISBN").is_null()) {
                        std::string isbn = payload.at("ISBN").as_string().c_str();
                        res = w.exec_prepared(tag_ins_book, title, author, year, isbn);
                    } else {
                        res = w.exec_prepared(tag_ins_book, title, author, year, nullptr);
                    }
                    w.commit();
                    std::cout << "{\"result\":true}" << std::endl;
                } catch (const pqxx::sql_error&) {
                    w.abort();
                    std::cout << "{\"result\":false}" << std::endl;
                }

            } else if (action == "all_books") {
                pqxx::work w(conn);
                pqxx::result rows = w.exec_prepared(tag_sel_books);
                w.commit();
                json::value result = books_to_json(rows);
                std::cout << json::serialize(result) << std::endl;

            } else if (action == "exit") {
                break;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
