#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace collision_detector {

// ------------------- Специализация StringMaker -------------------

}  // namespace collision_detector

namespace Catch {
template <>
struct StringMaker<collision_detector::GatheringEvent> {
    static std::string convert(collision_detector::GatheringEvent const& value) {
        std::ostringstream tmp;
        tmp << "(gatherer=" << value.gatherer_id << ", item=" << value.item_id
            << ", sq_dist=" << value.sq_distance << ", time=" << value.time << ")";
        return tmp.str();
    }
};
}  // namespace Catch

namespace collision_detector {

// ------------------- Тестовый провайдер -------------------

class MockItemGathererProvider : public ItemGathererProvider {
public:
    MockItemGathererProvider(std::vector<Item> items, std::vector<Gatherer> gatherers)
        : items_(std::move(items)), gatherers_(std::move(gatherers)) {}

    size_t ItemsCount() const override { return items_.size(); }
    Item GetItem(size_t idx) const override { return items_[idx]; }
    size_t GatherersCount() const override { return gatherers_.size(); }
    Gatherer GetGatherer(size_t idx) const override { return gatherers_[idx]; }

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

// ------------------- Вспомогательные утилиты -------------------

constexpr double EPSILON = 1e-10;

struct EventComparator {
    bool operator()(const GatheringEvent& a, const GatheringEvent& b) const {
        if (a.time != b.time) return a.time < b.time;
        if (a.gatherer_id != b.gatherer_id) return a.gatherer_id < b.gatherer_id;
        return a.item_id < b.item_id;
    }
};

bool EventsAreEqual(const GatheringEvent& a, const GatheringEvent& b) {
    return a.gatherer_id == b.gatherer_id && a.item_id == b.item_id &&
           std::abs(a.sq_distance - b.sq_distance) < EPSILON &&
           std::abs(a.time - b.time) < EPSILON;
}

// Матчер для сравнения векторов событий с учётом погрешности
class GatheringEventsMatcher : public Catch::Matchers::MatcherBase<std::vector<GatheringEvent>> {
public:
    explicit GatheringEventsMatcher(std::vector<GatheringEvent> exp)
        : expected_(std::move(exp)) {
        std::sort(expected_.begin(), expected_.end(), EventComparator{});
    }

