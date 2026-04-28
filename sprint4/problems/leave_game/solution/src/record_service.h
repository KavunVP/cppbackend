#pragma once

#include "db_connection_pool.h"
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <pqxx/pqxx>
#include "constants.h"

// Структура для хранения информации о рекорде
struct RecordEntry {
    std::string name;
    int score;
    double play_time;  // в секундах
};

// Сервис для работы с таблицей рекордов в PostgreSQL
class RecordService {
public:
    // Инициализация сервиса: создание пула соединений и таблицы при необходимости
    explicit RecordService(std::string db_url, size_t pool_size = 5)
        : db_url_{std::move(db_url)}
        , pool_{pool_size, [this]() {
            return std::make_shared<pqxx::connection>(db_url_);
        }} {
        EnsureTableExists();
    }

    // Добавить запись о вышедшем на покой игроке
    void AddRecord(const std::string& name, int score, std::chrono::milliseconds play_time_ms) {
        auto conn = pool_.GetConnection();
        pqxx::work txn{*conn};
        txn.exec_params(
            "INSERT INTO retired_players (id, name, score, play_time_ms) "
            "VALUES (gen_random_uuid(), $1, $2, $3)",
            name, score, static_cast<int>(play_time_ms.count()));
        txn.commit();
    }

    // Получить список рекордов с пагинацией
    // start - номер начального элемента (0 - начальный)
    // maxItems - максимальное количество элементов (по умолчанию 100, максимум 100)
    std::vector<RecordEntry> GetRecords(size_t start = 0, std::optional<size_t> maxItems = std::nullopt) {
        size_t limit = maxItems.value_or(100);
        if (limit > 100) {
            throw std::invalid_argument("maxItems cannot exceed 100");
        }

        auto conn = pool_.GetConnection();
        pqxx::work txn{*conn};

        // Сортировка: score DESC, play_time_ms ASC, name ASC
        pqxx::result result = txn.exec_params(
            "SELECT name, score, play_time_ms FROM retired_players "
            "ORDER BY score DESC, play_time_ms ASC, name ASC "
            "LIMIT $1 OFFSET $2",
            static_cast<int>(limit), static_cast<int>(start));

        std::vector<RecordEntry> records;
        records.reserve(result.size());
        for (const auto& row : result) {
            records.push_back({
                row[0].as<std::string>(),
                row[1].as<int>(),
                row[2].as<int>() / MILLISECONDS_PER_SECOND  // переводим миллисекунды в секунды
            });
        }

        return records;
    }

    // Проверить, существует ли таблица, и создать при необходимости
    void EnsureTableExists() {
        auto conn = pool_.GetConnection();
        pqxx::work txn{*conn};

        // Создаём таблицу, если не существует
        txn.exec(
            "CREATE TABLE IF NOT EXISTS retired_players ("
            "    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),"
            "    name VARCHAR(100) NOT NULL,"
            "    score INT NOT NULL,"
            "    play_time_ms INT NOT NULL"
            ")");

        // Создаём мультииндекс для быстрой сортировки
        txn.exec(
            "CREATE INDEX IF NOT EXISTS idx_retired_players_score_playtime_name "
            "ON retired_players (score DESC, play_time_ms ASC, name ASC)");

        txn.commit();
    }

private:
    std::string db_url_;
    ConnectionPool pool_;
};
