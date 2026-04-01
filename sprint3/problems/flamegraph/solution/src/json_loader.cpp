#include "json_loader.h"
#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path) {
    model::Game game;

    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file");
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string data = buffer.str();

    auto value = boost::json::parse(data);
    auto const& root = value.as_object();

    if (!root.contains("maps")) {
        return game;
    }

    for (auto const& map_val : root.at("maps").as_array()) {
        auto const& map_obj = map_val.as_object();

        std::string id = map_obj.at("id").as_string().c_str();
        std::string name = map_obj.at("name").as_string().c_str();
        model::Map map(model::Map::Id(id), name);

        // Roads
        if (map_obj.contains("roads")) {
            for (auto const& road_val : map_obj.at("roads").as_array()) {
                auto const& road_obj = road_val.as_object();
                if (road_obj.contains("x1")) { // горизонтальная
                    int x0 = road_obj.at("x0").as_int64();
                    int y0 = road_obj.at("y0").as_int64();
                    int x1 = road_obj.at("x1").as_int64();
                    map.AddRoad(model::Road(model::Road::HORIZONTAL, {x0, y0}, x1));
                } else { // вертикальная
                    int x0 = road_obj.at("x0").as_int64();
                    int y0 = road_obj.at("y0").as_int64();
                    int y1 = road_obj.at("y1").as_int64();
                    map.AddRoad(model::Road(model::Road::VERTICAL, {x0, y0}, y1));
                }
            }
        }

        // Buildings
        if (map_obj.contains("buildings")) {
            for (auto const& building_val : map_obj.at("buildings").as_array()) {
                auto const& building_obj = building_val.as_object();
                int x = building_obj.at("x").as_int64();
                int y = building_obj.at("y").as_int64();
                int w = building_obj.at("w").as_int64();
                int h = building_obj.at("h").as_int64();
                map.AddBuilding(model::Building({{x, y}, {w, h}}));
            }
        }

        // Offices
        if (map_obj.contains("offices")) {
            for (auto const& office_val : map_obj.at("offices").as_array()) {
                auto const& office_obj = office_val.as_object();
                std::string office_id = office_obj.at("id").as_string().c_str();
                int x = office_obj.at("x").as_int64();
                int y = office_obj.at("y").as_int64();
                int offsetX = office_obj.at("offsetX").as_int64();
                int offsetY = office_obj.at("offsetY").as_int64();
                map.AddOffice(model::Office(model::Office::Id(office_id), {x, y}, {offsetX, offsetY}));
            }
        }

        game.AddMap(std::move(map));
    }

    return game;
}

}  // namespace json_loader