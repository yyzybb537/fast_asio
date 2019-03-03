#pragma once

namespace fast_asio {
namespace quic {
namespace detail {

inline void stream_handle::OnDataAvailable(QuartcStreamInterface* stream) override
{
    if (async_read_handler_) {
        async_read_handler_(true);
    }
}

inline void stream_handle::OnClose(QuartcStreamInterface* stream) override
{
    {
        std::unique_lock<std::recursive_mutex> lock(session_->mutex());
        closed_ = true;
        stream_ = nullptr;

        if (async_read_handler_) {
            async_read_handler_(true);
        }

        if (async_write_handler_) {
            async_write_handler_(true);
        }
    }
}

inline void stream_handle::OnCanWriteNewData(QuartcStreamInterface* stream) override
{
    if (async_write_handler_) {
        async_write_handler_(true);
    }
}

} // namespace detail
} // namespace quic
} // namespace fast_asio
