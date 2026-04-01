#include "request_handler.h"
#include <boost/json.hpp>

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
    // Явное преобразование std::string_view в boost::json::string_view
    obj["code"] = boost::json::string_view(code.data(), code.size());
    obj["message"] = boost::json::string_view(message.data(), message.size());
    std::string body = boost::json::serialize(obj);
    return MakeJsonResponse(status, body, version, keep_alive);
}

}  // namespace http_handler