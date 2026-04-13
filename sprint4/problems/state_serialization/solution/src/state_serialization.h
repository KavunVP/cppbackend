#pragma once

#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/access.hpp>

#include <string>
#include <vector>

#include "geom.h"
#include "model.h"

// Free serialize функции для geom (ADL lookup)
namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& p, const unsigned /*version*/) {
    ar& p.x;
    ar& p.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& v, const unsigned /*version*/) {
    ar& v.x;
    ar& v.y;
}

}  // namespace geom

namespace serialization {

// Представление собаки для сериализации
struct DogRepr {
    size_t id = 0;
    geom::Point2D position;
    geom::Vec2D speed;
    model::Direction direction = model::Direction::NORTH;

    explicit DogRepr(const model::Dog& dog)
        : id(*dog.GetId())
        , position(dog.GetPosition().x, dog.GetPosition().y)
        , speed(dog.GetSpeed().dx, dog.GetSpeed().dy)
        , direction(dog.GetDirection()) {
    }

    DogRepr() = default;

    [[nodiscard]] model::Dog Restore() const {
        return model::Dog{
            model::Dog::Id{id},
            model::Position{position.x, position.y},
            model::Speed{speed.x, speed.y},
            direction};
    }

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar& id;
        ar& position;
        ar& speed;
        ar& direction;
    }
};

// Предмет в рюкзаке
struct BagItemRepr {
    size_t id = 0;
    unsigned type = 0;

    explicit BagItemRepr(const model::BagItem& item)
        : id(item.id), type(item.type) {
    }

    BagItemRepr() = default;

    [[nodiscard]] model::BagItem Restore() const {
        return model::BagItem{id, type};
    }

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar& id;
        ar& type;
    }
};

// Представление потерянного предмета
struct LostObjectRepr {
    size_t id = 0;
    unsigned type = 0;
    geom::Point2D position;

    explicit LostObjectRepr(const model::LostObject& obj)
        : id(*obj.GetId())
        , type(obj.GetType())
        , position(obj.GetPosition().x, obj.GetPosition().y) {
    }

    LostObjectRepr() = default;

    [[nodiscard]] model::LostObject Restore() const {
        return model::LostObject{model::LostObject::Id{id}, type,
                                 model::Position{position.x, position.y}};
    }

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar& id;
        ar& type;
        ar& position;
    }
};

// Представление сессии
struct SessionRepr {
    size_t id = 0;
    size_t map_index = 0;  // Индекс карты в maps_
    uint64_t game_time = 0;
    std::vector<DogRepr> dogs;
    std::vector<LostObjectRepr> lost_objects;
    size_t next_dog_id = 0;
    size_t next_lost_object_id = 0;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar& id;
        ar& map_index;
        ar& game_time;
        ar& dogs;
        ar& lost_objects;
        ar& next_dog_id;
        ar& next_lost_object_id;
    }
};

// Представление игрока
struct PlayerRepr {
    size_t id = 0;
    std::string name;
    size_t dog_id = 0;      // Id собаки
    size_t session_id = 0;  // Id сессии
    std::vector<BagItemRepr> bag;
    unsigned score = 0;
    unsigned bag_capacity = 3;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar& id;
        ar& name;
        ar& dog_id;
        ar& session_id;
        ar& bag;
        ar& score;
        ar& bag_capacity;
    }
};

// Маппинг токена
struct TokenMappingRepr {
    std::string token;
    size_t player_id = 0;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar& token;
        ar& player_id;
    }
};

// Полное состояние игры
struct GameStateRepr {
    std::vector<SessionRepr> sessions;
    std::vector<PlayerRepr> players;
    std::vector<TokenMappingRepr> token_mappings;
    size_t next_player_id = 0;
    size_t next_session_id = 0;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar& sessions;
        ar& players;
        ar& token_mappings;
        ar& next_player_id;
        ar& next_session_id;
    }
};

