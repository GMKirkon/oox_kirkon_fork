// Copyright (C) 2026 OOX contributors
//
// SPDX-License-Identifier: Apache-2.0
//
// Cooperative cancellation token/source used by every cross-framework branch
// search benchmark — oneTBB Flow Graph, Taskflow, std::execution and futures.
// Branch-agnostic and library-agnostic by design.

#ifndef OOX_BENCH_BASELINE_CANCELLATION_HPP
#define OOX_BENCH_BASELINE_CANCELLATION_HPP

#include <atomic>
#include <memory>
#include <utility>

namespace bench_base {

struct cancellation_state {
    std::atomic<bool> requested{false};
};

class cancellation_token {
public:
    cancellation_token() = default;

    explicit cancellation_token(std::shared_ptr<cancellation_state> state)
        : state_(std::move(state)) {}

    bool stop_requested() const noexcept {
        return state_ && state_->requested.load(std::memory_order_relaxed);
    }

private:
    std::shared_ptr<cancellation_state> state_;
};

class cancellation_source {
public:
    cancellation_source()
        : state_(std::make_shared<cancellation_state>()) {}

    cancellation_token token() const noexcept {
        return cancellation_token{state_};
    }

    bool request_cancel() noexcept {
        bool expected = false;
        return state_->requested.compare_exchange_strong(
            expected, true,
            std::memory_order_relaxed,
            std::memory_order_relaxed);
    }

    bool stop_requested() const noexcept {
        return state_->requested.load(std::memory_order_relaxed);
    }

private:
    std::shared_ptr<cancellation_state> state_;
};

struct branch_config {
    std::size_t branch_count;
    std::size_t leaves_per_branch;
    std::size_t winner_branch;
    std::size_t winner_leaf;
    std::size_t work_iterations;
    std::size_t poll_interval;
};

} // namespace bench_base

#endif // OOX_BENCH_BASELINE_CANCELLATION_HPP
