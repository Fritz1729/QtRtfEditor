#pragma once

#include <chrono>
#include <future>
#include <optional>
#include <thread>
#include <type_traits>

namespace Rte {

/**
 * @brief Run a callable with a timeout. Returns std::nullopt on timeout.
 * The worker thread is joined; if timeout fires, the worker continues
 * until completion before join returns (safe across CRT boundaries).
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
    if (future.wait_for(timeout) != std::future_status::ready) {
        worker.join();
        return std::nullopt;
    }
    R result = future.get();
    worker.join();
    return result;
}

} // namespace Rte
