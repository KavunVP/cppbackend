#pragma once

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/vector.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "model.h"
#include "state_serialization.h"

namespace state_persistence {

using namespace std::literals;

// Сервис сохранения и загрузки состояния игры
class SaveLoadService {
public:
    // Если state_file пустой — сохранение отключено
    SaveLoadService(std::filesystem::path state_file,
                    std::chrono::milliseconds save_period = std::chrono::milliseconds{0})
        : state_file_(std::move(state_file))
        , save_period_(save_period) {
    }

    // Загрузить состояние из файла. Возвращает true, если загрузка успешна.
    // Если файла нет — возвращает true (чистый старт).
    // Если файл есть, но данные повреждены — логирует ошибку и возвращает false.
    bool LoadState(model::Game& game) {
        if (state_file_.empty()) {
            return true;  // Сохранение отключено
        }

        std::error_code ec;
        if (!std::filesystem::exists(state_file_, ec) || ec) {
            return true;  // Файла нет — чистый старт
        }

        try {
            std::ifstream ifs(state_file_, std::ios::binary);
            if (!ifs.is_open()) {
                return false;
            }

            boost::archive::text_iarchive ar(ifs);
            serialization::GameStateRepr repr;
            ar >> repr;

            DeserializeGameState(repr, game);
            return true;
        } catch (const std::exception& e) {
            // Логирование будет вызвано из main
            last_load_error_ = e.what();
            return false;
        }
    }

    // Сохранить состояние в файл (атомарно: через временный файл + rename)
    void SaveState(const model::Game& game) {
        if (state_file_.empty()) {
            return;  // Сохранение отключено
        }

        try {
            auto repr = serialization::SerializeGameState(game);

            // Пишем во временный файл
            auto temp_path = state_file_.string() + ".tmp";

            std::ofstream ofs(temp_path, std::ios::binary);
            if (!ofs.is_open()) {
                return;
            }

            boost::archive::text_oarchive ar(ofs);
            ar << repr;
            ofs.flush();
            ofs.close();

            // Атомарное переименование
            std::error_code ec;
            std::filesystem::rename(temp_path, state_file_, ec);
            // Игнорируем ошибку rename — в худшем случае останется старый файл
        } catch (const std::exception&) {
            // Игнорируем ошибки сохранения
        }
    }

    // Вызывается при каждом тике. Сохраняет состояние, если прошёл период.
    void OnTick(std::chrono::milliseconds delta) {
        if (!save_period_valid_ || state_file_.empty()) {
            return;
        }

        elapsed_since_save_ += delta;
        if (elapsed_since_save_ >= save_period_) {
            // elapsed сбрасываем относительно периода (чтобы не копить drift)
            elapsed_since_save_ = elapsed_since_save_ % save_period_;
            SaveState(*game_ref_);
        }
    }

    // Привязать к Game (нужно для OnTick)
    void BindGame(model::Game& game) {
        game_ref_ = &game;
    }

    // Принудительное сохранение (при завершении работы)
    void SaveNow() {
        if (game_ref_) {
            SaveState(*game_ref_);
        }
    }

    const std::string& GetLoadError() const noexcept {
        return last_load_error_;
    }

    bool IsEnabled() const noexcept {
        return !state_file_.empty();
    }

private:
    std::filesystem::path state_file_;
    std::chrono::milliseconds save_period_;
    bool save_period_valid_ = false;
    std::chrono::milliseconds elapsed_since_save_{0};
    model::Game* game_ref_ = nullptr;
    std::string last_load_error_;

public:
    // Инициализация: проверка валидности периода
    void Init() {
        save_period_valid_ = (save_period_ > std::chrono::milliseconds{0});
    }
};

}  // namespace state_persistence
