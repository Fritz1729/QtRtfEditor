#pragma once

#include <chrono>
#include <future>
#include <optional>
#include <thread>
#include <type_traits>

namespace Rte {

/**
 * @brief Run a callable with a timeout. Returns std::nullopt on timeout.
 * The worker thread is detached; if timeout fires, it continues in the background.
 */
template<typename F>
auto RunWithTimeout(F func, std::chrono::seconds timeout)
    -> std::optional<std::invoke_result_t<F>> {
    using R = std::invoke_result_t<F>;
    std::promise<R> promise;
    std::future<R> future = promise.get_future();
    std::thread t([f = std::move(func), p = std::move(promise)]() mutable {
        p.set_value(f());
    });
    t.detach();
    if (future.wait_for(timeout) != std::future_status::ready)
        return std::nullopt;
    return future.get();
}

} // namespace Rte
