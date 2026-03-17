#pragma once
#include "request_handler.h"
#include <cctype>
#include <unordered_map>

namespace http_handler {

// ============================================================================
// StaticHandler implementation
// ============================================================================

template <typename Body, typename Allocator, typename Send>
void StaticHandler::operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                               const std::string& /*remote_ip*/, Send&& send) {
    std::string_view target = req.target();
    auto pos = target.find('?');
    if (pos != std::string_view::npos) {
        target = target.substr(0, pos);
    }
    HandleStaticRequest(target, req, send);
}

template <typename Body, typename Allocator, typename Send>
void StaticHandler::HandleStaticRequest(std::string_view target,
                                        http::request<Body, http::basic_fields<Allocator>>& req,
                                        Send& send) {
    std::string decoded_target;
    try {
        decoded_target = UrlDecode(target);
    } catch (const std::exception&) {
        return send(MakePlainResponse(http::status::bad_request, "Invalid URL encoding",
                                      req.version(), req.keep_alive()));
    }

    std::filesystem::path req_path = decoded_target;
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

    if (!IsSubPath(full_path, static_dir_)) {
        return send(MakePlainResponse(http::status::bad_request, "Path is outside static directory",
                                      req.version(), req.keep_alive()));
    }

    if (std::filesystem::is_directory(full_path, ec)) {
        full_path /= "index.html";
    }

    if (!std::filesystem::is_regular_file(full_path, ec) || ec) {
        return send(MakePlainResponse(http::status::not_found, "File not found",
                                      req.version(), req.keep_alive()));
    }

    std::string content_type = GetMimeType(full_path.extension().string());

    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open()) {
        return send(MakePlainResponse(http::status::internal_server_error, "Cannot open file",
                                      req.version(), req.keep_alive()));
    }
    std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    if (req.method() == http::verb::head) {
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

inline std::string StaticHandler::UrlDecode(std::string_view encoded) {
    std::string res;
    res.reserve(encoded.size());
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '+') {
            res += ' ';
        } else if (encoded[i] == '%' && i + 2 < encoded.size()) {
            int hex_val = 0;
            for (int j = 1; j <= 2; ++j) {
                char c = encoded[i + j];
                int digit;
                if (c >= '0' && c <= '9') digit = c - '0';
                else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
                else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
                else throw std::runtime_error("Invalid hex digit");
                hex_val = hex_val * 16 + digit;
            }
            res += static_cast<char>(hex_val);
            i += 2;
        } else {
            res += encoded[i];
        }
    }
    return res;
}

