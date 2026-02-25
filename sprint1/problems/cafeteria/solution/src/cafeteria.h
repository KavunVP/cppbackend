#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <atomic>      // для счётчика ID

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;
namespace sys = boost::system;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

// Класс, управляющий асинхронным приготовлением одного хот-дога
class CookingSession : public std::enable_shared_from_this<CookingSession> {
public:
    CookingSession(net::io_context& io,
                   std::shared_ptr<GasCooker> cooker,
                   std::shared_ptr<Bread> bread,
                   std::shared_ptr<Sausage> sausage,
                   HotDogHandler handler,
                   int hotdog_id)
        : io_(io)
        , strand_(net::make_strand(io))
        , cooker_(std::move(cooker))
        , bread_(std::move(bread))
        , sausage_(std::move(sausage))
        , user_handler_(std::move(handler))
        , hotdog_id_(hotdog_id)
        , timer_bread_(io)
        , timer_sausage_(io) {
    }

    void Start() {
        net::dispatch(strand_, [self = shared_from_this()] {
            self->StartImpl();
        });
    }

private:
    void StartImpl() {
        try {
            bread_->StartBake(*cooker_, [self = shared_from_this()] {
                net::post(self->strand_, [self] { self->OnBreadStarted(); });
            });
            bread_invoked_ = true;
        } catch (...) {
            OnError(std::current_exception());
            return;
        }

        try {
            sausage_->StartFry(*cooker_, [self = shared_from_this()] {
                net::post(self->strand_, [self] { self->OnSausageStarted(); });
            });
            sausage_invoked_ = true;
        } catch (...) {
            OnError(std::current_exception());
            return;
        }
    }

    void OnBreadStarted() {
        if (error_) {
            // Уже произошла ошибка, освобождаем горелку и завершаем
            try { bread_->StopBaking(); } catch (...) {}
            return;
        }
        bread_started_ = true;
        timer_bread_.expires_after(HotDog::MIN_BREAD_COOK_DURATION);
        timer_bread_.async_wait([self = shared_from_this()](sys::error_code ec) {
            net::post(self->strand_, [self, ec] { self->OnBreadTimer(ec); });
        });
    }

    void OnSausageStarted() {
        if (error_) {
            try { sausage_->StopFry(); } catch (...) {}
            return;
        }
        sausage_started_ = true;
        timer_sausage_.expires_after(HotDog::MIN_SAUSAGE_COOK_DURATION);
        timer_sausage_.async_wait([self = shared_from_this()](sys::error_code ec) {
            net::post(self->strand_, [self, ec] { self->OnSausageTimer(ec); });
        });
    }

    void OnBreadTimer(sys::error_code ec) {
        if (ec == net::error::operation_aborted || error_) return;
        try {
            bread_->StopBaking();
        } catch (...) {
            OnError(std::current_exception());
            return;
        }
        bread_done_ = true;
        CheckDone();
    }

    void OnSausageTimer(sys::error_code ec) {
        if (ec == net::error::operation_aborted || error_) return;
        try {
            sausage_->StopFry();
        } catch (...) {
            OnError(std::current_exception());
            return;
        }
        sausage_done_ = true;
        CheckDone();
    }

    void CheckDone() {
        if (error_) return;
        if (bread_done_ && sausage_done_) {
            try {
                HotDog hd(hotdog_id_, sausage_, bread_);
                user_handler_(Result<HotDog>(std::move(hd)));
            } catch (...) {
                user_handler_(Result<HotDog>::FromCurrentException());
            }
            error_ = true;   // предотвращаем повторный вызов
        }
    }

    void OnError(std::exception_ptr ex) {
        if (error_) return;
        error_ = true;

        // Отменяем таймеры
        timer_bread_.cancel();
        timer_sausage_.cancel();

        // Освобождаем горелки, если ингредиенты уже начали готовиться, но ещё не закончили
        if (bread_started_ && !bread_done_) {
            try { bread_->StopBaking(); } catch (...) {}
        }
        if (sausage_started_ && !sausage_done_) {
            try { sausage_->StopFry(); } catch (...) {}
        }

        user_handler_(Result<HotDog>(ex));
    }

    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_;
    std::shared_ptr<GasCooker> cooker_;
    std::shared_ptr<Bread> bread_;
    std::shared_ptr<Sausage> sausage_;
    HotDogHandler user_handler_;
    int hotdog_id_;

    net::steady_timer timer_bread_;
    net::steady_timer timer_sausage_;

    bool bread_invoked_ = false;
    bool sausage_invoked_ = false;
    bool bread_started_ = false;
    bool sausage_started_ = false;
    bool bread_done_ = false;
    bool sausage_done_ = false;
    bool error_ = false;
};

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io}
        , strand_{net::make_strand(io)}   // добавляем strand
        , next_hotdog_id_{1} {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
        // Получение ингредиентов выполняется последовательно через strand
        net::dispatch(strand_, [this, handler = std::move(handler)]() mutable {
            auto bread = store_.GetBread();
            auto sausage = store_.GetSausage();
            int hotdog_id = next_hotdog_id_++;   // atomic, безопасно
            auto session = std::make_shared<CookingSession>(
                io_, gas_cooker_, std::move(bread), std::move(sausage), std::move(handler), hotdog_id
            );
            session->Start();   // запуск сессии (внутри использует свой strand)
        });
    }

private:
    net::io_context& io_;
    // Используется для создания ингредиентов хот-дога
    Store store_;
    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    // Используйте её для приготовления ингредиентов хот-дога.
    // Плита создаётся с помощью make_shared, так как GasCooker унаследован от
    // enable_shared_from_this.
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
    std::atomic<int> next_hotdog_id_;
    net::strand<net::io_context::executor_type> strand_;   // для синхронизации доступа к store_
};
