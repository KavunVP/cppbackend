#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "../src/tv.h"

namespace Catch {

template <>
struct StringMaker<std::nullopt_t> {
    static std::string convert(std::nullopt_t) {
        using namespace std::literals;
        return "nullopt"s;
    }
};

template <typename T>
struct StringMaker<std::optional<T>> {
    static std::string convert(const std::optional<T>& opt_value) {
        if (opt_value) {
            return StringMaker<T>::convert(*opt_value);
        } else {
            return StringMaker<std::nullopt_t>::convert(std::nullopt);
        }
    }
};

}  // namespace Catch

SCENARIO("TV", "[TV]") {
    GIVEN("A TV") {
        TV tv;

        SECTION("Initially it is off and doesn't show any channel") {
            CHECK(!tv.IsTurnedOn());
            CHECK(!tv.GetChannel().has_value());
        }

        WHEN("it is turned off") {
            REQUIRE(!tv.IsTurnedOn());

            THEN("it can't select any channel") {
                CHECK_THROWS_AS(tv.SelectChannel(10), std::logic_error);
                CHECK(tv.GetChannel() == std::nullopt);
                tv.TurnOn();
                CHECK(tv.GetChannel() == 1);
            }
        }

        WHEN("it is turned on first time") {
            tv.TurnOn();

            THEN("it is turned on and shows channel #1") {
                CHECK(tv.IsTurnedOn());
                CHECK(tv.GetChannel() == 1);

                AND_WHEN("it is turned off") {
                    tv.TurnOff();

                    THEN("it is turned off and doesn't show any channel") {
                        CHECK(!tv.IsTurnedOn());
                        CHECK(tv.GetChannel() == std::nullopt);
                    }
                }
            }

            AND_THEN("it can select channel from 1 to 99") {
                tv.SelectChannel(50);
                CHECK(tv.GetChannel() == 50);

                tv.SelectChannel(1);
                CHECK(tv.GetChannel() == 1);

                tv.SelectChannel(99);
                CHECK(tv.GetChannel() == 99);
            }

            AND_THEN("SelectChannel throws out_of_range for invalid channels") {
                CHECK_THROWS_AS(tv.SelectChannel(0), std::out_of_range);
                CHECK_THROWS_AS(tv.SelectChannel(100), std::out_of_range);
                CHECK_THROWS_AS(tv.SelectChannel(-5), std::out_of_range);
            }

            AND_THEN("SelectChannel does nothing if channel is same as current") {
                tv.SelectChannel(5);
                CHECK(tv.GetChannel() == 5);
                // Не запоминает как "предыдущий" — просто ничего не делает
                tv.SelectChannel(5);
                CHECK(tv.GetChannel() == 5);
            }

            AND_THEN("SelectLastViewedChannel switches between two last channels") {
                tv.SelectChannel(10);
                CHECK(tv.GetChannel() == 10);

                tv.SelectLastViewedChannel();
                CHECK(tv.GetChannel() == 1);  // возвращаемся к каналу 1

                tv.SelectLastViewedChannel();
                CHECK(tv.GetChannel() == 10);  // обратно к 10

                tv.SelectLastViewedChannel();
                CHECK(tv.GetChannel() == 1);
            }

            AND_THEN("SelectLastViewedChannel when no previous channel yet") {
                // Предыдущий канал — 1 (изначально), текущий — 1
                // Переключение должно оставить 1
                tv.SelectLastViewedChannel();
                CHECK(tv.GetChannel() == 1);
            }

            AND_WHEN("channel is selected and TV is turned off then on") {
                tv.SelectChannel(25);
                tv.TurnOff();
                tv.TurnOn();

                THEN("previous channel is restored") {
                    CHECK(tv.GetChannel() == 25);
                }
            }
        }
    }
}
