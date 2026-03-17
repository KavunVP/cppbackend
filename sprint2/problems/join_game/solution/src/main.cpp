#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/system/error_code.hpp>
#include <iostream>
#include <thread>

#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"
#include "logging.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    //Ожидаем три аргумента
    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> <static-dir>"sv << std::endl;
        return EXIT_FAILURE;
    }
    try {
        // Инициализация логгера
        logging::InitBoostLog();

        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(argv[1]);

        // Логирование запуска сервера
        boost::json::object start_data;
        start_data["port"] = 8080;
        start_data["address"] = "0.0.0.0";
        logging::LogMessage("server started", start_data);

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, int) {
            if (!ec) {
                ioc.stop();
            }
        });

        // 4. Создаём strand для защиты API запросов
        auto api_strand = net::make_strand(ioc);

        // 5. Создаём обработчики
        http_handler::StaticHandler static_handler{argv[2]};
        auto api_handler = std::make_shared<http_handler::ApiHandler>(game, api_strand);

        // 6. Создаём комбинированный обработчик, который маршрутизирует запросы
        auto combined_handler = [static_handler = std::move(static_handler),
                                 api_handler = std::move(api_handler)](
                                    auto&& req, const std::string& remote_ip, auto&& send) mutable {
            std::string_view target = req.target();
            auto pos = target.find('?');
            if (pos != std::string_view::npos) {
                target = target.substr(0, pos);
            }

            // Если запрос начинается с /api/, отправляем в API handler
            if (target.starts_with("/api/")) {
                (*api_handler)(std::forward<decltype(req)>(req), remote_ip,
                               std::forward<decltype(send)>(send));
            } else {
                // Иначе обрабатываем как статический запрос
                static_handler(std::forward<decltype(req)>(req), remote_ip,
                               std::forward<decltype(send)>(send));
            }
        };

        // Оборачиваем в декоратор с логированием
        auto handler = std::make_shared<http_handler::LoggingRequestHandler<decltype(combined_handler)>>(
            std::move(combined_handler));

        // 7. Запустить обработчик HTTP-запросов
        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;
        http_server::ServeHttp(ioc, {address, port}, std::move(handler));

        // 8. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

        // Логирование успешного завершения
        boost::json::object exit_data;
        exit_data["code"] = 0;
        logging::LogMessage("server exited", exit_data);

    } catch (const std::exception& ex) {
        boost::json::object err_data;
        err_data["code"] = EXIT_FAILURE;
        err_data["exception"] = ex.what();
        logging::LogMessage("server exited", err_data);
        return EXIT_FAILURE;
    }
}