    bool match(std::vector<GatheringEvent> const& actual) const override;
    std::string describe() const override;

private:
    std::vector<GatheringEvent> expected_;
};

bool GatheringEventsMatcher::match(std::vector<GatheringEvent> const& actual) const {
    if (actual.size() != expected_.size()) return false;
    auto sorted_actual = actual;
    std::sort(sorted_actual.begin(), sorted_actual.end(), EventComparator{});
    for (size_t i = 0; i < expected_.size(); ++i) {
        if (!EventsAreEqual(sorted_actual[i], expected_[i])) return false;
    }
    return true;
}

std::string GatheringEventsMatcher::describe() const {
    std::ostringstream oss;
    oss << "equals expected events: [";
    for (size_t i = 0; i < expected_.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "(g=" << expected_[i].gatherer_id
            << ", i=" << expected_[i].item_id
            << ", d=" << expected_[i].sq_distance
            << ", t=" << expected_[i].time << ")";
    }
    oss << "]";
    return oss.str();
}

inline GatheringEventsMatcher GatheringEventsEqual(std::vector<GatheringEvent> expected) {
    return GatheringEventsMatcher{std::move(expected)};
}

// Матчер проверки хронологического порядка
class ChronologicalOrderMatcher : public Catch::Matchers::MatcherBase<std::vector<GatheringEvent>> {
public:
    bool match(std::vector<GatheringEvent> const& actual) const override;
    std::string describe() const override;
};

bool ChronologicalOrderMatcher::match(std::vector<GatheringEvent> const& actual) const {
    for (size_t i = 1; i < actual.size(); ++i) {
        if (actual[i].time < actual[i - 1].time) return false;
    }
    return true;
}

std::string ChronologicalOrderMatcher::describe() const {
    return "events are in chronological order";
}

inline ChronologicalOrderMatcher InChronologicalOrder() {
    return ChronologicalOrderMatcher{};
}

// ====================== ТЕСТЫ ======================

// ----- Пустые входные данные -----

TEST_CASE("No items, no gatherers — empty result", "[gather]") {
    MockItemGathererProvider provider({}, {});
    auto result = FindGatherEvents(provider);
    REQUIRE(result.empty());
}

TEST_CASE("Items but no gatherers — empty result", "[gather]") {
    MockItemGathererProvider provider(
        {Item{geom::Point2D{0, 0}, 1.0}}, {});
    auto result = FindGatherEvents(provider);
    REQUIRE(result.empty());
}

TEST_CASE("Gatherers but no items — empty result", "[gather]") {
    MockItemGathererProvider provider(
        {}, {Gatherer{geom::Point2D{0, 0}, geom::Point2D{10, 0}, 1.0}});
    auto result = FindGatherEvents(provider);
    REQUIRE(result.empty());
}

// ----- Одиночное столкновение -----

TEST_CASE("Single direct collision — item on the path", "[gather]") {
    // Gatherer moves from (0,0) to (10,0), width=1
    // Item at (5,0), width=1 => collect_radius = 2
    MockItemGathererProvider provider(
        {Item{geom::Point2D{5.0, 0.0}, 1.0}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE_THAT(result, GatheringEventsEqual({
        GatheringEvent{0, 0, 0.0, 0.5}
    }));
}

TEST_CASE("Single collision — item offset but within radius", "[gather]") {
    // Gatherer: (0,0)->(10,0), width=1
    // Item: (5, 1.5), width=0.5 => collect_radius = 1.5
    // sq_distance = 1.5^2 = 2.25 <= 2.25 => collision
    MockItemGathererProvider provider(
        {Item{geom::Point2D{5.0, 1.5}, 0.5}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].item_id == 0);
    REQUIRE(result[0].gatherer_id == 0);
    REQUIRE(result[0].time == Catch::Approx(0.5).epsilon(EPSILON));
    REQUIRE(result[0].sq_distance == Catch::Approx(2.25).epsilon(EPSILON));
}

// ----- Отсутствие столкновения -----

TEST_CASE("No collision — item too far from path", "[gather]") {
    // Gatherer: (0,0)->(10,0), width=1
    // Item: (5, 3), width=1 => collect_radius=2, sq_distance=9 > 4
    MockItemGathererProvider provider(
        {Item{geom::Point2D{5.0, 3.0}, 1.0}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.empty());
}

TEST_CASE("No collision — item before start (proj_ratio < 0)", "[gather]") {
    MockItemGathererProvider provider(
        {Item{geom::Point2D{-1.0, 0.0}, 1.0}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.empty());
}

TEST_CASE("No collision — item after end (proj_ratio > 1)", "[gather]") {
    MockItemGathererProvider provider(
        {Item{geom::Point2D{11.0, 0.0}, 1.0}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.empty());
}

TEST_CASE("No collision — zero movement", "[gather]") {
    // Gatherer doesn't move
    MockItemGathererProvider provider(
        {Item{geom::Point2D{0.0, 0.0}, 1.0}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{0.0, 0.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.empty());
}

// ----- Граничные случаи -----

TEST_CASE("Boundary collision — item exactly at collect_radius", "[gather]") {
    // Gatherer: (0,0)->(10,0), width=1
    // Item: (5, 2), width=1 => collect_radius=2, sq_distance=4 == 4
    MockItemGathererProvider provider(
        {Item{geom::Point2D{5.0, 2.0}, 1.0}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].sq_distance == Catch::Approx(4.0).epsilon(EPSILON));
    REQUIRE(result[0].time == Catch::Approx(0.5).epsilon(EPSILON));
}

TEST_CASE("Boundary collision — item at start point", "[gather]") {
    // Item exactly at gatherer start position
    MockItemGathererProvider provider(
        {Item{geom::Point2D{0.0, 0.0}, 1.0}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].time == Catch::Approx(0.0).epsilon(EPSILON));
    REQUIRE(result[0].sq_distance == Catch::Approx(0.0).epsilon(EPSILON));
}

TEST_CASE("Boundary collision — item at end point", "[gather]") {
    // Item exactly at gatherer end position
    MockItemGathererProvider provider(
        {Item{geom::Point2D{10.0, 0.0}, 1.0}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].time == Catch::Approx(1.0).epsilon(EPSILON));
    REQUIRE(result[0].sq_distance == Catch::Approx(0.0).epsilon(EPSILON));
}

// ----- Несколько столкновений с одним собирателем -----

TEST_CASE("Multiple items collected by one gatherer", "[gather]") {
    // Gatherer: (0,0)->(10,0), width=1
    // Item 0: (2, 0), width=1 => time=0.2
    // Item 1: (5, 0), width=1 => time=0.5
    // Item 2: (8, 0), width=1 => time=0.8
    MockItemGathererProvider provider(
        {
            Item{geom::Point2D{2.0, 0.0}, 1.0},
            Item{geom::Point2D{5.0, 0.0}, 1.0},
            Item{geom::Point2D{8.0, 0.0}, 1.0}
        },
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 3);
    REQUIRE_THAT(result, InChronologicalOrder());
    REQUIRE(result[0].item_id == 0);
    REQUIRE(result[0].time == Catch::Approx(0.2).epsilon(EPSILON));
    REQUIRE(result[1].item_id == 1);
    REQUIRE(result[1].time == Catch::Approx(0.5).epsilon(EPSILON));
    REQUIRE(result[2].item_id == 2);
    REQUIRE(result[2].time == Catch::Approx(0.8).epsilon(EPSILON));
}

// ----- Несколько собирателей -----

TEST_CASE("Multiple gatherers collecting different items", "[gather]") {
    // Gatherer 0: (0,0)->(10,0), width=1
    // Gatherer 1: (0,5)->(10,5), width=1
    // Item 0: (3, 0), width=1 => collected by G0 at t=0.3
    // Item 1: (7, 5), width=1 => collected by G1 at t=0.7
    MockItemGathererProvider provider(
        {
            Item{geom::Point2D{3.0, 0.0}, 1.0},
            Item{geom::Point2D{7.0, 5.0}, 1.0}
        },
        {
            Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0},
            Gatherer{geom::Point2D{0.0, 5.0}, geom::Point2D{10.0, 5.0}, 1.0}
        }
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 2);
    REQUIRE_THAT(result, GatheringEventsEqual({
        GatheringEvent{0, 0, 0.0, 0.3},
        GatheringEvent{1, 1, 0.0, 0.7}
    }));
}

TEST_CASE("Same item collected by multiple gatherers", "[gather]") {
    // Gatherer 0: (0,0)->(10,0), width=1
    // Gatherer 1: (0,5)->(10,5), width=10
    // Item: (5, 0), width=1
    // G0: proj=0.5, sq_dist=0
    // G1: u=(5,-5), v=(10,0), u·v=50, |v|^2=100, proj=0.5
    //     |u|^2=50, sq_dist=50-25=25, collect_radius=11, 25<=121
    MockItemGathererProvider provider(
        {Item{geom::Point2D{5.0, 0.0}, 1.0}},
        {
            Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0},
            Gatherer{geom::Point2D{0.0, 5.0}, geom::Point2D{10.0, 5.0}, 10.0}
        }
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 2);
    REQUIRE_THAT(result, GatheringEventsEqual({
        GatheringEvent{0, 0, 0.0, 0.5},
        GatheringEvent{0, 1, 25.0, 0.5}  // same item (0), different gatherer (1)
    }));
}

TEST_CASE("Multiple gatherers, multiple items, mixed collisions", "[gather]") {
    // Gatherer 0: (0,0)->(10,0), width=1
    // Gatherer 1: (5,0)->(5,10), width=1
    // Item 0: (2, 0), width=0.5 => G0 at t=0.2, sq_dist=0
    // Item 1: (5, 3), width=0.5 => G1 at t=0.3, sq_dist=0
    // Item 2: (12, 0), width=0.5 => too far, no collision
    // Item 3: (5, 12), width=0.5 => too far, no collision
    MockItemGathererProvider provider(
        {
            Item{geom::Point2D{2.0, 0.0}, 0.5},
            Item{geom::Point2D{5.0, 3.0}, 0.5},
            Item{geom::Point2D{12.0, 0.0}, 0.5},
            Item{geom::Point2D{5.0, 12.0}, 0.5}
        },
        {
            Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0},
            Gatherer{geom::Point2D{5.0, 0.0}, geom::Point2D{5.0, 10.0}, 1.0}
        }
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 2);
    REQUIRE_THAT(result, GatheringEventsEqual({
        GatheringEvent{0, 0, 0.0, 0.2},
        GatheringEvent{1, 1, 0.0, 0.3}
    }));
}

// ----- Хронологический порядок -----

TEST_CASE("Events are returned in chronological order", "[gather]") {
    // Gatherer: (0,0)->(20,0), width=1
    // Items at x=2, x=6, x=10, x=14, x=18
    std::vector<Item> items;
    for (double x = 2.0; x <= 18.0; x += 4.0) {
        items.push_back(Item{geom::Point2D{x, 0.0}, 1.0});
    }
    MockItemGathererProvider provider(
        std::move(items),
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{20.0, 0.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 5);
    REQUIRE_THAT(result, InChronologicalOrder());
    for (size_t i = 0; i < 5; ++i) {
        CHECK(result[i].item_id == i);
        CHECK(result[i].time == Catch::Approx(0.1 + 0.2 * i).epsilon(EPSILON));
    }
}

// ----- Диагональное перемещение -----

TEST_CASE("Diagonal movement collision", "[gather]") {
    // Gatherer: (0,0)->(10,10), width=1
    // Item: (5, 5), width=1 => on the path
    // u=(5,5), v=(10,10), u·v=100, |v|^2=200, proj=0.5
    // |u|^2=50, sq_dist=50-10000/200=50-50=0
    MockItemGathererProvider provider(
        {Item{geom::Point2D{5.0, 5.0}, 1.0}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 10.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].time == Catch::Approx(0.5).epsilon(EPSILON));
    REQUIRE(result[0].sq_distance == Catch::Approx(0.0).epsilon(EPSILON));
}

TEST_CASE("Diagonal movement — no collision (too far)", "[gather]") {
    // Gatherer: (0,0)->(10,10), width=1
    // Item: (0, 5), width=1 => collect_radius=2
    // u=(0,5), v=(10,10), u·v=50, |v|^2=200, proj=0.25
    // |u|^2=25, sq_dist=25-2500/200=25-12.5=12.5 > 4
    MockItemGathererProvider provider(
        {Item{geom::Point2D{0.0, 5.0}, 1.0}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 10.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.empty());
}

// ----- Отрицательные координаты -----

TEST_CASE("Collision with negative coordinates", "[gather]") {
    // Gatherer: (-10,-10)->(0,0), width=1
    // Item: (-5, -5), width=1
    MockItemGathererProvider provider(
        {Item{geom::Point2D{-5.0, -5.0}, 1.0}},
        {Gatherer{geom::Point2D{-10.0, -10.0}, geom::Point2D{0.0, 0.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].time == Catch::Approx(0.5).epsilon(EPSILON));
    REQUIRE(result[0].sq_distance == Catch::Approx(0.0).epsilon(EPSILON));
}

// ----- Очень маленькие и очень большие радиусы -----

TEST_CASE("Very large gatherer radius collects distant item", "[gather]") {
    // Gatherer: (0,0)->(10,0), width=100
    // Item: (5, 10), width=1 => collect_radius=101, sq_distance=100 <= 10201
    MockItemGathererProvider provider(
        {Item{geom::Point2D{5.0, 10.0}, 1.0}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 100.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].time == Catch::Approx(0.5).epsilon(EPSILON));
    REQUIRE(result[0].sq_distance == Catch::Approx(100.0).epsilon(EPSILON));
}

TEST_CASE("Very small radii — precise collision", "[gather]") {
    // Gatherer: (0,0)->(10,0), width=0.001
    // Item: (5, 0.001), width=0.001 => collect_radius=0.002
    // sq_distance = 0.000001 = 1e-6, collect_radius^2 = 4e-6, 1e-6 <= 4e-6
    MockItemGathererProvider provider(
        {Item{geom::Point2D{5.0, 0.001}, 0.001}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 0.001}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].sq_distance == Catch::Approx(1e-6).margin(1e-12));
}

TEST_CASE("Very small radii — no collision (just outside)", "[gather]") {
    // Gatherer: (0,0)->(10,0), width=0.001
    // Item: (5, 0.003), width=0.001 => collect_radius=0.002
    // sq_distance = 9e-6 > 4e-6
    MockItemGathererProvider provider(
        {Item{geom::Point2D{5.0, 0.003}, 0.001}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 0.001}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.empty());
}

// ----- Предмет остаётся после столкновения -----

TEST_CASE("Item remains after collision — collected by second gatherer", "[gather]") {
    // Two gatherers pass through the same item at different times
    // Gatherer 0: (0,0)->(10,0), width=1
    // Gatherer 1: (5,-5)->(5,5), width=1
    // Item: (5, 0), width=1
    // G0: proj=0.5, sq_dist=0
    // G1: u=(0,5), v=(0,10), proj=0.5, sq_dist=0
    MockItemGathererProvider provider(
        {Item{geom::Point2D{5.0, 0.0}, 1.0}},
        {
            Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0},
            Gatherer{geom::Point2D{5.0, -5.0}, geom::Point2D{5.0, 5.0}, 1.0}
        }
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 2);
    REQUIRE_THAT(result, GatheringEventsEqual({
        GatheringEvent{0, 0, 0.0, 0.5},
        GatheringEvent{0, 1, 0.0, 0.5}  // item_id=0, gatherer_id=1
    }));
}

// ----- Только граничные (без столкновений) -----

TEST_CASE("Item just outside collision radius — no event", "[gather]") {
    // Gatherer: (0,0)->(10,0), width=1
    // Item: (5, 2.0000000001), width=1 => sq_distance > 4
    MockItemGathererProvider provider(
        {Item{geom::Point2D{5.0, 2.0000000001}, 1.0}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.empty());
}

// ----- Столкновение в начальной точке с ненулевым sq_distance -----

TEST_CASE("Collision at start with non-zero perpendicular distance", "[gather]") {
    // Gatherer: (0,0)->(10,0), width=2
    // Item: (0, 1.5), width=0.5 => collect_radius=2.5
    // u=(0,1.5), v=(10,0), u·v=0, proj=0
    // |u|^2=2.25, sq_dist=2.25 - 0 = 2.25, 2.25 <= 6.25
    MockItemGathererProvider provider(
        {Item{geom::Point2D{0.0, 1.5}, 0.5}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 2.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].time == Catch::Approx(0.0).epsilon(EPSILON));
    REQUIRE(result[0].sq_distance == Catch::Approx(2.25).epsilon(EPSILON));
}

TEST_CASE("Collision at end with non-zero perpendicular distance", "[gather]") {
    // Gatherer: (0,0)->(10,0), width=2
    // Item: (10, 1.5), width=0.5 => collect_radius=2.5
    // u=(10,1.5), v=(10,0), u·v=100, proj=1.0
    // |u|^2=102.25, sq_dist=102.25 - 10000/100 = 2.25, 2.25 <= 6.25
    MockItemGathererProvider provider(
        {Item{geom::Point2D{10.0, 1.5}, 0.5}},
        {Gatherer{geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 2.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].time == Catch::Approx(1.0).epsilon(EPSILON));
    REQUIRE(result[0].sq_distance == Catch::Approx(2.25).epsilon(EPSILON));
}

// ----- Вертикальное перемещение -----

TEST_CASE("Vertical movement collision", "[gather]") {
    // Gatherer: (3,0)->(3,10), width=1
    // Item: (3, 5), width=1
    MockItemGathererProvider provider(
        {Item{geom::Point2D{3.0, 5.0}, 1.0}},
        {Gatherer{geom::Point2D{3.0, 0.0}, geom::Point2D{3.0, 10.0}, 1.0}}
    );
    auto result = FindGatherEvents(provider);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].time == Catch::Approx(0.5).epsilon(EPSILON));
    REQUIRE(result[0].sq_distance == Catch::Approx(0.0).epsilon(EPSILON));
}

}  // namespace collision_detector
