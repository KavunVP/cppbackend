#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <string_view>
#include <filesystem>
#include <fstream> 

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, std::string_view static_dir)
        : game_{game}, static_dir_{std::filesystem::path(static_dir).lexically_normal()} {
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

        // Если запрос начинается с /api/, обрабатываем как REST API
        if (target.starts_with("/api/")) {
            HandleApiRequest(target, req, send);
        } else {
            HandleStaticRequest(target, req, send);
        }
    }

private:

    template <typename Body, typename Allocator, typename Send>
    void HandleApiRequest(std::string_view target,
                          http::request<Body, http::basic_fields<Allocator>>& req,
                          Send& send) {
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

    template <typename Body, typename Allocator, typename Send>
    void HandleStaticRequest(std::string_view target,
                             http::request<Body, http::basic_fields<Allocator>>& req,
                             Send& send) {
        // Декодируем URL 
        std::string decoded_target;
        try {
            decoded_target = UrlDecode(target);
        } catch (const std::exception&) {
            return send(MakePlainResponse(http::status::bad_request, "Invalid URL encoding",
                                          req.version(), req.keep_alive()));
        }

        // Строим полный путь
        std::filesystem::path req_path = decoded_target;
        // Убираем ведущий слеш для безопасного соединения
        if (!req_path.empty() && req_path.has_root_path()) {
            req_path = req_path.relative_path();
        }
        std::filesystem::path full_path = static_dir_ / req_path;
        std::error_code ec;
        full_path = std::filesystem::weakly_canonical(full_path, ec);
        if (ec) {
            return send(MakePlainResponse(http::status::bad_request, "Invalid path",
                                          req.version(), req.keep_alive()));
        }

        // Проверяем, что путь не выходит за пределы static_dir_
        if (!IsSubPath(full_path, static_dir_)) {
            return send(MakePlainResponse(http::status::bad_request, "Path is outside static directory",
                                          req.version(), req.keep_alive()));
        }

        // Если это директория, добавляем index.html
        if (std::filesystem::is_directory(full_path, ec)) {
            full_path /= "index.html";
        }

        // Проверяем существование файла
        if (!std::filesystem::is_regular_file(full_path, ec) || ec) {
            return send(MakePlainResponse(http::status::not_found, "File not found",
                                          req.version(), req.keep_alive()));
        }

        // Определяем MIME-тип
        std::string content_type = GetMimeType(full_path.extension().string());

        // Читаем файл
        std::ifstream file(full_path, std::ios::binary);
        if (!file.is_open()) {
            return send(MakePlainResponse(http::status::internal_server_error, "Cannot open file",
                                          req.version(), req.keep_alive()));
        }
        std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        // Формируем ответ
        if (req.method() == http::verb::head) {
            // Для HEAD ответ без тела
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, content_type);
            res.content_length(body.size());
            res.keep_alive(req.keep_alive());
            return send(std::move(res));
        } else {
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, content_type);
            res.body() = std::move(body);
            res.prepare_payload();
            res.keep_alive(req.keep_alive());
            return send(std::move(res));
        }
    }

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

    // Вспомогательные функции для статики
    static std::string UrlDecode(std::string_view encoded);
    static std::string GetMimeType(const std::string& extension);
    static http::response<http::string_body> MakePlainResponse(http::status status, std::string_view text,
                                                               unsigned version, bool keep_alive);
    // Проверка, что path находится внутри base
    static bool IsSubPath(const std::filesystem::path& path, const std::filesystem::path& base);

    model::Game& game_;
    std::filesystem::path static_dir_;
};

}  // namespace http_handler
