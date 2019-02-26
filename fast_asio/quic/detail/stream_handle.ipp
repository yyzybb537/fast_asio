#pragma once

namespace fast_asio {
namespace quic {
namespace detail {

inline void stream_handle::OnDataAvailable(QuartcStreamInterface* stream) override
{
    quic_socket_service & service = boost::asio::use_service<quic_socket_service>(ioc_);

    // TODO: notify service
}

inline void stream_handle::OnClose(QuartcStreamInterface* stream) override
{
    {
        std::unique_lock<std::recursive_mutex> lock(session_->mutex());
        closed_ = true;
        stream_ = nullptr;
    }

    quic_socket_service & service = boost::asio::use_service<quic_socket_service>(ioc_);
    // TODO: notify service
}

inline void stream_handle::OnCanWriteNewData(QuartcStreamInterface* stream) override
{
    quic_socket_service & service = boost::asio::use_service<quic_socket_service>(ioc_);
    // TODO: notify service
}

} // namespace detail
} // namespace quic
} // namespace fast_asio
