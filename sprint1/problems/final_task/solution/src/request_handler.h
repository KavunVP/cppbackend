#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>      
#include <string_view>         


namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;


class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Только GET
        if (req.method() != http::verb::get) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "Only GET method allowed",
                                          req.version(), req.keep_alive()));
        }

        std::string_view target = req.target();
        auto pos = target.find('?');
        if (pos != std::string_view::npos) {
            target = target.substr(0, pos);
        }

        constexpr std::string_view api_prefix = "/api/v1/maps";
        if (!target.starts_with(api_prefix)) {
            return send(MakeErrorResponse(http::status::bad_request, "Bad request",
                                          req.version(), req.keep_alive()));
        }

        if (target == api_prefix) {
            // Список карт
            boost::json::array arr;
            for (const auto& map : game_.GetMaps()) {
                boost::json::object obj;
                obj["id"] = *map.GetId();
                obj["name"] = map.GetName();
                arr.push_back(std::move(obj));
            }
            std::string body = boost::json::serialize(arr);
            return send(MakeJsonResponse(http::status::ok, body, req.version(), req.keep_alive()));
        }

        // Проверка наличия / после префикса и id
        if (target.size() <= api_prefix.size() + 1 || target[api_prefix.size()] != '/') {
            return send(MakeErrorResponse(http::status::bad_request, "Bad request",
                                          req.version(), req.keep_alive()));
        }

        std::string_view map_id = target.substr(api_prefix.size() + 1);
        const auto* map = game_.FindMap(model::Map::Id(std::string(map_id)));
        if (!map) {
            return send(MakeErrorResponse(http::status::not_found, "Map not found",
                                          req.version(), req.keep_alive(), "mapNotFound"));
        }

        // Полная информация о карте
        boost::json::object obj;
        obj["id"] = *map->GetId();
        obj["name"] = map->GetName();

        // Roads
        boost::json::array roads_arr;
        for (const auto& road : map->GetRoads()) {
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
            roads_arr.push_back(std::move(road_obj));
        }
        obj["roads"] = std::move(roads_arr);

        // Buildings
        boost::json::array buildings_arr;
        for (const auto& building : map->GetBuildings()) {
            boost::json::object building_obj;
            const auto& bounds = building.GetBounds();
            building_obj["x"] = bounds.position.x;
            building_obj["y"] = bounds.position.y;
            building_obj["w"] = bounds.size.width;
            building_obj["h"] = bounds.size.height;
            buildings_arr.push_back(std::move(building_obj));
        }
        obj["buildings"] = std::move(buildings_arr);

        // Offices
        boost::json::array offices_arr;
        for (const auto& office : map->GetOffices()) {
            boost::json::object office_obj;
            office_obj["id"] = *office.GetId();
            office_obj["x"] = office.GetPosition().x;
            office_obj["y"] = office.GetPosition().y;
            office_obj["offsetX"] = office.GetOffset().dx;
            office_obj["offsetY"] = office.GetOffset().dy;
            offices_arr.push_back(std::move(office_obj));
        }
        obj["offices"] = std::move(offices_arr);

        std::string body = boost::json::serialize(obj);
        return send(MakeJsonResponse(http::status::ok, body, req.version(), req.keep_alive()));
    }


private:
    static http::response<http::string_body> MakeJsonResponse(http::status status, std::string_view body,
                                                              unsigned version, bool keep_alive);
    static http::response<http::string_body> MakeErrorResponse(http::status status, std::string_view message,
                                                               unsigned version, bool keep_alive,
                                                               std::string_view code = "badRequest");

    model::Game& game_;
};

}  // namespace http_handler
