#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../src/model.h"
#include "../src/loot_generator.h"

using namespace std::literals;

SCENARIO("GameSession loot generation") {
    using namespace model;

    GIVEN("a map with roads and loot types") {
        Map map(Map::Id{"test_map"s}, "Test Map"s);
        map.AddRoad(Road(Road::HORIZONTAL, {0, 0}, 100));
        map.AddRoad(Road(Road::VERTICAL, {0, 0}, 100));
        map.SetLootTypeCount(3);  // 3 типа трофеев
        map.SetDogSpeed(1.0);

        Game game;
        game.AddMap(std::move(map));
        game.SetLootGeneratorConfig(1000ms, 1.0);  // period=1s, probability=1.0

        const auto* map_ptr = game.FindMap(Map::Id{"test_map"s});
        REQUIRE(map_ptr != nullptr);

        WHEN("a session is created and ticked with no dogs") {
            auto& session = game.GetOrCreateSession(map_ptr);

            // Тик 1 секунду (probability=1.0, loot_shortage=0 т.к. 0 dogs)
            session.Tick(1000);

            THEN("no loot is generated because there are no dogs (looters)") {
                REQUIRE(session.GetLostObjects().empty());
            }
        }

        WHEN("a session has one dog and is ticked") {
            auto& session = game.GetOrCreateSession(map_ptr);

            // Добавляем собаку
            session.AddDog(Dog(Dog::Id{0}, Position{0.0, 0.0}, Speed{0.0, 0.0}, Direction::NORTH));

            // Тик 1 секунду (probability=1.0, loot_shortage = 1 - 0 = 1)
            session.Tick(1000);

            THEN("loot is generated") {
                REQUIRE(session.GetLostObjects().size() == 1);
            }

            THEN("loot type is in valid range [0, loot_type_count-1]") {
                const auto& loot = session.GetLostObjects();
                for (const auto& obj : loot) {
                    REQUIRE(obj.GetType() < map_ptr->GetLootTypeCount());
                }
            }

            THEN("loot position is on one of the roads") {
                const auto& loot = session.GetLostObjects();
                for (const auto& obj : loot) {
                    const auto* road = map_ptr->FindRoadAt(obj.GetPosition().x, obj.GetPosition().y);
                    REQUIRE(road != nullptr);
                }
            }
        }

        WHEN("a session has two dogs and loot is generated up to looter count") {
            auto& session = game.GetOrCreateSession(map_ptr);

            // Добавляем двух собак
            session.AddDog(Dog(Dog::Id{0}, Position{0.0, 0.0}, Speed{0.0, 0.0}, Direction::NORTH));
            session.AddDog(Dog(Dog::Id{1}, Position{0.0, 0.0}, Speed{0.0, 0.0}, Direction::NORTH));

            // Первый тик: loot_shortage = 2 - 0 = 2, probability=1.0 => 2 loot
            session.Tick(1000);
            REQUIRE(session.GetLostObjects().size() == 2);

            // Второй тик: loot_shortage = 2 - 2 = 0 => 0 loot
            session.Tick(1000);

            THEN("total loot count equals looter count and no more is generated") {
                REQUIRE(session.GetLostObjects().size() == 2);
            }
        }

        WHEN("session is ticked with probability less than 1") {
            // Пере-создаём игру с probability=0.5
            Game game2;
            Map map2(Map::Id{"test_map2"s}, "Test Map 2"s);
            map2.AddRoad(Road(Road::HORIZONTAL, {0, 0}, 100));
            map2.AddRoad(Road(Road::VERTICAL, {0, 0}, 100));
            map2.SetLootTypeCount(2);
            map2.SetDogSpeed(1.0);
            game2.AddMap(std::move(map2));
            game2.SetLootGeneratorConfig(1000ms, 0.5);

            const auto* map2_ptr = game2.FindMap(Map::Id{"test_map2"s});
            REQUIRE(map2_ptr != nullptr);

            auto& session2 = game2.GetOrCreateSession(map2_ptr);
            session2.AddDog(Dog(Dog::Id{0}, Position{0.0, 0.0}, Speed{0.0, 0.0}, Direction::NORTH));

            // Тик 1 секунду с probability=0.5 и 1 dog => loot_shortage=1
            // expected loot = round(1 * 0.5) = 0 или 1
            session2.Tick(1000);

            THEN("loot count does not exceed looter count") {
                REQUIRE(session2.GetLostObjects().size() <= 1);
            }
        }
    }
}

SCENARIO("LostObject properties") {
    using namespace model;

    GIVEN("a LostObject") {
        LostObject obj(LostObject::Id{42}, 2, Position{10.5, 20.3});

        THEN("id is correct") {
            REQUIRE(*obj.GetId() == 42);
        }

        THEN("type is correct") {
            REQUIRE(obj.GetType() == 2);
        }

        THEN("position is correct") {
            REQUIRE(obj.GetPosition().x == 10.5);
            REQUIRE(obj.GetPosition().y == 20.3);
        }
    }
}

SCENARIO("Map loot type count") {
    using namespace model;

    GIVEN("a map") {
        Map map(Map::Id{"map1"s}, "Map 1"s);

        WHEN("loot type count is not set") {
            THEN("it defaults to 0") {
                REQUIRE(map.GetLootTypeCount() == 0);
            }
        }

        WHEN("loot type count is set") {
            map.SetLootTypeCount(5);

            THEN("it returns the set value") {
                REQUIRE(map.GetLootTypeCount() == 5);
            }
        }
    }
}

SCENARIO("Player score management") {
    using namespace model;

    GIVEN("a player") {
        Game game;
        Map map(Map::Id{"map1"s}, "Map 1"s);
        map.AddRoad(Road(Road::HORIZONTAL, {0, 0}, 100));
        game.AddMap(std::move(map));
        const auto* map_ptr = game.FindMap(Map::Id{"map1"s});
        REQUIRE(map_ptr != nullptr);

        auto& session = game.GetOrCreateSession(map_ptr);
        session.AddDog(Dog(Dog::Id{0}, Position{0.0, 0.0}, Speed{0.0, 0.0}, Direction::NORTH));
        auto& dog = session.GetDogs().back();
        auto& player = game.GetPlayers().Add(&dog, &session, "TestPlayer"s);

        WHEN("player is created") {
            THEN("score is zero") {
                REQUIRE(player.GetScore() == 0);
            }
        }

        WHEN("score is added") {
            player.AddScore(50);

            THEN("score is updated") {
                REQUIRE(player.GetScore() == 50);
            }
        }

        WHEN("score is added multiple times") {
            player.AddScore(10);
            player.AddScore(20);
            player.AddScore(30);

            THEN("scores are accumulated") {
                REQUIRE(player.GetScore() == 60);
            }
        }
    }
}

SCENARIO("Map loot values") {
    using namespace model;

    GIVEN("a map without loot values") {
        Map map(Map::Id{"map1"s}, "Map 1"s);

        THEN("loot values are empty") {
            REQUIRE(map.GetLootValues().empty());
        }
    }

    GIVEN("a map with loot values") {
        Map map(Map::Id{"map1"s}, "Map 1"s);
        map.SetLootValues({10, 30, 50});

        THEN("loot values are accessible") {
            const auto& values = map.GetLootValues();
            REQUIRE(values.size() == 3);
            REQUIRE(values[0] == 10);
            REQUIRE(values[1] == 30);
            REQUIRE(values[2] == 50);
        }
    }
}
