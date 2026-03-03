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
            std::string body = SerializeMapsList(game_.GetMaps());
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
        std::string body = SerializeMap(*map);
        return send(MakeJsonResponse(http::status::ok, body, req.version(), req.keep_alive()));
    }

private:
    static http::response<http::string_body> MakeJsonResponse(http::status status, std::string_view body,
                                                              unsigned version, bool keep_alive);
    static http::response<http::string_body> MakeErrorResponse(http::status status, std::string_view message,
                                                               unsigned version, bool keep_alive,
                                                               std::string_view code = "badRequest");

    // Сериализация списка карт (id и name)
    static std::string SerializeMapsList(const model::Game::Maps& maps);

    // Сериализация полной информации о карте
    static std::string SerializeMap(const model::Map& map);

    // Вспомогательные функции сериализации подобъектов
    static boost::json::array SerializeRoads(const model::Map::Roads& roads);
    static boost::json::array SerializeBuildings(const model::Map::Buildings& buildings);
    static boost::json::array SerializeOffices(const model::Map::Offices& offices);

    model::Game& game_;
};

}  // namespace http_handler
