#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <sstream>

#include "../src/state_serialization.h"

using namespace model;
using namespace serialization;
using namespace std::literals;

namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

struct Fixture {
    std::stringstream strm;
    OutputArchive output_archive{strm};
};

}  // namespace

SCENARIO_METHOD(Fixture, "DogRepr serialization") {
    GIVEN("A dog") {
        Dog dog{Dog::Id{42}, Position{42.2, 12.5}, Speed{2.3, -1.2}, Direction::EAST};

        WHEN("dog is serialized via DogRepr") {
            {
                DogRepr repr{dog};
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                DogRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();

                CHECK(dog.GetId() == restored.GetId());
                CHECK(dog.GetPosition().x == Catch::Approx(restored.GetPosition().x));
                CHECK(dog.GetPosition().y == Catch::Approx(restored.GetPosition().y));
                CHECK(dog.GetSpeed().dx == Catch::Approx(restored.GetSpeed().dx));
                CHECK(dog.GetSpeed().dy == Catch::Approx(restored.GetSpeed().dy));
                CHECK(dog.GetDirection() == restored.GetDirection());
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "LostObjectRepr serialization") {
    GIVEN("A lost object") {
        LostObject obj{LostObject::Id{10}, 2u, Position{5.5, 10.3}};

        WHEN("it is serialized via LostObjectRepr") {
            {
                LostObjectRepr repr{obj};
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                LostObjectRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();

                CHECK(obj.GetId() == restored.GetId());
                CHECK(obj.GetType() == restored.GetType());
                CHECK(obj.GetPosition().x == Catch::Approx(restored.GetPosition().x));
                CHECK(obj.GetPosition().y == Catch::Approx(restored.GetPosition().y));
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "BagItemRepr serialization") {
    GIVEN("A bag item") {
        BagItem item{10, 3u};

        WHEN("it is serialized via BagItemRepr") {
            {
                BagItemRepr repr{item};
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                BagItemRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();

                CHECK(item.id == restored.id);
                CHECK(item.type == restored.type);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Full game state serialization") {
    GIVEN("A game with maps, sessions, players and tokens") {
        // Создаём игру с одной картой
        Game game;
        Map map(Map::Id{"map1"s}, "Test Map"s);
        map.AddRoad(Road(Road::HORIZONTAL, {0, 0}, 100));
        map.AddRoad(Road(Road::VERTICAL, {0, 0}, 100));
        map.SetLootTypeCount(3);
        map.SetDogSpeed(1.0);
        game.AddMap(std::move(map));

        const auto* map_ptr = game.FindMap(Map::Id{"map1"s});
        REQUIRE(map_ptr != nullptr);

        // Создаём сессию
        auto& session = game.GetOrCreateSession(map_ptr);

        // Добавляем собаку
        session.AddDog(Dog(Dog::Id{0}, Position{10.0, 20.0}, Speed{0.0, 0.0}, Direction::NORTH));
        auto& dog = session.GetDogs().back();

        // Добавляем игрока
        auto& player = game.GetPlayers().Add(&dog, &session, "TestPlayer"s);
        player.AddToBag(BagItem{0, 1u});
        player.AddScore(50);

        // Генерируем токен
        auto token = game.GetPlayerTokens().GenerateToken();
        game.GetPlayerTokens().AddPlayer(token, &player);

        // Добавляем потерянный предмет
        session.AddLostObject(LostObject(LostObject::Id{0}, 2u, Position{50.0, 60.0}));

        WHEN("game state is serialized") {
            auto repr = SerializeGameState(game);

            std::stringstream strm;
            {
                OutputArchive output_archive{strm};
                output_archive << repr;
            }

            THEN("it can be deserialized and restored") {
                InputArchive input_archive{strm};
                GameStateRepr restored_repr;
                input_archive >> restored_repr;

                // Проверяем что сессии восстановлены
                CHECK(restored_repr.sessions.size() == 1);
                CHECK(restored_repr.sessions[0].dogs.size() == 1);
                CHECK(restored_repr.sessions[0].lost_objects.size() == 1);
                CHECK(restored_repr.players.size() == 1);
                CHECK(restored_repr.token_mappings.size() == 1);
            }
        }
    }
}
