#pragma once
#include "http_server.h"
#include "model.h"
#include "extra_data.h"
#include "logging.h"
#include "collision_detector.h"
#include <boost/json.hpp>
#include <boost/signals2.hpp>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cassert>
#include <memory>
#include <optional>

class RecordService;  // Forward declaration

namespace http_handler
{
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace net = boost::asio;

    // Обработчик статических файлов
    class StaticHandler
    {
    public:
        explicit StaticHandler(std::filesystem::path static_dir)
            : static_dir_{std::move(static_dir)}
        {
        }

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>> &&req,
                        const std::string & /*remote_ip*/, Send &&send);

    private:
        template <typename Body, typename Allocator, typename Send>
        void HandleStaticRequest(std::string_view target,
                                 http::request<Body, http::basic_fields<Allocator>> &req,
                                 Send &send);

        static std::string UrlDecode(std::string_view encoded);
        static std::string GetMimeType(const std::string &extension);
        static http::response<http::string_body> MakePlainResponse(http::status status, std::string_view text,
                                                                   unsigned version, bool keep_alive);
        static bool IsSubPath(const std::filesystem::path &path, const std::filesystem::path &base);

        std::filesystem::path static_dir_;
    };

    // Обработчик API запросов с защитой через strand
    class ApiHandler : public std::enable_shared_from_this<ApiHandler>
    {
    public:
        using Strand = net::strand<net::io_context::executor_type>;
        using TickSignal = boost::signals2::signal<void(std::chrono::milliseconds)>;

        ApiHandler(model::Game &game, const extra_data::MapExtraData &extra_data, Strand api_strand,
                   std::optional<std::chrono::milliseconds> tick_period,
                   bool randomize_spawn_points,
                   RecordService* record_service = nullptr)
            : game_{game}, extra_data_{extra_data}, api_strand_{std::move(api_strand)}, tick_period_{tick_period}, randomize_spawn_points_{randomize_spawn_points}, record_service_{record_service}
        {
        }

        ApiHandler(const ApiHandler &) = delete;
        ApiHandler &operator=(const ApiHandler &) = delete;

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>> &&req,
                        const std::string & /*remote_ip*/, Send &&send);

        // Публичный метод для обновления времени из Ticker
        void Tick(std::chrono::milliseconds delta);

        // Сигнал, вызываемый при каждом тике (для подписчиков: сериализация и т.д.)
        TickSignal& GetTickSignal() noexcept { return tick_signal_; }
        const TickSignal& GetTickSignal() const noexcept { return tick_signal_; }

    private:

        inline static constexpr std::string_view API_PREFIX = "/api/";
        inline static constexpr std::string_view API_V1_MAPS = "/api/v1/maps";
        inline static constexpr std::string_view GAME_JOIN = "/api/v1/game/join";
        inline static constexpr std::string_view GAME_TICK = "/api/v1/game/tick";
        inline static constexpr std::string_view GAME_PLAYER_ACTION = "/api/v1/game/player/action";
        inline static constexpr std::string_view GAME_STATE = "/api/v1/game/state";
        inline static constexpr std::string_view GAME_PLAYERS = "/api/v1/game/players";
        inline static constexpr std::string_view GAME_RECORDS = "/api/v1/game/records";

        template <typename Body, typename Allocator, typename Send>
        void HandleApiRequest(http::request<Body, http::basic_fields<Allocator>> &&req,
                              std::string_view target,
                              unsigned version, bool keep_alive, Send &&send);

        template <typename Body, typename Allocator, typename Send>
        void HandleMapsRequest(http::request<Body, http::basic_fields<Allocator>> &&req,
                               std::string_view target,
                               unsigned version, bool keep_alive, Send &&send);

        template <typename Body, typename Allocator, typename Send>
        void HandleJoinRequest(http::request<Body, http::basic_fields<Allocator>> &&req,
                               unsigned version, bool keep_alive, Send &&send);

        template <typename Body, typename Allocator, typename Send>
        void HandleTickRequest(http::request<Body, http::basic_fields<Allocator>> &&req,
                               unsigned version, bool keep_alive, Send &&send);

        template <typename Body, typename Allocator, typename Send>
        void HandlePlayerActionRequest(http::request<Body, http::basic_fields<Allocator>> &&req,
                                       unsigned version, bool keep_alive, Send &&send);

        template <typename Body, typename Allocator, typename Send>
        void HandleStateRequest(http::request<Body, http::basic_fields<Allocator>> &&req,
                                unsigned version, bool keep_alive, Send &&send);

        template <typename Body, typename Allocator, typename Send>
        void HandlePlayersRequest(http::request<Body, http::basic_fields<Allocator>> &&req,
                                  unsigned version, bool keep_alive, Send &&send);

        template <typename Body, typename Allocator, typename Send>
        void HandleRecordsRequest(http::request<Body, http::basic_fields<Allocator>> &&req,
                                  unsigned version, bool keep_alive, Send &&send);

        static http::response<http::string_body> MakeJsonResponse(http::status status, std::string_view body,
                                                                  unsigned version, bool keep_alive);

        static http::response<http::string_body> MakeErrorResponse(http::status status, std::string_view message,
                                                                   unsigned version, bool keep_alive,
                                                                   std::string_view code = "badRequest",
                                                                   bool add_cache_control = false);

        // Сериализация списка карт (id и name)
        static std::string SerializeMapsList(const model::Game::Maps &maps);

        // Сериализация полной информации о карте
        std::string SerializeMap(const model::Map &map);

        // Вспомогательные функции сериализации подобъектов
        static boost::json::array SerializeRoads(const model::Map::Roads &roads);
        static boost::json::array SerializeBuildings(const model::Map::Buildings &buildings);
        static boost::json::array SerializeOffices(const model::Map::Offices &offices);

        // Обновление состояния игры
        void UpdateGameState(uint64_t time_delta_ms);

        // Обновление позиции собаки
        static void UpdateDogPosition(model::Dog &dog, const model::Map &map, double dt);
        static void ClampDogToRoad(model::Dog &dog, const model::Map &map, double dt);

        // Структура для хранения границ дороги
        struct RoadBounds
        {
            double min_x, max_x, min_y, max_y;
        };

        // Вычисление границ дороги с учётом ширины
        static RoadBounds CalculateRoadBounds(double min_x, double max_x,
                                              double min_y, double max_y,
                                              double half_width);

        model::Game &game_;
        const extra_data::MapExtraData &extra_data_;
        Strand api_strand_;
        std::optional<std::chrono::milliseconds> tick_period_;
        bool randomize_spawn_points_;
        TickSignal tick_signal_;
        RecordService* record_service_;  // Не владеет, только указатель
    };

    // Декоратор для логирования запросов и ответов
    template <typename Handler>
    class LoggingRequestHandler
    {
    public:
        explicit LoggingRequestHandler(Handler &&handler) : handler_(std::move(handler)) {}

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>> &&req,
                        const std::string &remote_ip,
                        Send &&send)
        {
            auto start = std::chrono::steady_clock::now();
            LogRequest(req, remote_ip);

            auto wrapped_send = [this, start, remote_ip, send = std::forward<Send>(send)](
                                    auto &&response) mutable
            {
                LogResponse(response, start, remote_ip);
                send(std::forward<decltype(response)>(response));
            };

            handler_(std::move(req), remote_ip, std::move(wrapped_send));
        }

    private:
        void LogRequest(const auto &req, const std::string &remote_ip)
        {
            boost::json::object data;
            data["ip"] = remote_ip;
            data["URI"] = std::string(req.target());
            data["method"] = std::string(req.method_string());
            logging::LogMessage("request received", data);
        }

        void LogResponse(const auto &res, std::chrono::steady_clock::time_point start,
                         const std::string &remote_ip)
        {
            using namespace std::chrono;
            auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start).count();

            boost::json::object data;
            data["ip"] = remote_ip;
            data["response_time"] = elapsed;
            data["code"] = res.result_int();

            auto it = res.find(http::field::content_type);
            if (it != res.end())
            {
                data["content_type"] = std::string(it->value());
            }
            else
            {
                data["content_type"] = nullptr; // null в JSON
            }

            logging::LogMessage("response sent", data);
        }

        Handler handler_;
    };

} // namespace http_handler

// Включаем реализацию шаблонных методов
#include "request_handler_impl.h"