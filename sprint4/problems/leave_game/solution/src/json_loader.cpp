#include "json_loader.h"
#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json_loader {

// Загрузка одной дороги из JSON-объекта
model::Road LoadRoad(const boost::json::object& obj) {
    int x0 = obj.at("x0").as_int64();
    int y0 = obj.at("y0").as_int64();
    if (obj.contains("x1")) { // горизонтальная
        int x1 = obj.at("x1").as_int64();
        return model::Road(model::Road::HORIZONTAL, {x0, y0}, x1);
    } else { // вертикальная
        int y1 = obj.at("y1").as_int64();
        return model::Road(model::Road::VERTICAL, {x0, y0}, y1);
    }
}

// Загрузка массива дорог
void LoadRoads(model::Map& map, const boost::json::array& roads_array) {
    for (const auto& road_val : roads_array) {
        map.AddRoad(LoadRoad(road_val.as_object()));
    }
}

// Загрузка одного здания
model::Building LoadBuilding(const boost::json::object& obj) {
    int x = obj.at("x").as_int64();
    int y = obj.at("y").as_int64();
    int w = obj.at("w").as_int64();
    int h = obj.at("h").as_int64();
    return model::Building({{x, y}, {w, h}});
}

void LoadBuildings(model::Map& map, const boost::json::array& buildings_array) {
    for (const auto& building_val : buildings_array) {
        map.AddBuilding(LoadBuilding(building_val.as_object()));
    }
}

// Загрузка одного офиса
model::Office LoadOffice(const boost::json::object& obj) {
    std::string id = obj.at("id").as_string().c_str();
    int x = obj.at("x").as_int64();
    int y = obj.at("y").as_int64();
    int offsetX = obj.at("offsetX").as_int64();
    int offsetY = obj.at("offsetY").as_int64();
    return model::Office(model::Office::Id(id), {x, y}, {offsetX, offsetY});
}

void LoadOffices(model::Map& map, const boost::json::array& offices_array) {
    for (const auto& office_val : offices_array) {
        map.AddOffice(LoadOffice(office_val.as_object()));
    }
}

// Загрузка массива lootTypes (просто возвращаем как JSON array)
boost::json::array LoadLootTypes(const boost::json::array& loot_types_array) {
    return loot_types_array;  // Просто копируем
}

// Загрузка значений value из lootTypes
std::vector<unsigned> LoadLootValues(const boost::json::array& loot_types_array) {
    std::vector<unsigned> values;
    values.reserve(loot_types_array.size());
    for (const auto& item : loot_types_array) {
        const auto& obj = item.as_object();
        if (obj.contains("value")) {
            values.push_back(static_cast<unsigned>(obj.at("value").as_int64()));
        } else {
            values.push_back(0);  // По умолчанию 0 очков
        }
    }
    return values;
}

// Загрузка одной карты
model::Map LoadMap(const boost::json::object& map_obj) {
    std::string id = map_obj.at("id").as_string().c_str();
    std::string name = map_obj.at("name").as_string().c_str();
    model::Map map(model::Map::Id(id), name);

    // Загружаем скорость собаки, если указана
    if (map_obj.contains("dogSpeed")) {
        double dog_speed = map_obj.at("dogSpeed").as_double();
        map.SetDogSpeed(dog_speed);
    }

    // Загружаем lootTypes, если указаны
    if (map_obj.contains("lootTypes")) {
        const auto& loot_types = map_obj.at("lootTypes").as_array();
        map.SetLootTypeCount(static_cast<unsigned>(loot_types.size()));
        map.SetLootValues(LoadLootValues(loot_types));
    }

    if (map_obj.contains("roads")) {
        LoadRoads(map, map_obj.at("roads").as_array());
    }
    if (map_obj.contains("buildings")) {
        LoadBuildings(map, map_obj.at("buildings").as_array());
    }
    if (map_obj.contains("offices")) {
        LoadOffices(map, map_obj.at("offices").as_array());
    }

    return map;
}

// Загрузка extra_data для карты (lootTypes как JSON)
void LoadMapExtraData(const boost::json::object& map_obj,
                      const model::Map& map,
                      extra_data::MapExtraData& extra) {
    if (map_obj.contains("lootTypes")) {
        boost::json::array loot_types = LoadLootTypes(map_obj.at("lootTypes").as_array());
        extra.SetLootTypes(map.GetId(), std::move(loot_types));
    }
}

GameData LoadGame(const std::filesystem::path& json_path) {
    GameData result;

    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file");
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string data = buffer.str();

    boost::json::value value;
    try {
        value = boost::json::parse(data);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to parse JSON: ") + e.what());
    }

    auto const& root = value.as_object();

    // Загружаем defaultDogSpeed, если указано
    double default_dog_speed = 0.0;
    if (root.contains("defaultDogSpeed")) {
        default_dog_speed = root.at("defaultDogSpeed").as_double();
    }

    // Загружаем defaultBagCapacity, если указано
    unsigned default_bag_capacity = 3;
    if (root.contains("defaultBagCapacity")) {
        default_bag_capacity = static_cast<unsigned>(root.at("defaultBagCapacity").as_int64());
    }
    result.game.SetDefaultBagCapacity(default_bag_capacity);

    // Загружаем lootGeneratorConfig, если указано
    if (root.contains("lootGeneratorConfig")) {
        const auto& config = root.at("lootGeneratorConfig").as_object();
        double period_sec = config.at("period").as_double();
        double probability = config.at("probability").as_double();
        auto period_ms = std::chrono::milliseconds{
            static_cast<uint64_t>(period_sec * 1000.0)};
        result.game.SetLootGeneratorConfig(period_ms, probability);
    }

    // Загружаем dogRetirementTime, если указано (в секундах)
    if (root.contains("dogRetirementTime")) {
        double retirement_sec = root.at("dogRetirementTime").as_double();
        auto retirement_ms = std::chrono::milliseconds{
            static_cast<uint64_t>(retirement_sec * 1000.0)};
        result.game.SetDogRetirementTime(retirement_ms);
    }

    if (!root.contains("maps")) {
        return result;
    }

    for (auto const& map_val : root.at("maps").as_array()) {
        const auto& map_obj = map_val.as_object();
        auto map = LoadMap(map_obj);
        // Если dogSpeed не указана для карты, используем defaultDogSpeed
        if (map.GetDogSpeed() == 0.0 && default_dog_speed != 0.0) {
            map.SetDogSpeed(default_dog_speed);
        }

        // Если bagCapacity не указана для карты, используем defaultBagCapacity
        if (map_obj.contains("bagCapacity")) {
            unsigned bag_capacity = static_cast<unsigned>(map_obj.at("bagCapacity").as_int64());
            map.SetBagCapacity(bag_capacity);
        } else {
            map.SetBagCapacity(default_bag_capacity);
        }

        // Сохраняем extra_data для этой карты
        LoadMapExtraData(map_obj, map, result.extra_data);

        result.game.AddMap(std::move(map));
    }

    return result;
}

}  // namespace json_loader
