#pragma once
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "tagged.h"
#include "loot_generator.h"

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

// Вещественные координаты
struct Position {
    double x, y;
};

// Вещественная скорость
struct Speed {
    double dx, dy;
};

// Направление движения
enum class Direction {
    NORTH,  // U - вверх
    SOUTH,  // D - вниз
    WEST,   // L - влево
    EAST    // R - вправо
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

    double GetDogSpeed() const noexcept {
        return dog_speed_;
    }

    void SetDogSpeed(double speed) noexcept {
        dog_speed_ = speed;
    }

    unsigned GetLootTypeCount() const noexcept {
        return loot_type_count_;
    }

    void SetLootTypeCount(unsigned count) noexcept {
        loot_type_count_ = count;
    }

    unsigned GetBagCapacity() const noexcept {
        return bag_capacity_;
    }

    void SetBagCapacity(unsigned capacity) noexcept {
        bag_capacity_ = capacity;
    }

    const std::vector<unsigned>& GetLootValues() const noexcept {
        return loot_values_;
    }

    void SetLootValues(std::vector<unsigned> values) noexcept {
        loot_values_ = std::move(values);
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

    // Найти дорогу, содержащую точку (x, y)
    // Возвращает nullptr, если точка не на дороге
    const Road* FindRoadAt(double x, double y) const noexcept;

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    double dog_speed_ = 0.0;  // Скорость собаки для этой карты
    unsigned loot_type_count_ = 0;  // Количество типов трофеев
    unsigned bag_capacity_ = 3;  // Вместимость рюкзака (по умолчанию 3)
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    std::vector<unsigned> loot_values_;  // value для каждого типа трофея (по индексу)
};

class Game;

// Предмет в рюкзаке игрока
struct BagItem {
    size_t id;
    unsigned type;
};

// Потерянный предмет на карте
class LostObject {
public:
    using Id = util::Tagged<size_t, struct LostObjectTag>;

    LostObject(Id id, unsigned type, Position pos) noexcept
        : id_{std::move(id)}
        , type_{type}
        , position_{pos} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    unsigned GetType() const noexcept {
        return type_;
    }

    const Position& GetPosition() const noexcept {
        return position_;
    }

private:
    Id id_;
    unsigned type_;
    Position position_;
};

class Dog {
public:
    using Id = util::Tagged<size_t, struct DogTag>;

    Dog(Id id, Position pos, Speed speed, Direction dir) noexcept
        : id_{std::move(id)}
        , position_{pos}
        , speed_{speed}
        , direction_{dir} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const Position& GetPosition() const noexcept {
        return position_;
    }

    const Speed& GetSpeed() const noexcept {
        return speed_;
    }

    Direction GetDirection() const noexcept {
        return direction_;
    }

    void SetPosition(Position pos) noexcept {
        position_ = pos;
    }

    void SetSpeed(Speed speed) noexcept {
        speed_ = speed;
    }

    void SetDirection(Direction dir) noexcept {
        direction_ = dir;
    }

private:
    Id id_;
    Position position_;
    Speed speed_;
    Direction direction_;
};

class GameSession {
public:
    using Dogs = std::deque<Dog>;
    using LostObjects = std::deque<LostObject>;
    using Id = util::Tagged<size_t, struct GameSessionTag>;

