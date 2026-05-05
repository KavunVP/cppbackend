#pragma once
#include "request_handler.h"
#include "record_service.h"
#include <cctype>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include "constants.h"

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

        if (!target.starts_with(API_PREFIX)) {
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
            } catch (const std::exception&) {
                return send(self->MakeErrorResponse(http::status::internal_server_error,
                                                    "Internal server error", version, keep_alive));
            }
        };

        net::dispatch(api_strand_, std::move(handle));
    } catch (const std::exception&) {
        send(MakeErrorResponse(http::status::internal_server_error, "Internal server error",
                               version, keep_alive));
    }
}

template <typename Body, typename Allocator, typename Send>
void ApiHandler::HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                                  std::string_view target,
                                  unsigned version, bool keep_alive, Send&& send) {
    // /api/v1/game/join - POST (проверяем раньше, чем maps, чтобы избежать ложных срабатываний)
    if (target == GAME_JOIN) {
        return HandleJoinRequest(std::move(req), version, keep_alive, std::move(send));
    }

    // /api/v1/game/tick - POST
    if (target == GAME_TICK) {
        return HandleTickRequest(std::move(req), version, keep_alive, std::move(send));
    }

    // /api/v1/game/player/action - POST
    if (target == GAME_PLAYER_ACTION) {
        return HandlePlayerActionRequest(std::move(req), version, keep_alive, std::move(send));
    }

    // /api/v1/game/state - GET
    if (target == GAME_STATE) {
        return HandleStateRequest(std::move(req), version, keep_alive, std::move(send));
    }

    // /api/v1/game/players - GET
    if (target == GAME_PLAYERS) {
        return HandlePlayersRequest(std::move(req), version, keep_alive, std::move(send));
    }

    // /api/v1/game/records - GET
    if (target == GAME_RECORDS) {
        return HandleRecordsRequest(std::move(req), version, keep_alive, std::move(send));
    }

    // /api/v1/maps и /api/v1/maps/{id}
    if (target.starts_with(API_V1_MAPS)) {
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
    // Проверяем метод - только GET и HEAD для maps
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "Method not allowed",
                                      version, keep_alive, "invalidMethod", true);
        res.set(http::field::allow, "GET, HEAD");
        return send(std::move(res));
    }

    if (target == API_V1_MAPS) {
        // Список карт
        std::string body = SerializeMapsList(game_.GetMaps());
        auto res = MakeJsonResponse(http::status::ok, body, version, keep_alive);
        res.set(http::field::cache_control, "no-cache");
        if (req.method() == http::verb::head) {
            res.body().clear();
            res.prepare_payload();
        }
        return send(std::move(res));
    }

    // Проверка наличия / после префикса и id
    if (target.size() <= API_V1_MAPS.size() + 1 || target[API_V1_MAPS.size()] != '/') {
        return send(MakeErrorResponse(http::status::bad_request, "Bad request",
                                      version, keep_alive, "badRequest", true));
    }

    std::string_view map_id = target.substr(API_V1_MAPS.size() + 1);
    const auto* map = game_.FindMap(model::Map::Id(std::string(map_id)));
    if (!map) {
        return send(MakeErrorResponse(http::status::not_found, "Map not found",
                                      version, keep_alive, "mapNotFound", true));
    }

    // Полная информация о карте
    std::string body = SerializeMap(*map);
    auto res = MakeJsonResponse(http::status::ok, body, version, keep_alive);
    res.set(http::field::cache_control, "no-cache");
    if (req.method() == http::verb::head) {
        res.body().clear();
        res.prepare_payload();
    }
    return send(std::move(res));
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
    } catch (const std::exception&) {
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

    std::optional<model::Dog> dog;
    if (randomize_spawn_points_) {
        // Создаём собаку в случайной точке дороги
        std::random_device rd;
        std::mt19937_64 gen(rd());
        dog = model::CreateDogWithRandomPosition(*map, dog_id, gen);
    } else {
        // Создаём собаку в начале первой дороги карты
        dog = model::CreateDogAtStart(*map, dog_id);
    }

    auto& session_dog = session.AddDog(std::move(*dog));

    // Создаём игрока с вместимостью рюкзака из карты
    auto& player = game_.GetPlayers().Add(&session_dog, &session, user_name, map->GetBagCapacity());

    // Устанавливаем время входа в игру (текущее игровое время сессии)
    player.SetJoinTimeMs(session.GetGameTime());

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
    if (!auth_value.starts_with(bearer_prefix) || auth_value.size() <= bearer_prefix.size()) {
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

std::string ApiHandler::SerializeMap(const model::Map& map) {
    boost::json::object obj;
    obj["id"] = *map.GetId();
    obj["name"] = map.GetName();
    obj["roads"] = SerializeRoads(map.GetRoads());
    obj["buildings"] = SerializeBuildings(map.GetBuildings());
    obj["offices"] = SerializeOffices(map.GetOffices());

    // Добавляем lootTypes из extra_data
    if (const auto* loot_types = extra_data_.GetLootTypes(map.GetId())) {
        obj["lootTypes"] = *loot_types;
    }

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

template <typename Body, typename Allocator, typename Send>
void ApiHandler::HandleStateRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
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
                                      "Authorization header is required",
                                      version, keep_alive, "invalidToken", true));
    }

    std::string_view auth_value = auth_it->value();
    // Ожидаем формат "Bearer <token>"
    constexpr std::string_view bearer_prefix = "Bearer ";
    if (!auth_value.starts_with(bearer_prefix) || auth_value.size() <= bearer_prefix.size()) {
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

    // Проверяем, вышел ли игрок на покой
    if (player->IsRetired()) {
        return send(MakeErrorResponse(http::status::unauthorized,
                                      "Player has retired",
                                      version, keep_alive, "unknownToken", true));
    }

    // Получаем сессию игрока
    const auto* session = player->GetSession();
    if (!session) {
        return send(MakeErrorResponse(http::status::unauthorized,
                                      "Player has no session",
                                      version, keep_alive, "unknownToken", true));
    }

    // Сериализуем состояние всех игроков в сессии
    boost::json::object players_obj;
    const auto& all_players = game_.GetPlayers().GetPlayers();
    for (const auto& p : all_players) {
        if (p->GetSession() == session) {
            const auto* dog = p->GetDog();
            if (!dog) continue;

            boost::json::object player_obj;
            
            // pos - массив координат
            boost::json::array pos_arr;
            pos_arr.push_back(dog->GetPosition().x);
            pos_arr.push_back(dog->GetPosition().y);
            player_obj["pos"] = std::move(pos_arr);
            
            // speed - массив скоростей
            boost::json::array speed_arr;
            speed_arr.push_back(dog->GetSpeed().dx);
            speed_arr.push_back(dog->GetSpeed().dy);
            player_obj["speed"] = std::move(speed_arr);
            
            // dir - направление
            std::string dir_str;
            switch (dog->GetDirection()) {
                case model::Direction::NORTH: dir_str = "U"; break;
                case model::Direction::SOUTH: dir_str = "D"; break;
                case model::Direction::WEST:  dir_str = "L"; break;
                case model::Direction::EAST:  dir_str = "R"; break;
            }
            player_obj["dir"] = dir_str;

            // Добавляем содержимое рюкзака
            boost::json::array bag_arr;
            for (const auto& item : p->GetBag()) {
                boost::json::object bag_item;
                bag_item["id"] = static_cast<int64_t>(item.id);
                bag_item["type"] = static_cast<int64_t>(item.type);
                bag_arr.push_back(std::move(bag_item));
            }
            player_obj["bag"] = std::move(bag_arr);

            // Добавляем счёт игрока
            player_obj["score"] = static_cast<int64_t>(p->GetScore());

            players_obj[std::to_string(*p->GetId())] = std::move(player_obj);
        }
    }

    boost::json::object response;
    response["players"] = std::move(players_obj);

    // Добавляем lostObjects из сессии
    boost::json::object lost_objects_obj;
    const auto& lost_objects = session->GetLostObjects();
    for (const auto& lost_obj : lost_objects) {
        boost::json::object obj;
        obj["type"] = static_cast<int64_t>(lost_obj.GetType());
        boost::json::array pos_arr;
        pos_arr.push_back(lost_obj.GetPosition().x);
        pos_arr.push_back(lost_obj.GetPosition().y);
        obj["pos"] = std::move(pos_arr);
        lost_objects_obj[std::to_string(*lost_obj.GetId())] = std::move(obj);
    }
    response["lostObjects"] = std::move(lost_objects_obj);

    std::string body = boost::json::serialize(response);
    
    auto res = MakeJsonResponse(http::status::ok, body, version, keep_alive);
    res.set(http::field::cache_control, "no-cache");

    if (req.method() == http::verb::head) {
        res.body().clear();
        res.prepare_payload();
    }

    return send(std::move(res));
}

