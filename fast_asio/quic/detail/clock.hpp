#pragma once

#include "../../asio_include.h"
#include <net/quic/quartc/quartc_clock_interface.h>
#include <chrono>

namespace fast_asio {
namespace quic {
namespace detail {

using namespace net;

class clock
    : public QuartcClockInterface
{
public:
    static clock& getInstance() {
        static clock obj;
        return obj;
    }

    int64_t NowMicroseconds() override {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    }
};

} // namespace detail
} // namespace quic
} // namespace fast_asio