    GameSession() = default;
    GameSession(Id id, const Map* map,
                std::chrono::milliseconds loot_gen_period, double loot_gen_probability,
                std::mt19937_64 loot_rng) noexcept
        : id_{std::move(id)}
        , map_{map}
        , game_time_{0}
        , loot_generator_{loot_gen_period, loot_gen_probability}
        , loot_rng_{std::move(loot_rng)}
        , loot_type_dist_{0, map ? map->GetLootTypeCount() - 1 : 0u}
    {
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

    Dogs& GetDogs() noexcept {
        return dogs_;
    }

    Dog& AddDog(Dog dog) {
        dogs_.emplace_back(std::move(dog));
        return dogs_.back();
    }

    const LostObjects& GetLostObjects() const noexcept {
        return lost_objects_;
    }

    LostObjects& GetLostObjects() noexcept {
        return lost_objects_;
    }

    LostObject& AddLostObject(LostObject obj) {
        lost_objects_.emplace_back(std::move(obj));
        return lost_objects_.back();
    }

    // Игровое время в миллисекундах
    uint64_t GetGameTime() const noexcept {
        return game_time_;
    }

    void AddGameTime(uint64_t delta_ms) noexcept {
        game_time_ += delta_ms;
    }

    // Генерация трофеев за прошедшее время
    void Tick(uint64_t delta_ms);

private:
    Id id_;
    const Map* map_;
    Dogs dogs_;
    uint64_t game_time_;  // Игровое время в миллисекундах
    loot_gen::LootGenerator loot_generator_;
    LostObjects lost_objects_;
    std::mt19937_64 loot_rng_;
    std::uniform_real_distribution<double> loot_uniform_dist_{0.0, 1.0};
    std::uniform_int_distribution<unsigned> loot_type_dist_;
    std::uniform_real_distribution<double> road_pos_dist_{0.0, 1.0};
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

    const std::vector<BagItem>& GetBag() const noexcept {
        return bag_;
    }

    std::vector<BagItem>& GetBag() noexcept {
        return bag_;
    }

    void ClearBag() {
        bag_.clear();
    }

    bool IsBagFull() const noexcept {
        return bag_.size() >= bag_capacity_;
    }

    unsigned GetBagCapacity() const noexcept {
        return bag_capacity_;
    }

    void SetBagCapacity(unsigned capacity) noexcept {
        bag_capacity_ = capacity;
    }

    void AddToBag(BagItem item) {
        bag_.push_back(std::move(item));
    }

    unsigned GetScore() const noexcept {
        return score_;
    }

    void AddScore(unsigned points) noexcept {
        score_ += points;
    }

private:
    Id id_;
    std::string name_;
    Dog* dog_;
    GameSession* session_;
    std::vector<BagItem> bag_;
    unsigned bag_capacity_ = 3;
    unsigned score_ = 0;
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

    Player& Add(Dog* dog, GameSession* session, std::string name, unsigned bag_capacity = 3) {
        static size_t next_id = 0;
        auto id = Player::Id{next_id++};
        players_.emplace_back(std::make_unique<Player>(id, std::move(name), dog, session));
        players_.back()->SetBagCapacity(bag_capacity);
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

    // Установить конфигурацию генератора трофеев
    void SetLootGeneratorConfig(std::chrono::milliseconds period, double probability) {
        loot_gen_period_ = period;
        loot_gen_probability_ = probability;
    }

    std::chrono::milliseconds GetLootGenPeriod() const noexcept {
        return loot_gen_period_;
    }

    double GetLootGenProbability() const noexcept {
        return loot_gen_probability_;
    }

    // Создать генератор случайных чисел для сессии
    std::mt19937_64 CreateSessionRng() {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        std::random_device rd;
        return std::mt19937_64{rd()};
    }

    GameSession& AddSession(const Map* map) {
        static size_t next_session_id = 0;
        auto id = GameSession::Id{next_session_id++};
        auto rng = CreateSessionRng();
        sessions_.emplace_back(id, map, loot_gen_period_, loot_gen_probability_, std::move(rng));
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

    Sessions& GetSessions() noexcept {
        return sessions_;
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

    unsigned GetDefaultBagCapacity() const noexcept {
        return default_bag_capacity_;
    }

    void SetDefaultBagCapacity(unsigned capacity) noexcept {
        default_bag_capacity_ = capacity;
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
    std::chrono::milliseconds loot_gen_period_{5000};  // По умолчанию 5 секунд
    double loot_gen_probability_ = 0.5;  // По умолчанию 50%
    unsigned default_bag_capacity_ = 3;  // Вместимость рюкзака по умолчанию
};

// Вспомогательные функции для работы с собаками
inline Position GetRandomPositionOnRoad(const Map& map, std::mt19937_64& gen) {
    const auto& roads = map.GetRoads();
    if (roads.empty()) {
        return {0.0, 0.0};
    }

    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    const auto& road = roads[road_dist(gen)];

    auto start = road.GetStart();
    auto end = road.GetEnd();

    std::uniform_real_distribution<double> pos_dist(0.0, 1.0);
    double t = pos_dist(gen);

    double x = start.x + t * (end.x - start.x);
    double y = start.y + t * (end.y - start.y);

    return {x, y};
}

// Получить позицию в начале первой дороги
inline Position GetStartPositionOnRoad(const Map& map) {
    const auto& roads = map.GetRoads();
    if (roads.empty()) {
        return {0.0, 0.0};
    }

    const auto& road = roads[0];
    auto start = road.GetStart();
    return {static_cast<double>(start.x), static_cast<double>(start.y)};
}

inline Dog CreateDogWithRandomPosition(const Map& map, Dog::Id id, std::mt19937_64& gen) {
    Position pos = GetRandomPositionOnRoad(map, gen);
    Speed speed{0.0, 0.0};
    Direction dir = Direction::NORTH;
    return Dog(id, pos, speed, dir);
}

// Создать собаку в начале первой дороги карты
inline Dog CreateDogAtStart(const Map& map, Dog::Id id) {
    Position pos = GetStartPositionOnRoad(map);
    Speed speed{0.0, 0.0};
    Direction dir = Direction::NORTH;
    return Dog(id, pos, speed, dir);
}

}  // namespace model