// ============================================================================
// UpdateGameState implementation
// ============================================================================

// Ширины объектов
constexpr double PLAYER_WIDTH = 0.6;
constexpr double ITEM_WIDTH = 0.0;
constexpr double BASE_WIDTH = 0.5;

// Радиусы коллизий
constexpr double PLAYER_ITEM_COLLECT_RADIUS = (PLAYER_WIDTH + ITEM_WIDTH) / 2.0;   // 0.3
constexpr double PLAYER_BASE_COLLECT_RADIUS = (PLAYER_WIDTH + BASE_WIDTH) / 2.0;   // 0.55

// Провайдер для поиска коллизий при сборе предметов
class ItemCollectionProvider : public collision_detector::ItemGathererProvider {
public:
    ItemCollectionProvider(const model::GameSession::LostObjects& lost_objects,
                           const std::vector<geom::Point2D>& start_positions,
                           const std::vector<geom::Point2D>& end_positions)
        : lost_objects_(lost_objects)
        , start_positions_(start_positions)
        , end_positions_(end_positions) {
    }

    size_t ItemsCount() const override { return lost_objects_.size(); }

    collision_detector::Item GetItem(size_t idx) const override {
        const auto& obj = lost_objects_[idx];
        return {geom::Point2D{obj.GetPosition().x, obj.GetPosition().y}, ITEM_WIDTH};
    }

    size_t GatherersCount() const override { return start_positions_.size(); }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return {start_positions_[idx], end_positions_[idx], PLAYER_WIDTH};
    }

private:
    const model::GameSession::LostObjects& lost_objects_;
    const std::vector<geom::Point2D>& start_positions_;
    const std::vector<geom::Point2D>& end_positions_;
};

// Провайдер для поиска коллизий при возврате на базу
class BaseReturnProvider : public collision_detector::ItemGathererProvider {
public:
    BaseReturnProvider(const model::Map::Offices& offices,
                       const std::vector<geom::Point2D>& start_positions,
                       const std::vector<geom::Point2D>& end_positions)
        : offices_(offices)
        , start_positions_(start_positions)
        , end_positions_(end_positions) {
    }

    size_t ItemsCount() const override { return offices_.size(); }

    collision_detector::Item GetItem(size_t idx) const override {
        const auto& office = offices_[idx];
        auto pos = office.GetPosition();
        return {geom::Point2D{static_cast<double>(pos.x), static_cast<double>(pos.y)}, BASE_WIDTH};
    }

    size_t GatherersCount() const override { return start_positions_.size(); }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return {start_positions_[idx], end_positions_[idx], PLAYER_WIDTH};
    }

private:
    const model::Map::Offices& offices_;
    const std::vector<geom::Point2D>& start_positions_;
    const std::vector<geom::Point2D>& end_positions_;
};

// Тип события сбора
enum class CollisionEventType { ItemPickup, BaseReturn };

struct UnifiedCollisionEvent {
    CollisionEventType type;
    size_t item_idx;      // индекс в lost_objects (для ItemPickup) или offices (для BaseReturn)
    size_t gatherer_idx;  // индекс игрока в массиве перемещений
    double time;          // доля пройденного пути (0..1)
    double sq_distance;   // квадрат расстояния до точки
};

