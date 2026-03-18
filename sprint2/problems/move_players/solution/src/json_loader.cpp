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

// Загрузка одной карты
model::Map LoadMap(const boost::json::object& map_obj, double default_dog_speed) {
    std::string id = map_obj.at("id").as_string().c_str();
    std::string name = map_obj.at("name").as_string().c_str();
    
    double dog_speed = default_dog_speed;
    if (map_obj.contains("dogSpeed")) {
        dog_speed = map_obj.at("dogSpeed").as_double();
    }
    
    model::Map map(model::Map::Id(id), name, dog_speed);

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

model::Game LoadGame(const std::filesystem::path& json_path) {
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
    
    // Читаем defaultDogSpeed, если присутствует
    double default_dog_speed = 1.0;
    if (root.contains("defaultDogSpeed")) {
        default_dog_speed = root.at("defaultDogSpeed").as_double();
    }
    
    model::Game game(default_dog_speed);

    if (!root.contains("maps")) {
        return game;
    }

    for (auto const& map_val : root.at("maps").as_array()) {
        game.AddMap(LoadMap(map_val.as_object(), default_dog_speed));
    }

    return game;
}

}  // namespace json_loader