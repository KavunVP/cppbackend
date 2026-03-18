#include "model.h"

#include <stdexcept>
#include <cmath>

namespace model {
using namespace std::literals;

// Ширина дороги (собака может отклоняться на 0.4 от оси)
constexpr double ROAD_HALF_WIDTH = 0.4;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
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
        } catch (...) {
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
