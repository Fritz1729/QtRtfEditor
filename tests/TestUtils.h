#pragma once

#include <chrono>
#include <future>
#include <optional>
#include <thread>
#include <type_traits>

#include <QDebug>

namespace Rte {

/**
 * @brief Run a callable with a timeout. Returns std::nullopt on timeout.
 * On timeout, the worker is joined to ensure clean termination.
 */
template<typename F>
auto RunWithTimeout(F func, std::chrono::seconds timeout)
    -> std::optional<std::invoke_result_t<F>> {
    using R = std::invoke_result_t<F>;
    std::promise<R> promise;
    std::future<R> future = promise.get_future();
    std::thread worker([f = std::move(func), p = std::move(promise)]() mutable {
        p.set_value(f());
    });
    qDebug() << "[timeout] Spawning worker, timeout=" << timeout.count() << "s";
    if (future.wait_for(timeout) != std::future_status::ready) {
        qDebug() << "[timeout] Timeout fired, joining worker";
        worker.join();
        return std::nullopt;
    }
    R result = future.get();
    worker.join();
    return result;
}

} // namespace Rte