void ApiHandler::UpdateGameState(uint64_t time_delta_ms) {
    // time_delta_ms в миллисекундах, переводим в секунды
    double dt = static_cast<double>(time_delta_ms) / MILLISECONDS_PER_SECOND;

    // Проходим по всем сессиям
    auto& sessions = game_.GetSessions();
    for (size_t i = 0; i < sessions.size(); ++i) {
        auto& session = sessions[i];
        const auto* map = session.GetMap();
        if (!map) continue;

        // Генерация трофеев
        session.Tick(time_delta_ms);

        // Сохраняем начальные позиции собак и обновляем их позиции
        auto& dogs = session.GetDogs();
        std::vector<geom::Point2D> start_positions;
        std::vector<geom::Point2D> end_positions;
        start_positions.reserve(dogs.size());
        end_positions.reserve(dogs.size());

        for (auto& dog : dogs) {
            start_positions.emplace_back(dog.GetPosition().x, dog.GetPosition().y);
            UpdateDogPosition(dog, *map, dt);
            end_positions.emplace_back(dog.GetPosition().x, dog.GetPosition().y);
        }

        // Ищем игроков для каждой собаки
        const auto& all_players = game_.GetPlayers().GetPlayers();
        std::vector<model::Player*> players_for_dogs;
        players_for_dogs.reserve(dogs.size());
        for (const auto& dog : dogs) {
            model::Player* player = nullptr;
            for (const auto& p : all_players) {
                if (p->GetDog() == &dog) {
                    player = p.get();
                    break;
                }
            }
            players_for_dogs.push_back(player);
        }

        // Если нет перемещений или нет игроков, пропускаем коллизии
        if (start_positions.empty()) {
            session.AddGameTime(time_delta_ms);
            continue;
        }

        // Находим события сбора предметов
        std::vector<UnifiedCollisionEvent> all_events;

        // События сбора предметов
        {
            ItemCollectionProvider provider{session.GetLostObjects(), start_positions, end_positions};
            auto gather_events = collision_detector::FindGatherEvents(provider);

            for (const auto& evt : gather_events) {
                all_events.push_back({
                    CollisionEventType::ItemPickup,
                    evt.item_id,
                    evt.gatherer_id,
                    evt.time,
                    evt.sq_distance
                });
            }
        }

        // События возврата на базу
        {
            BaseReturnProvider provider{map->GetOffices(), start_positions, end_positions};
            auto base_events = collision_detector::FindGatherEvents(provider);

            for (const auto& evt : base_events) {
                all_events.push_back({
                    CollisionEventType::BaseReturn,
                    evt.item_id,
                    evt.gatherer_id,
                    evt.time,
                    evt.sq_distance
                });
            }
        }

        // Сортируем события по времени
        std::sort(all_events.begin(), all_events.end(),
                  [](const UnifiedCollisionEvent& a, const UnifiedCollisionEvent& b) {
                      return a.time < b.time;
                  });

        // Отслеживаем, какие предметы уже собраны
        const auto& lost_objects = session.GetLostObjects();
        std::vector<bool> item_collected(lost_objects.size(), false);

        // Обрабатываем события в хронологическом порядке
        for (const auto& event : all_events) {
            auto* player = players_for_dogs[event.gatherer_idx];
            if (!player) continue;

            if (event.type == CollisionEventType::ItemPickup) {
                // Если предмет уже собран другим игроком — пропускаем
                if (item_collected[event.item_idx]) continue;

                // Если рюкзак полон — пропускаем предмет
                if (player->IsBagFull()) continue;

                // Собираем предмет
                const auto& lost_obj = lost_objects[event.item_idx];
                player->AddToBag(model::BagItem{*lost_obj.GetId(), lost_obj.GetType()});
                item_collected[event.item_idx] = true;
            } else {
                // BaseReturn — сдаём предметы на базу
                const auto& bag = player->GetBag();
                if (!bag.empty()) {
                    // Получаем значения трофеев из карты
                    const auto* map = session.GetMap();
                    const auto& loot_values = map ? map->GetLootValues() : std::vector<unsigned>{};

                    // Начисляем очки за каждый предмет в рюкзаке
                    unsigned total_score = 0;
                    for (const auto& item : bag) {
                        if (item.type < loot_values.size()) {
                            total_score += loot_values[item.type];
                        }
                    }
                    player->AddScore(total_score);

                    player->ClearBag();
                }
            }
        }

        // Удаляем собранные предметы из сессии
        model::GameSession::LostObjects new_lost;
        for (size_t j = 0; j < lost_objects.size(); ++j) {
            if (!item_collected[j]) {
                new_lost.push_back(std::move(const_cast<model::LostObject&>(lost_objects[j])));
            }
        }
        session.GetLostObjects() = std::move(new_lost);

        // Обновляем игровое время
        session.AddGameTime(time_delta_ms);

        // Проверяем бездействие игроков и удаляем вышедших на покой
        auto retirement_time = game_.GetDogRetirementTime();
        std::vector<size_t> retired_player_indices;
        
        for (size_t dog_idx = 0; dog_idx < dogs.size(); ++dog_idx) {
            auto* player = players_for_dogs[dog_idx];
            if (!player) continue;
            if (player->IsRetired()) continue;  // Уже вышел на покой
            
            const auto& dog = dogs[dog_idx];
            const auto& speed = dog.GetSpeed();
            
            // Проверяем, движется ли собака
            if (speed.dx == 0.0 && speed.dy == 0.0) {
                // Собака не движется - увеличиваем время бездействия
                player->AddIdleTime(time_delta_ms);
                
                // Проверяем, превысило ли время бездействия лимит
                if (player->GetIdleTimeMs() >= static_cast<uint64_t>(retirement_time.count())) {
                    // Игрок выходит на покой
                    player->SetRetired(true);
                    retired_player_indices.push_back(dog_idx);
                }
            } else {
                // Собака движется - сбрасываем время бездействия
                player->ResetIdleTime();
            }
        }
        
        // Записываем рекорды для вышедших на покой игроков
        if (record_service_ && !retired_player_indices.empty()) {
            for (size_t dog_idx : retired_player_indices) {
                auto* player = players_for_dogs[dog_idx];
                if (!player) continue;
                
                // playTime = время от входа в игру до текущего момента
                // Включает время бездействия
                uint64_t current_game_time = session.GetGameTime();
                uint64_t play_time_ms = current_game_time - player->GetJoinTimeMs();
                
                record_service_->AddRecord(player->GetName(), player->GetScore(),
                                           std::chrono::milliseconds{play_time_ms});
            }
        }
    }
}

void ApiHandler::UpdateDogPosition(model::Dog& dog, const model::Map& map, double dt) {
    const auto& speed = dog.GetSpeed();

    // Если скорость нулевая, ничего не делаем
    if (speed.dx == 0.0 && speed.dy == 0.0) {
        return;
    }

    auto pos = dog.GetPosition();
    double new_x = pos.x + speed.dx * dt;
    double new_y = pos.y + speed.dy * dt;

    // Проверяем, может ли собака переместиться в новую точку
    // Для этого проверяем, находится ли новая точка на дороге
    const model::Road* new_road = map.FindRoadAt(new_x, new_y);

    if (new_road) {
        // Точка на дороге - перемещаем
        dog.SetPosition({new_x, new_y});
    } else {
        // Проверяем, находится ли текущая позиция на дороге
        const model::Road* current_road = map.FindRoadAt(pos.x, pos.y);
        if (current_road) {
            // Текущая позиция на дороге, но новая - нет
            // ClampDogToRoad сам решит, останавливать собаку или нет
            ClampDogToRoad(dog, map, dt);
        } else {
            // Собака не на дороге (не должно происходить)
            dog.SetSpeed({0.0, 0.0});
        }
    }
}

inline ApiHandler::RoadBounds ApiHandler::CalculateRoadBounds(
    double min_x, double max_x,
    double min_y, double max_y,
    double half_width) {

    RoadBounds roadBounds;
    roadBounds.min_x = min_x - half_width;
    roadBounds.max_x = max_x + half_width;
    roadBounds.min_y = min_y - half_width;
    roadBounds.max_y = max_y + half_width;
    return roadBounds;
}

void ApiHandler::ClampDogToRoad(model::Dog& dog, const model::Map& map, double dt) {
    const auto& speed = dog.GetSpeed();
    auto pos = dog.GetPosition();

    // If speed is zero, nothing to do
    if (speed.dx == 0.0 && speed.dy == 0.0) {
        return;
    }

    // Calculate target position
    double target_x = pos.x + speed.dx * dt;
    double target_y = pos.y + speed.dy * dt;

    constexpr double ROAD_HALF_WIDTH = 0.4;

    // Find all roads the dog is currently on
    std::vector<const model::Road*> start_roads;
    for (const auto& road : map.GetRoads()) {
        auto start = road.GetStart();
        auto end = road.GetEnd();
        double base_min_x, base_max_x, base_min_y, base_max_y;

        if (road.IsHorizontal()) {
            base_min_x = std::min(start.x, end.x);
            base_max_x = std::max(start.x, end.x);
            base_min_y = base_max_y = static_cast<double>(start.y);
        } else {
            base_min_y = std::min(start.y, end.y);
            base_max_y = std::max(start.y, end.y);
            base_min_x = base_max_x = static_cast<double>(start.x);
        }

        auto bounds = CalculateRoadBounds(base_min_x, base_max_x,
                                          base_min_y, base_max_y,
                                          ROAD_HALF_WIDTH);

        if (pos.x >= bounds.min_x && pos.x <= bounds.max_x &&
            pos.y >= bounds.min_y && pos.y <= bounds.max_y) {
            start_roads.push_back(&road);
        }
    }

    // If dog is not on any road, stop it
    if (start_roads.empty()) {
        dog.SetSpeed({0.0, 0.0});
        return;
    }

    // For each road, calculate the clamped position and choose the best one
    double best_dist = -1.0;
    double best_x = pos.x;
    double best_y = pos.y;
    const model::Road* best_road = nullptr;

    for (const auto* road : start_roads) {
        auto start = road->GetStart();
        auto end = road->GetEnd();

        double base_min_x, base_max_x, base_min_y, base_max_y;

        if (road->IsHorizontal()) {
            base_min_x = std::min(start.x, end.x);
            base_max_x = std::max(start.x, end.x);
            base_min_y = base_max_y = static_cast<double>(start.y);
        } else {
            base_min_y = std::min(start.y, end.y);
            base_max_y = std::max(start.y, end.y);
            base_min_x = base_max_x = static_cast<double>(start.x);
        }

        auto bounds = CalculateRoadBounds(base_min_x, base_max_x,
                                          base_min_y, base_max_y,
                                          ROAD_HALF_WIDTH);

        double new_x = std::clamp(target_x, bounds.min_x, bounds.max_x);
        double new_y = std::clamp(target_y, bounds.min_y, bounds.max_y);

        double dist = std::sqrt((new_x - pos.x) * (new_x - pos.x) +
                                (new_y - pos.y) * (new_y - pos.y));

        if (dist > best_dist) {
            best_dist = dist;
            best_x = new_x;
            best_y = new_y;
            best_road = road;
        }
    }

    // Check if the target position was clamped on ANY axis.
    // If clamped, check if there is a road ahead in the movement direction.
    // If no road ahead, stop the dog.
    if (best_road) {
        bool x_clamped = (std::abs(best_x - target_x) > 0.001);
        bool y_clamped = (std::abs(best_y - target_y) > 0.001);

        // If any coordinate was clamped, check for road ahead
        if (x_clamped || y_clamped) {
            double step = ROAD_HALF_WIDTH * 0.5;
            double check_x = best_x + speed.dx * step;
            double check_y = best_y + speed.dy * step;

            bool has_road_ahead = false;
            for (const auto& road : map.GetRoads()) {
                auto r_start = road.GetStart();
                auto r_end = road.GetEnd();
                if (road.IsHorizontal()) {
                    double r_min_x = std::min(r_start.x, r_end.x) - ROAD_HALF_WIDTH;
                    double r_max_x = std::max(r_start.x, r_end.x) + ROAD_HALF_WIDTH;
                    double r_road_y = static_cast<double>(r_start.y);
                    if (std::abs(check_y - r_road_y) <= ROAD_HALF_WIDTH &&
                        check_x >= r_min_x && check_x <= r_max_x) {
                        has_road_ahead = true;
                        break;
                    }
                } else {
                    double r_min_y = std::min(r_start.y, r_end.y) - ROAD_HALF_WIDTH;
                    double r_max_y = std::max(r_start.y, r_end.y) + ROAD_HALF_WIDTH;
                    double r_road_x = static_cast<double>(r_start.x);
                    if (std::abs(check_x - r_road_x) <= ROAD_HALF_WIDTH &&
                        check_y >= r_min_y && check_y <= r_max_y) {
                        has_road_ahead = true;
                        break;
                    }
                }
            }
            if (!has_road_ahead) {
                dog.SetSpeed({0.0, 0.0});
            }
        }
    } else {
        dog.SetSpeed({0.0, 0.0});
    }

    dog.SetPosition({best_x, best_y});
}

template <typename Body, typename Allocator, typename Send>
void ApiHandler::HandlePlayerActionRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
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

    // Получаем токен из заголовка Authorization
    auto auth_it = req.find(http::field::authorization);
    if (auth_it == req.end()) {
        return send(MakeErrorResponse(http::status::unauthorized,
                                      "Authorization header is missing",
                                      version, keep_alive, "invalidToken", true));
    }

    std::string_view auth_value = auth_it->value();
    constexpr std::string_view bearer_prefix = "Bearer ";
    if (!auth_value.starts_with(bearer_prefix) || auth_value.size() <= bearer_prefix.size()) {
        return send(MakeErrorResponse(http::status::unauthorized,
                                      "Invalid authorization format",
                                      version, keep_alive, "invalidToken", true));
    }

    std::string token_str = std::string(auth_value.substr(bearer_prefix.size()));
    model::Token token{std::move(token_str)};

    // Ищем игрока по токену
    auto* player = game_.GetPlayerTokens().FindPlayerByToken(token);
    if (!player) {
        return send(MakeErrorResponse(http::status::unauthorized,
                                      "Player token has not been found",
                                      version, keep_alive, "invalidToken", true));
    }

    // Парсим JSON
    boost::json::value json_value;
    try {
        json_value = boost::json::parse(req.body());
    } catch (const std::exception&) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "Failed to parse request JSON",
                                      version, keep_alive, "invalidArgument", true));
    }

    const auto* obj = json_value.if_object();
    if (!obj) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "Request body must be a JSON object",
                                      version, keep_alive, "invalidArgument", true));
    }

    // Получаем направление движения
    auto move_it = obj->find("move");
    if (move_it == obj->end() || !move_it->value().is_string()) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "Missing or invalid 'move' field",
                                      version, keep_alive, "invalidArgument", true));
    }

    std::string move_dir = move_it->value().as_string().c_str();
    
    // Обработка пустой команды движения - собака останавливается
    if (move_dir.empty()) {
        auto* dog = const_cast<model::Dog*>(player->GetDog());
        if (!dog) {
            return send(MakeErrorResponse(http::status::bad_request,
                                          "Player has no dog",
                                          version, keep_alive, "invalidArgument", true));
        }
        dog->SetSpeed({0.0, 0.0});
        // Не сбрасываем время бездействия - оно продолжит накапливаться
        
        // Возвращаем пустой JSON-объект
        auto res = MakeJsonResponse(http::status::ok, "{}", version, keep_alive);
        res.set(http::field::cache_control, "no-cache");
        return send(std::move(res));
    }
    
    if (move_dir.size() != 1) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "Invalid move direction",
                                      version, keep_alive, "invalidArgument", true));
    }

    // Преобразуем направление
    model::Direction direction;
    char dir_char = move_dir[0];
    switch (dir_char) {
        case 'U': direction = model::Direction::NORTH; break;
        case 'D': direction = model::Direction::SOUTH; break;
        case 'L': direction = model::Direction::WEST;  break;
        case 'R': direction = model::Direction::EAST;  break;
        default:
            return send(MakeErrorResponse(http::status::bad_request,
                                          "Invalid move direction. Must be U, D, L, or R",
                                          version, keep_alive, "invalidArgument", true));
    }

    // Получаем собаку игрока и устанавливаем направление
    auto* dog = const_cast<model::Dog*>(player->GetDog());
    if (!dog) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "Player has no dog",
                                      version, keep_alive, "invalidArgument", true));
    }

    // Устанавливаем направление и скорость
    dog->SetDirection(direction);
    
    // Вычисляем скорость на основе направления
    // Скорость собаки берётся из карты
    const auto* session = player->GetSession();
    const auto* map = session ? session->GetMap() : nullptr;
    double dog_speed = map ? map->GetDogSpeed() : 0.0;
    
    // Устанавливаем скорость в зависимости от направления
    // В нашей системе координат: Y растёт вниз, X растёт вправо
    // U (NORTH) - вверх = отрицательный Y
    // D (SOUTH) - вниз = положительный Y  
    // L (WEST) - влево = отрицательный X
    // R (EAST) - вправо = положительный X
    model::Speed new_speed{0.0, 0.0};
    switch (direction) {
        case model::Direction::NORTH: new_speed = {0.0, -dog_speed}; break;
        case model::Direction::SOUTH: new_speed = {0.0, dog_speed}; break;
        case model::Direction::WEST:  new_speed = {-dog_speed, 0.0}; break;
        case model::Direction::EAST:  new_speed = {dog_speed, 0.0}; break;
    }
    dog->SetSpeed(new_speed);

    // Сбрасываем время бездействия при получении команды движения
    player->ResetIdleTime();

    // Возвращаем пустой JSON-объект
    auto res = MakeJsonResponse(http::status::ok, "{}", version, keep_alive);
    res.set(http::field::cache_control, "no-cache");
    return send(std::move(res));
}

template <typename Body, typename Allocator, typename Send>
void ApiHandler::HandleTickRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                                   unsigned version, bool keep_alive, Send&& send) {
    // Если сервер запущен с --tick-period, отклоняем запросы к /api/v1/game/tick
    if (tick_period_.has_value()) {
        return send(MakeErrorResponse(http::status::bad_request, "Invalid endpoint",
                                      version, keep_alive, "badRequest", true));
    }

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
    } catch (const std::exception&) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "Failed to parse tick request JSON",
                                      version, keep_alive, "invalidArgument", true));
    }

    const auto* obj = json_value.if_object();
    if (!obj) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "Tick request body must be a JSON object",
                                      version, keep_alive, "invalidArgument", true));
    }

    // Получаем timeDelta
    auto time_delta_it = obj->find("timeDelta");
    if (time_delta_it == obj->end()) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "Missing timeDelta field",
                                      version, keep_alive, "invalidArgument", true));
    }

    // Проверяем, что timeDelta - целое число
    if (!time_delta_it->value().is_int64() && !time_delta_it->value().is_uint64()) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "timeDelta must be an integer",
                                      version, keep_alive, "invalidArgument", true));
    }

    int64_t time_delta = 0;
    if (time_delta_it->value().is_int64()) {
        time_delta = time_delta_it->value().as_int64();
    } else {
        time_delta = static_cast<int64_t>(time_delta_it->value().as_uint64());
    }

    // Проверяем, что timeDelta неотрицательное
    if (time_delta < 0) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "timeDelta must be non-negative",
                                      version, keep_alive, "invalidArgument", true));
    }

    // Обновляем состояние игры
    auto delta_ms = std::chrono::milliseconds{static_cast<uint64_t>(time_delta)};
    UpdateGameState(static_cast<uint64_t>(time_delta));
    tick_signal_(delta_ms);

    // Возвращаем пустой JSON-объект
    auto res = MakeJsonResponse(http::status::ok, "{}", version, keep_alive);
    res.set(http::field::cache_control, "no-cache");
    return send(std::move(res));
}

