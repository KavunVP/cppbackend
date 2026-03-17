#pragma once
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

// Метка для токена игрока
namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class Game;

class Dog {
public:
    using Id = util::Tagged<size_t, struct DogTag>;

    explicit Dog(Id id) noexcept
        : id_{std::move(id)} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

private:
    Id id_;
};

class GameSession {
public:
    using Dogs = std::vector<Dog>;
    using Id = util::Tagged<size_t, struct GameSessionTag>;

    GameSession() = default;
    GameSession(Id id, const Map* map) noexcept
        : id_{std::move(id)}
        , map_{map} {
    }

    GameSession(GameSession&&) = default;
    GameSession& operator=(GameSession&&) = default;
    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

    const Id& GetId() const noexcept {
        return id_;
    }

    const Map* GetMap() const noexcept {
        return map_;
    }

    const Dogs& GetDogs() const noexcept {
        return dogs_;
    }

    Dog& AddDog(Dog dog) {
        dogs_.emplace_back(std::move(dog));
        return dogs_.back();
    }

private:
    Id id_;
    const Map* map_;
    Dogs dogs_;
};

class Player {
public:
    using Id = util::Tagged<size_t, struct PlayerTag>;

    Player(Id id, std::string name, Dog* dog, GameSession* session) noexcept
        : id_{std::move(id)}
        , name_{std::move(name)}
        , dog_{dog}
        , session_{session} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Dog* GetDog() const noexcept {
        return dog_;
    }

    const GameSession* GetSession() const noexcept {
        return session_;
    }

private:
    Id id_;
    std::string name_;
    Dog* dog_;
    GameSession* session_;
};

class PlayerTokens {
public:
    PlayerTokens() {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        std::random_device rd;
        generator1_ = std::make_unique<std::mt19937_64>(dist(rd));
        generator2_ = std::make_unique<std::mt19937_64>(dist(rd));
    }

    PlayerTokens(PlayerTokens&&) = default;
    PlayerTokens& operator=(PlayerTokens&&) = default;
    PlayerTokens(const PlayerTokens&) = delete;
    PlayerTokens& operator=(const PlayerTokens&) = delete;

    Token GenerateToken() {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        auto part1 = dist(*generator1_);
        auto part2 = dist(*generator2_);
        return Token{ToHexString(part1) + ToHexString(part2)};
    }

    void AddPlayer(const Token& token, Player* player) {
        token_to_player_.emplace(*token, player);
    }

    Player* FindPlayerByToken(const Token& token) const {
        auto it = token_to_player_.find(*token);
        if (it != token_to_player_.end()) {
            return it->second;
        }
        return nullptr;
    }

private:
    static std::string ToHexString(uint64_t value) {
        static const char hex_digits[] = "0123456789abcdef";
        std::string result;
        result.reserve(16);
        for (int i = 15; i >= 0; --i) {
            result.push_back(hex_digits[(value >> (i * 4)) & 0xF]);
        }
        return result;
    }

    std::unique_ptr<std::mt19937_64> generator1_;
    std::unique_ptr<std::mt19937_64> generator2_;
    std::unordered_map<std::string, Player*> token_to_player_;
};

class Players {
public:
    Players() = default;
    Players(Players&&) = default;
    Players& operator=(Players&&) = default;
    Players(const Players&) = delete;
    Players& operator=(const Players&) = delete;

    Player& Add(Dog* dog, GameSession* session, std::string name) {
        static size_t next_id = 0;
        auto id = Player::Id{next_id++};
        players_.emplace_back(std::make_unique<Player>(id, std::move(name), dog, session));
        return *players_.back();
    }

    Player* FindByDogIdAndMapId(const Dog::Id& dog_id, const Map::Id& map_id) const {
        for (const auto& player : players_) {
            if (player->GetDog()->GetId() == dog_id &&
                player->GetSession()->GetMap()->GetId() == map_id) {
                return player.get();
            }
        }
        return nullptr;
    }

    const std::vector<std::unique_ptr<Player>>& GetPlayers() const noexcept {
        return players_;
    }

private:
    std::vector<std::unique_ptr<Player>> players_;
};

class Game {
public:
    using Maps = std::vector<Map>;
    using Sessions = std::vector<GameSession>;

    Game() = default;
    Game(Game&&) = default;
    Game& operator=(Game&&) = default;
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    GameSession& AddSession(const Map* map) {
        static size_t next_session_id = 0;
        auto id = GameSession::Id{next_session_id++};
        sessions_.emplace_back(id, map);
        return sessions_.back();
    }

    GameSession* FindSession(GameSession::Id id) noexcept {
        for (auto& session : sessions_) {
            if (session.GetId() == id) {
                return &session;
            }
        }
        return nullptr;
    }

    const Sessions& GetSessions() const noexcept {
        return sessions_;
    }

    // Найти или создать сессию для карты
    GameSession& GetOrCreateSession(const Map* map) {
        // Ищем существующую сессию для этой карты
        for (auto& session : sessions_) {
            if (session.GetMap() == map) {
                return session;
            }
        }
        // Не нашли — создаём новую
        return AddSession(map);
    }

    Players& GetPlayers() noexcept {
        return players_;
    }

    const Players& GetPlayers() const noexcept {
        return players_;
    }

    PlayerTokens& GetPlayerTokens() noexcept {
        return player_tokens_;
    }

    const PlayerTokens& GetPlayerTokens() const noexcept {
        return player_tokens_;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    Sessions sessions_;
    Players players_;
    PlayerTokens player_tokens_;
};

}  // namespace model
