#include "model.h"

#include <stdexcept>
#include <cmath>

namespace model {
using namespace std::literals;

// Ширина дороги (собака может отклоняться на 0.4 от оси)
constexpr double ROAD_HALF_WIDTH = 0.4;

void GameSession::Tick(uint64_t delta_ms) {
    if (!map_) {
        return;
    }

    auto delta = std::chrono::milliseconds{delta_ms};
    unsigned looter_count = static_cast<unsigned>(dogs_.size());
    unsigned loot_count = static_cast<unsigned>(lost_objects_.size());
    unsigned loot_type_count = map_->GetLootTypeCount();

    // Генерируем количество новых трофеев
    unsigned new_loot_count = loot_generator_.Generate(delta, loot_count, looter_count);

    if (loot_type_count == 0) {
        return;  // Нет типов трофеев — нечего генерировать
    }

    // Генерируем потерянные объекты
    static size_t next_lost_object_id = 0;
    const auto& roads = map_->GetRoads();
    if (roads.empty()) {
        return;  // Нет дорог — некуда размещать
    }

    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    std::uniform_int_distribution<unsigned> type_dist(0, loot_type_count - 1);
    std::uniform_real_distribution<double> pos_dist(0.0, 1.0);

    for (unsigned i = 0; i < new_loot_count; ++i) {
        // Случайная дорога
        const auto& road = roads[road_dist(loot_rng_)];
        auto start = road.GetStart();
        auto end = road.GetEnd();

        // Случайная позиция на дороге
        double t = pos_dist(loot_rng_);
        double x = start.x + t * (end.x - start.x);
        double y = start.y + t * (end.y - start.y);

        // Случайный тип
        unsigned type = type_dist(loot_rng_);

        LostObject obj(LostObject::Id{next_lost_object_id++}, type, {x, y});
        lost_objects_.emplace_back(std::move(obj));
    }
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (const std::exception&) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (const std::exception&) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

const Road* Map::FindRoadAt(double x, double y) const noexcept {
    for (const auto& road : roads_) {
        auto start = road.GetStart();
        auto end = road.GetEnd();
        
        if (road.IsHorizontal()) {
            // Горизонтальная дорога: y фиксирован, x меняется
            // Учитываем ширину дороги: собака может быть на 0.4 за пределами конца дороги
            double min_x = std::min(start.x, end.x) - ROAD_HALF_WIDTH;
            double max_x = std::max(start.x, end.x) + ROAD_HALF_WIDTH;
            double road_y = static_cast<double>(start.y);
            
            // Проверяем, находится ли точка в пределах дороги по ширине
            if (std::abs(y - road_y) <= ROAD_HALF_WIDTH &&
                x >= min_x && x <= max_x) {
                return &road;
            }
        } else {
            // Вертикальная дорога: x фиксирован, y меняется
            // Учитываем ширину дороги: собака может быть на 0.4 за пределами конца дороги
            double min_y = std::min(start.y, end.y) - ROAD_HALF_WIDTH;
            double max_y = std::max(start.y, end.y) + ROAD_HALF_WIDTH;
            double road_x = static_cast<double>(start.x);
            
            // Проверяем, находится ли точка в пределах дороги по ширине
            if (std::abs(x - road_x) <= ROAD_HALF_WIDTH &&
                y >= min_y && y <= max_y) {
                return &road;
            }
        }
    }
    return nullptr;
}

}  // namespace model