// Публичный метод Tick для вызова из Ticker
inline void ApiHandler::Tick(std::chrono::milliseconds delta) {
    UpdateGameState(static_cast<uint64_t>(delta.count()));
    tick_signal_(delta);
}

template <typename Body, typename Allocator, typename Send>
void ApiHandler::HandleRecordsRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                                      unsigned version, bool keep_alive, Send&& send) {
    // Проверяем метод - только GET и HEAD
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "Invalid method",
                                     version, keep_alive, "invalidMethod", true);
        res.set(http::field::allow, "GET, HEAD");
        return send(std::move(res));
    }

    // Проверяем, есть ли сервис записей
    if (!record_service_) {
        return send(MakeErrorResponse(http::status::internal_server_error,
                                      "Record service not available",
                                      version, keep_alive, "internalError", true));
    }

    // Парсим параметры пагинации из URL
    std::string_view target = req.target();
    size_t start = 0;
    std::optional<size_t> max_items;

    auto pos = target.find('?');
    if (pos != std::string_view::npos) {
        std::string_view query = target.substr(pos + 1);
        
        // Парсим start
        auto start_pos = query.find("start=");
        if (start_pos != std::string_view::npos) {
            auto end_pos = query.find('&', start_pos);
            std::string start_str;
            if (end_pos != std::string_view::npos) {
                start_str = std::string(query.substr(start_pos + 6, end_pos - start_pos - 6));
            } else {
                start_str = std::string(query.substr(start_pos + 6));
            }
            try {
                start = std::stoul(start_str);
            } catch (const std::exception&) {
                return send(MakeErrorResponse(http::status::bad_request,
                                              "Invalid start parameter",
                                              version, keep_alive, "invalidArgument", true));
            }
        }

        // Парсим maxItems
        auto max_pos = query.find("maxItems=");
        if (max_pos != std::string_view::npos) {
            auto end_pos = query.find('&', max_pos);
            std::string max_str;
            if (end_pos != std::string_view::npos) {
                max_str = std::string(query.substr(max_pos + 9, end_pos - max_pos - 9));
            } else {
                max_str = std::string(query.substr(max_pos + 9));
            }
            try {
                max_items = std::stoul(max_str);
            } catch (const std::exception&) {
                return send(MakeErrorResponse(http::status::bad_request,
                                              "Invalid maxItems parameter",
                                              version, keep_alive, "invalidArgument", true));
            }
        }
    }

    // Проверяем maxItems на ограничение 100
    if (max_items.has_value() && max_items.value() > 100) {
        return send(MakeErrorResponse(http::status::bad_request,
                                      "maxItems cannot exceed 100",
                                      version, keep_alive, "invalidArgument", true));
    }

    // Получаем записи
    try {
        auto records = record_service_->GetRecords(start, max_items);

        // Сериализуем в JSON
        boost::json::array arr;
        for (const auto& record : records) {
            boost::json::object obj;
            obj["name"] = record.name;
            obj["score"] = record.score;
            obj["playTime"] = record.play_time;
            arr.push_back(std::move(obj));
        }

        std::string body = boost::json::serialize(arr);
        auto res = MakeJsonResponse(http::status::ok, body, version, keep_alive);
        res.set(http::field::cache_control, "no-cache");

        if (req.method() == http::verb::head) {
            res.body().clear();
            res.prepare_payload();
        }

        return send(std::move(res));
    } catch (const std::exception& e) {
        return send(MakeErrorResponse(http::status::internal_server_error,
                                      std::string("Database error: ") + e.what(),
                                      version, keep_alive, "internalError", true));
    }
}

}  // namespace http_handler