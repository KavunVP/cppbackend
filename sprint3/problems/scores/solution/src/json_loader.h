#pragma once

#include <filesystem>
#include <utility>

#include "extra_data.h"
#include "model.h"

namespace json_loader {

struct GameData {
    model::Game game;
    extra_data::MapExtraData extra_data;
};

GameData LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader
