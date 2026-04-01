#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/program_options.hpp>
#include <boost/system/error_code.hpp>
#include <iostream>
#include <thread>
#include <optional>
#include <functional>
#include <chrono>

#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"
#include "logging.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace po = boost::program_options;

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

// Класс Ticker для периодического вызова функции в strand
class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    // Функция handler будет вызываться внутри strand с интервалом period
    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , timer_{strand}
        , handler_{std::move(handler)} {
    }

    void Start() {
        last_tick_ = Clock::now();
        net::dispatch(strand_, [self = shared_from_this()] {
            self->ScheduleTick();
        });
    }

private:
    void ScheduleTick() {
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnTick(ec);
        });
    }

    void OnTick(sys::error_code ec) {
        using namespace std::chrono;
        if (!ec) {
            auto this_tick = Clock::now();
            auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
            last_tick_ = this_tick;
            try {
                handler_(delta);
            } catch (const std::exception&) {
            }
            ScheduleTick();
        }
    }

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_;
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};

// Структура для хранения аргументов командной строки
struct Args {
    std::optional<std::chrono::milliseconds> tick_period;
    std::filesystem::path config_file;
    std::filesystem::path www_root;
    bool randomize_spawn_points = false;
};

// Парсинг командной строки
[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    po::options_description desc{"Allowed options"};

    Args args;
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<unsigned long>(), "set tick period in milliseconds")
        ("config-file,c", po::value(&args.config_file)->value_name("file"), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points), "spawn dogs at random positions");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help")) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }

    // Проверяем наличие обязательных опций
    if (!vm.contains("config-file")) {
        throw std::runtime_error("Config file path is not specified");
    }
    if (!vm.contains("www-root")) {
        throw std::runtime_error("WWW root directory is not specified");
    }

    // Обрабатываем tick-period, если указан
    if (vm.contains("tick-period")) {
        args.tick_period = std::chrono::milliseconds{vm["tick-period"].as<unsigned long>()};
    }

    return args;
}

}  // namespace

int main(int argc, const char* argv[]) {
    try {
        // Парсим командную строку
        auto args = ParseCommandLine(argc, argv);
        if (!args) {
            // Был показан help или ошибка
            return EXIT_SUCCESS;
        }

        // Инициализация логгера
        logging::InitBoostLog();

        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(args->config_file);

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
        http_handler::StaticHandler static_handler{args->www_root};
        auto api_handler = std::make_shared<http_handler::ApiHandler>(
            game, api_strand, args->tick_period, args->randomize_spawn_points);

        // 6. Если указан tick-period, запускаем Ticker для автоматического обновления времени
        std::shared_ptr<Ticker> ticker;
        if (args->tick_period) {
            ticker = std::make_shared<Ticker>(
                api_strand, *args->tick_period,
                [api_handler](std::chrono::milliseconds delta) {
                    api_handler->Tick(delta);
                });
            ticker->Start();
        }

        // 7. Создаём комбинированный обработчик, который маршрутизирует запросы
        auto combined_handler = [static_handler = std::move(static_handler),
                                 api_handler](
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

        // 8. Запустить обработчик HTTP-запросов
        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;
        http_server::ServeHttp(ioc, {address, port}, std::move(handler));

        // 9. Запускаем обработку асинхронных операций
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

    return EXIT_SUCCESS;
}
