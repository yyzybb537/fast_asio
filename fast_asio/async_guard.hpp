#pragma once
#include <mutex>
#include <memory>

namespace fast_asio {

// 保护异步回调的对象生命期
class async_guard;
typedef std::shared_ptr<async_guard> async_guard_ptr;

class async_guard
{
    std::mutex mtx_;
    bool destroyed_;

public:
    async_guard() : destroyed_(false) {}

    std::mutex& mutex() {
        return mtx_;
    }

    void cancel() {
        std::unique_lock<std::mutex> lock(mtx_);
        destroyed_ = true;
    }

    bool canceled() {
        return destroyed_;
    }

    static async_guard_ptr create() {
        return std::make_shared<async_guard>();
    }
};

class async_scoped
{
    async_guard_ptr const& async_guard_;

public:
    async_scoped(async_guard_ptr const& guard) : async_guard_(guard) {
        async_guard_->mutex().lock();
    }

    ~async_scoped() {
        async_guard_->mutex().unlock();
    }

    explicit operator bool() {
        return !async_guard_->canceled();
    }
};

} //namespace fast_asio
