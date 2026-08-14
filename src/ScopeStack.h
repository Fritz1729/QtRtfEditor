#pragma once

#include <utility>
#include <vector>

namespace Rte {

/**
 * @brief A value with a stack of outer values, for push/pop scope tracking.
 *
 * @tparam T The value type (must be move-constructible).
 */
template<typename T>
struct ScopeStack {
    std::vector<T> stack;
    T current;

    ScopeStack() = default;
    explicit ScopeStack(T initial) : current(std::move(initial)) {}

    void enterScope() { stack.push_back(current); }
    void leaveScope() {
        if (!stack.empty()) {
            current = std::move(stack.back());
            stack.pop_back();
        }
    }
    const T& get() const { return current; }
    T& get() { return current; }
};

} // namespace Rte
