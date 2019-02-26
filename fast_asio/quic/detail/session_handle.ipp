#pragma once

namespace fast_asio {
namespace quic {
namespace detail {

inline void session_handle::OnCryptoHandshakeComplete() override
{
    quic_socket_service & service = boost::asio::use_service<quic_socket_service>(ioc_);
    // TODO: notify service
}

inline void session_handle::OnIncomingStream(QuartcStreamInterface* stream) override
{
    {
        std::unique_lock<std::recursive_mutex> lock(mtx_);
        incoming_streams_.push_back(stream);
    }

    quic_socket_service & service = boost::asio::use_service<quic_socket_service>(ioc_);
    // TODO: notify service
}

inline void session_handle::OnConnectionClosed(int error_code, bool from_remote) override
{
    quic_socket_service & service = boost::asio::use_service<quic_socket_service>(ioc_);
    // TODO: notify service
}

} // namespace detail
} // namespace quic
} // namespace fast_asio
