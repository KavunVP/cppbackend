#pragma once
#include "http_server.h"
#include "model.h"
#include "logging.h"
#include <boost/json.hpp>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cassert>
#include <memory>
#include <optional>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

// Обработчик статических файлов
class StaticHandler {
public:
    explicit StaticHandler(std::filesystem::path static_dir)
        : static_dir_{std::move(static_dir)} {
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                    const std::string& /*remote_ip*/, Send&& send);

private:
    template <typename Body, typename Allocator, typename Send>
    void HandleStaticRequest(std::string_view target,
                             http::request<Body, http::basic_fields<Allocator>>& req,
                             Send& send);

    static std::string UrlDecode(std::string_view encoded);
    static std::string GetMimeType(const std::string& extension);
    static http::response<http::string_body> MakePlainResponse(http::status status, std::string_view text,
                                                               unsigned version, bool keep_alive);
    static bool IsSubPath(const std::filesystem::path& path, const std::filesystem::path& base);

    std::filesystem::path static_dir_;
};

// Обработчик API запросов с защитой через strand
class ApiHandler : public std::enable_shared_from_this<ApiHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    ApiHandler(model::Game& game, Strand api_strand)
        : game_{game}
        , api_strand_{std::move(api_strand)} {
    }

    ApiHandler(const ApiHandler&) = delete;
    ApiHandler& operator=(const ApiHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                    const std::string& /*remote_ip*/, Send&& send);

private:
    template <typename Body, typename Allocator, typename Send>
    void HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                          std::string_view target,
                          unsigned version, bool keep_alive, Send&& send);

    template <typename Body, typename Allocator, typename Send>
    void HandleMapsRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                           std::string_view target,
                           unsigned version, bool keep_alive, Send&& send);

    template <typename Body, typename Allocator, typename Send>
    void HandleJoinRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                           unsigned version, bool keep_alive, Send&& send);

    template <typename Body, typename Allocator, typename Send>
    void HandleStateRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                            unsigned version, bool keep_alive, Send&& send);

    template <typename Body, typename Allocator, typename Send>
    void HandlePlayersRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                              unsigned version, bool keep_alive, Send&& send);

    template <typename Body, typename Allocator, typename Send>
    void HandleActionRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                             unsigned version, bool keep_alive, Send&& send);

    // Вспомогательные функции для авторизации
    template <typename Body, typename Allocator>
    static std::optional<model::Token> ExtractToken(const http::request<Body, http::basic_fields<Allocator>>& req);

    // Для операций только для чтения (const Player& и const GameSession&)
    template <typename Body, typename Allocator, typename Send, typename Fn>
    void ExecuteAuthorized(const http::request<Body, http::basic_fields<Allocator>>& req,
                           unsigned version, bool keep_alive, Send&& send, Fn&& action) const;

    // Для операций записи (Player& и GameSession&)
    template <typename Body, typename Allocator, typename Send, typename Fn>
    void ExecuteAuthorizedMutable(const http::request<Body, http::basic_fields<Allocator>>& req,
                                  unsigned version, bool keep_alive, Send&& send, Fn&& action);

    static http::response<http::string_body> MakeJsonResponse(http::status status, std::string_view body,
                                                              unsigned version, bool keep_alive);

    static http::response<http::string_body> MakeErrorResponse(http::status status, std::string_view message,
                                                               unsigned version, bool keep_alive,
                                                               std::string_view code = "badRequest",
                                                               bool add_cache_control = false);

    // Сериализация списка карт (id и name)
    static std::string SerializeMapsList(const model::Game::Maps& maps);

    // Сериализация полной информации о карте
    static std::string SerializeMap(const model::Map& map);

    // Вспомогательные функции сериализации подобъектов
    static boost::json::array SerializeRoads(const model::Map::Roads& roads);
    static boost::json::array SerializeBuildings(const model::Map::Buildings& buildings);
    static boost::json::array SerializeOffices(const model::Map::Offices& offices);

    model::Game& game_;
    Strand api_strand_;
};

// Декоратор для логирования запросов и ответов
template <typename Handler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(Handler&& handler) : handler_(std::move(handler)) {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                    const std::string& remote_ip,
                    Send&& send) {
        auto start = std::chrono::steady_clock::now();
        LogRequest(req, remote_ip);

        auto wrapped_send = [this, start, remote_ip, send = std::forward<Send>(send)](
                                auto&& response) mutable {
            LogResponse(response, start, remote_ip);
            send(std::forward<decltype(response)>(response));
        };

        handler_(std::move(req), remote_ip, std::move(wrapped_send));
    }

private:
    void LogRequest(const auto& req, const std::string& remote_ip) {
        boost::json::object data;
        data["ip"] = remote_ip;
        data["URI"] = std::string(req.target());
        data["method"] = std::string(req.method_string());
        logging::LogMessage("request received", data);
    }

    void LogResponse(const auto& res, std::chrono::steady_clock::time_point start,
                     const std::string& remote_ip) {
        using namespace std::chrono;
        auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start).count();

        boost::json::object data;
        data["ip"] = remote_ip;
        data["response_time"] = elapsed;
        data["code"] = res.result_int();

        auto it = res.find(http::field::content_type);
        if (it != res.end()) {
            data["content_type"] = std::string(it->value());
        } else {
            data["content_type"] = nullptr;   // null в JSON
        }

        logging::LogMessage("response sent", data);
    }

    Handler handler_;
};

}  // namespace http_handler

// Включаем реализацию шаблонных методов
#include "request_handler_impl.h"
