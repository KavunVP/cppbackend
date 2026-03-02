#include "request_handler.h"

namespace http_handler {

http::response<http::string_body> RequestHandler::MakeJsonResponse(http::status status, std::string_view body,
                                                                    unsigned version, bool keep_alive) {
    http::response<http::string_body> res(status, version);
    res.set(http::field::content_type, "application/json");
    res.body() = body;
    res.content_length(body.size());
    res.keep_alive(keep_alive);
    return res;
}

http::response<http::string_body> RequestHandler::MakeErrorResponse(http::status status, std::string_view message,
                                                                     unsigned version, bool keep_alive,
                                                                     std::string_view code) {
    boost::json::object obj;
    obj["code"] = boost::json::string_view(code.data(), code.size());
    obj["message"] = boost::json::string_view(message.data(), message.size());
    std::string body = boost::json::serialize(obj);
    return MakeJsonResponse(status, body, version, keep_alive);
}

std::string RequestHandler::SerializeMapsList(const model::Game::Maps& maps) {
    boost::json::array arr;
    for (const auto& map : maps) {
        boost::json::object obj;
        obj["id"] = *map.GetId();
        obj["name"] = map.GetName();
        arr.push_back(std::move(obj));
    }
    return boost::json::serialize(arr);
}

std::string RequestHandler::SerializeMap(const model::Map& map) {
    boost::json::object obj;
    obj["id"] = *map.GetId();
    obj["name"] = map.GetName();
    obj["roads"] = SerializeRoads(map.GetRoads());
    obj["buildings"] = SerializeBuildings(map.GetBuildings());
    obj["offices"] = SerializeOffices(map.GetOffices());
    return boost::json::serialize(obj);
}

boost::json::array RequestHandler::SerializeRoads(const model::Map::Roads& roads) {
    boost::json::array arr;
    for (const auto& road : roads) {
        boost::json::object road_obj;
        auto start = road.GetStart();
        auto end = road.GetEnd();
        if (road.IsHorizontal()) {
            road_obj["x0"] = start.x;
            road_obj["y0"] = start.y;
            road_obj["x1"] = end.x;
        } else {
            road_obj["x0"] = start.x;
            road_obj["y0"] = start.y;
            road_obj["y1"] = end.y;
        }
        arr.push_back(std::move(road_obj));
    }
    return arr;
}

boost::json::array RequestHandler::SerializeBuildings(const model::Map::Buildings& buildings) {
    boost::json::array arr;
    for (const auto& building : buildings) {
        boost::json::object building_obj;
        const auto& bounds = building.GetBounds();
        building_obj["x"] = bounds.position.x;
        building_obj["y"] = bounds.position.y;
        building_obj["w"] = bounds.size.width;
        building_obj["h"] = bounds.size.height;
        arr.push_back(std::move(building_obj));
    }
    return arr;
}

boost::json::array RequestHandler::SerializeOffices(const model::Map::Offices& offices) {
    boost::json::array arr;
    for (const auto& office : offices) {
        boost::json::object office_obj;
        office_obj["id"] = *office.GetId();
        office_obj["x"] = office.GetPosition().x;
        office_obj["y"] = office.GetPosition().y;
        office_obj["offsetX"] = office.GetOffset().dx;
        office_obj["offsetY"] = office.GetOffset().dy;
        arr.push_back(std::move(office_obj));
    }
    return arr;
}

}  // namespace http_handler