inline std::string StaticHandler::GetMimeType(const std::string& extension) {
    static const std::unordered_map<std::string, std::string> mime_map = {
        {".htm", "text/html"},
        {".html", "text/html"},
        {".css", "text/css"},
        {".txt", "text/plain"},
        {".js", "text/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpe", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"},
        {".tiff", "image/tiff"},
        {".tif", "image/tiff"},
        {".svg", "image/svg+xml"},
        {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"},
    };
    std::string ext_lower;
    for (char ch : extension) {
        ext_lower += std::tolower(static_cast<unsigned char>(ch));
    }
    auto it = mime_map.find(ext_lower);
    if (it != mime_map.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

inline http::response<http::string_body> StaticHandler::MakePlainResponse(http::status status, std::string_view text,
                                                                          unsigned version, bool keep_alive) {
    http::response<http::string_body> res(status, version);
    res.set(http::field::content_type, "text/plain");
    res.body() = text;
    res.prepare_payload();
    res.keep_alive(keep_alive);
    return res;
}

inline bool StaticHandler::IsSubPath(const std::filesystem::path& path, const std::filesystem::path& base) {
    std::error_code ec;
    auto path_canon = std::filesystem::weakly_canonical(path, ec);
    if (ec) return false;
    auto base_canon = std::filesystem::weakly_canonical(base, ec);
    if (ec) return false;

    for (auto b = base_canon.begin(), p = path_canon.begin(); b != base_canon.end(); ++b, ++p) {
        if (p == path_canon.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// ApiHandler implementation
// ============================================================================

template <typename Body, typename Allocator, typename Send>
void ApiHandler::operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                            const std::string& /*remote_ip*/, Send&& send) {
    auto version = req.version();
    auto keep_alive = req.keep_alive();

    try {
        // Проверяем, что это API запрос
        std::string_view target = req.target();
        auto pos = target.find('?');
        if (pos != std::string_view::npos) {
            target = target.substr(0, pos);
        }

        if (!target.starts_with("/api/")) {
            // Это не API запрос, возвращаем ошибку
            return send(MakeErrorResponse(http::status::bad_request, "Bad request",
                                          version, keep_alive));
        }

        // Выполняем обработку внутри strand
        auto handle = [self = shared_from_this(), send = std::forward<Send>(send),
                       req = std::forward<decltype(req)>(req), version, keep_alive,
                       target = std::string(target)]() mutable {
            try {
                assert(self->api_strand_.running_in_this_thread());
                return self->HandleApiRequest(std::move(req), target, version, keep_alive,
                                              std::move(send));
            } catch (...) {
                return send(self->MakeErrorResponse(http::status::internal_server_error,
                                                    "Internal server error", version, keep_alive));
            }
        };

        net::dispatch(api_strand_, std::move(handle));
    } catch (...) {
        send(MakeErrorResponse(http::status::internal_server_error, "Internal server error",
                               version, keep_alive));
    }
}

template <typename Body, typename Allocator, typename Send>
void ApiHandler::HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                                  std::string_view target,
                                  unsigned version, bool keep_alive, Send&& send) {
    // /api/v1/game/join - POST (проверяем раньше, чем maps, чтобы избежать ложных срабатываний)
    if (target == "/api/v1/game/join") {
        return HandleJoinRequest(std::move(req), version, keep_alive, std::move(send));
    }

    // /api/v1/game/players - GET
    if (target == "/api/v1/game/players") {
        return HandlePlayersRequest(std::move(req), version, keep_alive, std::move(send));
    }

    // /api/v1/maps и /api/v1/maps/{id}
    if (target.starts_with("/api/v1/maps")) {
        return HandleMapsRequest(std::move(req), target, version, keep_alive, std::move(send));
    }

    // Неизвестный API endpoint
    return send(MakeErrorResponse(http::status::bad_request, "Bad request",
                                  version, keep_alive));
}

template <typename Body, typename Allocator, typename Send>
void ApiHandler::HandleMapsRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                                   std::string_view target,
                                   unsigned version, bool keep_alive, Send&& send) {
    // Проверяем метод - только GET для maps
    if (req.method() != http::verb::get) {
        return send(MakeErrorResponse(http::status::method_not_allowed, "Method not allowed",
                                      version, keep_alive));
    }

    constexpr std::string_view api_prefix = "/api/v1/maps";

    if (target == api_prefix) {
        // Список карт
        std::string body = SerializeMapsList(game_.GetMaps());
        return send(MakeJsonResponse(http::status::ok, body, version, keep_alive));
    }

    // Проверка наличия / после префикса и id
    if (target.size() <= api_prefix.size() + 1 || target[api_prefix.size()] != '/') {
        return send(MakeErrorResponse(http::status::bad_request, "Bad request",
                                      version, keep_alive));
    }

    std::string_view map_id = target.substr(api_prefix.size() + 1);
    const auto* map = game_.FindMap(model::Map::Id(std::string(map_id)));
    if (!map) {
        return send(MakeErrorResponse(http::status::not_found, "Map not found",
                                      version, keep_alive, "mapNotFound"));
    }

    // Полная информация о карте
    std::string body = SerializeMap(*map);
    return send(MakeJsonResponse(http::status::ok, body, version, keep_alive));
}

template <typename Body, typename Allocator, typename Send>
void ApiHandler::HandleJoinRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                                   unsigned version, bool keep_alive, Send&& send) {
    // Проверяем метод
    if (req.method() != http::verb::post) {
        auto res = MakeErrorResponse(http::status::method_not_allowed,
                                     "Only POST method is expected", version, keep_alive,
                                     "invalidMethod", true);
        res.set(http::field::allow, "POST");
        return send(std::move(res));
    }

    // Проверяем Content-Type
    auto content_type_it = req.find(http::field::content_type);
    if (content_type_it == req.end() ||
        content_type_it->value().find("application/json") == std::string_view::npos) {
        return send(MakeErrorResponse(http::status::bad_request, "Invalid content type",
                                      version, keep_alive, "invalidArgument", true));
    }

    // Парсим JSON
    boost::json::value json_value;
    try {
        json_value = boost::json::parse(req.body());
    } catch (...) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "Join game request parse error",
                                      version, keep_alive, "invalidArgument", true));
    }

    const auto* obj = json_value.if_object();
    if (!obj) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "Join game request parse error",
                                      version, keep_alive, "invalidArgument", true));
    }

    // Получаем userName
    auto user_name_it = obj->find("userName");
    if (user_name_it == obj->end() || !user_name_it->value().is_string()) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "Join game request parse error",
                                      version, keep_alive, "invalidArgument", true));
    }
    std::string user_name = user_name_it->value().as_string().c_str();

    // Проверяем имя на пустоту
    if (user_name.empty()) {
        return send(MakeErrorResponse(http::status::bad_request, "Invalid name",
                                      version, keep_alive, "invalidArgument", true));
    }

    // Получаем mapId
    auto map_id_it = obj->find("mapId");
    if (map_id_it == obj->end() || !map_id_it->value().is_string()) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "Join game request parse error",
                                      version, keep_alive, "invalidArgument", true));
    }
    std::string map_id = map_id_it->value().as_string().c_str();

    // Ищем карту
    const auto* map = game_.FindMap(model::Map::Id(map_id));
    if (!map) {
        return send(MakeErrorResponse(http::status::not_found, "Map not found",
                                      version, keep_alive, "mapNotFound", true));
    }

    // Получаем или создаём сессию для этой карты (одна сессия на карту)
    auto& session = game_.GetOrCreateSession(map);

    // Создаём собаку
    static size_t next_dog_id = 0;
    auto dog_id = model::Dog::Id{next_dog_id++};
    model::Dog dog{dog_id};
    auto& session_dog = session.AddDog(std::move(dog));

    // Создаём игрока
    auto& player = game_.GetPlayers().Add(&session_dog, &session, user_name);

    // Генерируем токен
    auto token = game_.GetPlayerTokens().GenerateToken();
    game_.GetPlayerTokens().AddPlayer(token, &player);

    // Формируем ответ
    boost::json::object response;
    response["authToken"] = *token;
    response["playerId"] = *player.GetId();

    std::string body = boost::json::serialize(response);
    auto res = MakeJsonResponse(http::status::ok, body, version, keep_alive);
    res.set(http::field::cache_control, "no-cache");
    return send(std::move(res));
}

