#include "request_handler.h"
#include <cctype>
#include <fstream>
#include <unordered_map>

namespace http_handler {

http::response<http::string_body> RequestHandler::MakeJsonResponse(http::status status, std::string_view body,
                                                                    unsigned version, bool keep_alive) {
    http::response<http::string_body> res(status, version);
    res.set(http::field::content_type, "application/json");
    res.body() = body;
    res.content_length(body.size());
    res.keep_alive(keep_alive);
    return res;
}

http::response<http::string_body> RequestHandler::MakeErrorResponse(http::status status, std::string_view message,
                                                                     unsigned version, bool keep_alive,
                                                                     std::string_view code) {
    boost::json::object obj;
    obj["code"] = boost::json::string_view(code.data(), code.size());
    obj["message"] = boost::json::string_view(message.data(), message.size());
    std::string body = boost::json::serialize(obj);
    return MakeJsonResponse(status, body, version, keep_alive);
}

http::response<http::string_body> RequestHandler::MakePlainResponse(http::status status, std::string_view text,
                                                                    unsigned version, bool keep_alive) {
    http::response<http::string_body> res(status, version);
    res.set(http::field::content_type, "text/plain");
    res.body() = text;
    res.prepare_payload();
    res.keep_alive(keep_alive);
    return res;
}

std::string RequestHandler::SerializeMapsList(const model::Game::Maps& maps) {
    boost::json::array arr;
    for (const auto& map : maps) {
        boost::json::object obj;
        obj["id"] = *map.GetId();
        obj["name"] = map.GetName();
        arr.push_back(std::move(obj));
    }
    return boost::json::serialize(arr);
}

std::string RequestHandler::SerializeMap(const model::Map& map) {
    boost::json::object obj;
    obj["id"] = *map.GetId();
    obj["name"] = map.GetName();
    obj["roads"] = SerializeRoads(map.GetRoads());
    obj["buildings"] = SerializeBuildings(map.GetBuildings());
    obj["offices"] = SerializeOffices(map.GetOffices());
    return boost::json::serialize(obj);
}

boost::json::array RequestHandler::SerializeRoads(const model::Map::Roads& roads) {
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

boost::json::array RequestHandler::SerializeBuildings(const model::Map::Buildings& buildings) {
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

boost::json::array RequestHandler::SerializeOffices(const model::Map::Offices& offices) {
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

std::string RequestHandler::UrlDecode(std::string_view encoded) {
    std::string res;
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
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

std::string RequestHandler::GetMimeType(const std::string& extension) {
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

bool RequestHandler::IsSubPath(const std::filesystem::path& path, const std::filesystem::path& base) {
    std::error_code ec;
    auto path_canon = std::filesystem::weakly_canonical(path, ec);
    if (ec) return false;
    auto base_canon = std::filesystem::weakly_canonical(base, ec);
    if (ec) return false;

    // Сравниваем компоненты пути
    for (auto b = base_canon.begin(), p = path_canon.begin(); b != base_canon.end(); ++b, ++p) {
        if (p == path_canon.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

}  // namespace http_handler