#pragma once

#include <boost/json.hpp>
#include <string>
#include <unordered_map>

#include "model.h"

namespace extra_data {

// Хранит JSON-данные, специфичные для фронтенда (lootTypes и т.д.),
// отдельно от игровой модели.
class MapExtraData {
public:
    MapExtraData() = default;
    MapExtraData(MapExtraData&&) = default;
    MapExtraData& operator=(MapExtraData&&) = default;
    MapExtraData(const MapExtraData&) = delete;
    MapExtraData& operator=(const MapExtraData&) = delete;

    // Установить JSON-массив lootTypes для карты
    void SetLootTypes(model::Map::Id map_id, boost::json::array loot_types) {
        loot_types_[std::move(map_id)] = std::move(loot_types);
    }

    // Получить JSON-массив lootTypes для карты
    const boost::json::array* GetLootTypes(const model::Map::Id& map_id) const {
        auto it = loot_types_.find(map_id);
        if (it != loot_types_.end()) {
            return &it->second;
        }
        return nullptr;
    }

private:
    using MapIdHasher = util::TaggedHasher<model::Map::Id>;
    std::unordered_map<model::Map::Id, boost::json::array, MapIdHasher> loot_types_;
};

}  // namespace extra_data