template <typename Body, typename Allocator, typename Send>
void ApiHandler::HandlePlayersRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                                      unsigned version, bool keep_alive, Send&& send) {
    // Проверяем метод
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "Invalid method",
                                     version, keep_alive, "invalidMethod", true);
        res.set(http::field::allow, "GET, HEAD");
        return send(std::move(res));
    }

    // Получаем токен из заголовка Authorization
    auto auth_it = req.find(http::field::authorization);
    if (auth_it == req.end()) {
        return send(MakeErrorResponse(http::status::unauthorized,
                                      "Authorization header is missing",
                                      version, keep_alive, "invalidToken", true));
    }

    std::string_view auth_value = auth_it->value();
    // Ожидаем формат "Bearer <token>"
    constexpr std::string_view bearer_prefix = "Bearer ";
    if (!auth_value.starts_with(bearer_prefix) || auth_value.size() != bearer_prefix.size() + 32) {
        return send(MakeErrorResponse(http::status::unauthorized,
                                      "Invalid authorization format",
                                      version, keep_alive, "invalidToken", true));
    }

    std::string token_str = std::string(auth_value.substr(bearer_prefix.size()));
    model::Token token{std::move(token_str)};

    // Ищем игрока по токену
    const auto* player = game_.GetPlayerTokens().FindPlayerByToken(token);
    if (!player) {
        return send(MakeErrorResponse(http::status::unauthorized,
                                      "Player token has not been found",
                                      version, keep_alive, "unknownToken", true));
    }

    // Получаем сессию игрока
    const auto* session = player->GetSession();
    if (!session) {
        return send(MakeErrorResponse(http::status::unauthorized,
                                      "Player has no session",
                                      version, keep_alive, "unknownToken", true));
    }

    // Получаем всех игроков в этой сессии
    boost::json::object players_obj;
    const auto& all_players = game_.GetPlayers().GetPlayers();
    for (const auto& p : all_players) {
        if (p->GetSession() == session) {
            boost::json::object player_obj;
            player_obj["name"] = p->GetName();
            players_obj[std::to_string(*p->GetId())] = std::move(player_obj);
        }
    }

    std::string body = boost::json::serialize(players_obj);
    auto res = MakeJsonResponse(http::status::ok, body, version, keep_alive);
    res.set(http::field::cache_control, "no-cache");

    if (req.method() == http::verb::head) {
        res.body().clear();
        res.prepare_payload();
    }

    return send(std::move(res));
}

