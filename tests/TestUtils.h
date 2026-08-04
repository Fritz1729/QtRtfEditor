#pragma once

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <thread>
#include <type_traits>

#include <QDebug>

namespace Rte {

/**
 * @brief Run a callable with a timeout. Returns std::nullopt on timeout.
 * On timeout, the worker is detached (not joined) to avoid hanging when
 * the worker is deadlocked (e.g., Qt operations on non-main threads on Windows).
 * Uses shared_ptr to keep the promise alive after the worker is detached,
 * preventing std::terminate from an unfulfilled promise on process exit.
 */
template<typename F>
auto RunWithTimeout(F func, std::chrono::seconds timeout)
    -> std::optional<std::invoke_result_t<F>> {
    using R = std::invoke_result_t<F>;
    auto promise = std::make_shared<std::promise<R>>();
    std::future<R> future = promise->get_future();
    std::thread worker([f = std::move(func), p = std::move(promise)]() mutable {
        p->set_value(f());
    });
    if (future.wait_for(timeout) != std::future_status::ready) {
        promise->set_value(R{});
        worker.detach();
        return std::nullopt;
    }
    R result = future.get();
    worker.join();
    return result;
}

} // namespace Rte
