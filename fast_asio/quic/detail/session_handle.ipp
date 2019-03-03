#pragma once

namespace fast_asio {
namespace quic {
namespace detail {

inline void session_handle::OnSyn(QuicSocketAddress peer_address)
{
    peer_address_ = peer_address;
    impl_->OnTransportCanWrite();
    impl_->Initialize();
    syning_ = true;
    impl_->StartCryptoHandshake();
}

inline void session_handle::OnCryptoHandshakeComplete() override
{
    syning_ = false;
    quic_socket_service & service = boost::asio::use_service<quic_socket_service>(ioc_);

    if (impl_->perspective() == Perspective::IS_CLIENT) {
        peer_address_ = QuicSocketAddress();
        if (peer_address_.IsInitialized()) {
            connect_handler_(boost::system::error_code(), shared_from_this());
        }
    } else {
        service.on_accept(boost::system::error_code(), shared_from_this());
    }
}

inline void session_handle::OnIncomingStream(QuartcStreamInterface* stream) override
{
    std::unique_lock<std::recursive_mutex> lock(mtx_);
    if (!async_accept_handlers_.empty()) {
        auto handler = async_accept_handlers_.front();
        async_accept_handlers_.pop_front();
        handler(shared_from_this(), stream);
        return ;
    }

    incoming_streams_.push_back(stream);
}

inline void session_handle::OnConnectionClosed(int error_code, bool from_remote) override
{
    quic_socket_service & service = boost::asio::use_service<quic_socket_service>(ioc_);

    // TODO: init quic error category
    boost::system::error_code ec = ;
    if (peer_address_.IsInitialized()) {
        connect_handler_(ec, shared_from_this());
    } else if (syning_) {
        service.on_syn_error(ec, shared_from_this());
    }

    service.close(ec, shared_from_this());
}

} // namespace detail
} // namespace quic
} // namespace fast_asio