inline http::response<http::string_body> ApiHandler::MakeJsonResponse(http::status status, std::string_view body,
                                                                      unsigned version, bool keep_alive) {
    http::response<http::string_body> res(status, version);
    res.set(http::field::content_type, "application/json");
    res.body() = body;
    res.content_length(body.size());
    res.keep_alive(keep_alive);
    return res;
}

inline http::response<http::string_body> ApiHandler::MakeErrorResponse(http::status status, std::string_view message,
                                                                       unsigned version, bool keep_alive,
                                                                       std::string_view code,
                                                                       bool add_cache_control) {
    boost::json::object obj;
    obj["code"] = boost::json::string_view(code.data(), code.size());
    obj["message"] = boost::json::string_view(message.data(), message.size());
    std::string body = boost::json::serialize(obj);
    auto res = MakeJsonResponse(status, body, version, keep_alive);
    if (add_cache_control) {
        res.set(http::field::cache_control, "no-cache");
    }
    return res;
}

inline std::string ApiHandler::SerializeMapsList(const model::Game::Maps& maps) {
    boost::json::array arr;
    for (const auto& map : maps) {
        boost::json::object obj;
        obj["id"] = *map.GetId();
        obj["name"] = map.GetName();
        arr.push_back(std::move(obj));
    }
    return boost::json::serialize(arr);
}

inline std::string ApiHandler::SerializeMap(const model::Map& map) {
    boost::json::object obj;
    obj["id"] = *map.GetId();
    obj["name"] = map.GetName();
    obj["roads"] = SerializeRoads(map.GetRoads());
    obj["buildings"] = SerializeBuildings(map.GetBuildings());
    obj["offices"] = SerializeOffices(map.GetOffices());
    return boost::json::serialize(obj);
}

inline boost::json::array ApiHandler::SerializeRoads(const model::Map::Roads& roads) {
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

inline boost::json::array ApiHandler::SerializeBuildings(const model::Map::Buildings& buildings) {
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

inline boost::json::array ApiHandler::SerializeOffices(const model::Map::Offices& offices) {
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