// Сериализовать GameState в представление
inline GameStateRepr SerializeGameState(const model::Game& game) {
    GameStateRepr repr;

    const auto& maps = game.GetMaps();
    const auto& sessions = game.GetSessions();

    // Сессии
    for (const auto& session : sessions) {
        SessionRepr sess_repr;
        sess_repr.id = *session.GetId();

        // Индекс карты
        const auto* session_map = session.GetMap();
        for (size_t j = 0; j < maps.size(); ++j) {
            if (&maps[j] == session_map) {
                sess_repr.map_index = j;
                break;
            }
        }

        sess_repr.game_time = session.GetGameTime();
        sess_repr.next_dog_id = session.GetNextDogId();
        sess_repr.next_lost_object_id = session.GetNextLostObjectId();

        for (const auto& dog : session.GetDogs()) {
            sess_repr.dogs.push_back(DogRepr(dog));
        }
        for (const auto& obj : session.GetLostObjects()) {
            sess_repr.lost_objects.push_back(LostObjectRepr(obj));
        }

        repr.sessions.push_back(std::move(sess_repr));
    }

    // Игроки
    for (const auto& player_ptr : game.GetPlayers().GetPlayers()) {
        const auto& player = *player_ptr;
        PlayerRepr player_repr;
        player_repr.id = *player.GetId();
        player_repr.name = player.GetName();
        player_repr.score = player.GetScore();
        player_repr.bag_capacity = player.GetBagCapacity();

        if (const auto* dog = player.GetDog()) {
            player_repr.dog_id = *dog->GetId();
        }
        if (const auto* session = player.GetSession()) {
            player_repr.session_id = *session->GetId();
        }

        for (const auto& item : player.GetBag()) {
            player_repr.bag.push_back(BagItemRepr(item));
        }

        repr.players.push_back(std::move(player_repr));
    }

    // Токены
    for (const auto& mapping : game.GetPlayerTokens().GetAllMappings()) {
        TokenMappingRepr tm;
        tm.token = mapping.token;
        tm.player_id = mapping.player_id;
        repr.token_mappings.push_back(std::move(tm));
    }

    // Счётчики
    repr.next_player_id = game.GetPlayers().GetNextId();
    repr.next_session_id = game.GetNextSessionId();

    return repr;
}

// Десериализовать представление в GameState
inline void DeserializeGameState(const GameStateRepr& repr, model::Game& game) {
    const auto& maps = game.GetMaps();

    // Восстанавливаем счётчики Game
    game.SetNextSessionId(repr.next_session_id);
    game.GetPlayers().SetNextId(repr.next_player_id);

    // Восстанавливаем сессии
    for (const auto& sess_repr : repr.sessions) {
        if (sess_repr.map_index >= maps.size()) {
            throw std::runtime_error("Invalid map index in session state");
        }
        const auto* map = &maps[sess_repr.map_index];

        auto& session = game.AddSessionWithId(model::GameSession::Id{sess_repr.id}, map);
        session.SetNextDogId(sess_repr.next_dog_id);
        session.SetNextLostObjectId(sess_repr.next_lost_object_id);

        // Восстанавливаем игровое время
        session.AddGameTime(sess_repr.game_time);

        // Собаки
        for (const auto& dog_repr : sess_repr.dogs) {
            auto dog = dog_repr.Restore();
            session.AddDogWithId(
                model::Dog::Id{*dog.GetId()},
                dog.GetPosition(), dog.GetSpeed(), dog.GetDirection());
        }

        // Потерянные предметы
        for (const auto& obj_repr : sess_repr.lost_objects) {
            session.AddLostObjectWithId(
                model::LostObject::Id{obj_repr.id},
                obj_repr.type,
                model::Position{obj_repr.position.x, obj_repr.position.y});
        }
    }

    // Игроки
    for (const auto& player_repr : repr.players) {
        // Находим собаку и сессию
        model::Dog* dog = nullptr;
        model::GameSession* session = nullptr;

        for (auto& sess : game.GetSessions()) {
            if (*sess.GetId() == player_repr.session_id) {
                session = &sess;
                for (auto& d : sess.GetDogs()) {
                    if (*d.GetId() == player_repr.dog_id) {
                        dog = &d;
                        break;
                    }
                }
                break;
            }
        }

        if (!dog || !session) {
            throw std::runtime_error("Dog or session not found for player "
                                     + std::to_string(player_repr.id));
        }

        auto& player = game.GetPlayers().AddWithId(
            model::Player::Id{player_repr.id},
            dog, session, player_repr.name, player_repr.bag_capacity);

        player.AddScore(player_repr.score);

        for (const auto& bag_item : player_repr.bag) {
            player.AddToBag(bag_item.Restore());
        }
    }

    // Токены
    for (const auto& token_repr : repr.token_mappings) {
        // Найти игрока по ID
        const model::Player* player = nullptr;
        for (const auto& p : game.GetPlayers().GetPlayers()) {
            if (*p->GetId() == token_repr.player_id) {
                player = p.get();
                break;
            }
        }
        if (player) {
            model::Token token{token_repr.token};
            game.GetPlayerTokens().RestoreMapping(token, const_cast<model::Player*>(player));
        }
    }
}

}  // namespace serialization
