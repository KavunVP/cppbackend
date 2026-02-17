#ifdef WIN32
#include <sdkddkver.h>
#endif
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <thread>
#include <optional>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;
namespace beast = boost::beast;
namespace http = beast::http;

// Запрос, тело которого представлено в виде строки
using StringRequest = http::request<http::string_body>;
// Ответ, тело которого представлено в виде строки
using StringResponse = http::response<http::string_body>; 

std::optional<StringRequest> ReadRequest(tcp::socket& socket, beast::flat_buffer& buffer) {
    beast::error_code ec;
    StringRequest req;
    http::read(socket, buffer, req, ec);

    if (ec == http::error::end_of_stream) {
        return std::nullopt;
    }
    if (ec) {
        throw std::runtime_error("Failed to read request: "s.append(ec.message()));
    }
    return req;
}

void DumpRequest(const StringRequest& req) {
    std::cout << req.method_string() << ' ' << req.target() << std::endl;
    for (const auto& header : req) {
        std::cout << "  "sv << header.name_string() << ": "sv << header.value() << std::endl;
    }
}

// Структура ContentType задаёт область видимости для констант,
// задающих значения HTTP-заголовка Content-Type
struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
};

// Создаёт StringResponse с заданными параметрами
StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                  bool keep_alive,
                                  std::string_view content_type = ContentType::TEXT_HTML) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    return response;
}

StringResponse HandleRequest(StringRequest&& req) {
    // Обработка GET-запроса
    if (req.method() == http::verb::get) {
        std::string target(req.target());
        // Удаляем ведущий символ '/', если он есть
        if (!target.empty() && target[0] == '/') {
            target = target.substr(1);
        }
        std::string body = "Hello, " + target;
        return MakeStringResponse(http::status::ok, body, req.version(), req.keep_alive());
    }
    // Обработка HEAD-запроса
    else if (req.method() == http::verb::head) {
        std::string target(req.target());
        if (!target.empty() && target[0] == '/') {
            target = target.substr(1);
        }
        std::string body = "Hello, " + target;   // тело, которое вернулось бы в GET
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, ContentType::TEXT_HTML);
        response.content_length(body.size());    // выставляем длину, хотя тело пустое
        response.keep_alive(req.keep_alive());
        return response;
    }
    // Все остальные методы не разрешены
    else {
        StringResponse response(http::status::method_not_allowed, req.version());
        response.set(http::field::content_type, ContentType::TEXT_HTML);
        response.set(http::field::allow, "GET, HEAD");
        response.body() = "Invalid method";
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        return response;
    }
}

template <typename RequestHandler>
void HandleConnection(tcp::socket& socket, RequestHandler&& handle_request) {
    try {
        beast::flat_buffer buffer;

        while (auto request = ReadRequest(socket, buffer)) {
            DumpRequest(*request);
            StringResponse response = handle_request(*std::move(request));
            http::write(socket, response);
            if (response.need_eof()) {
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    beast::error_code ec;
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

int main() {
    net::io_context ioc;
    
    const auto address = net::ip::make_address("0.0.0.0");
    constexpr unsigned short port = 8080;

    tcp::acceptor acceptor(ioc, {address, port});
    std::cout << "Server has started..."sv << std::endl;

    while (true) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
        std::thread t(
            [](tcp::socket socket) {
                HandleConnection(socket, HandleRequest);
            },
            std::move(socket));
        t.detach();
    }
}