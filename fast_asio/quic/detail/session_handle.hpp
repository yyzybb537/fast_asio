#pragma once

#include "../../asio_include.h"
#include <net/quic/quartc/quartc_session.h>
#include <net/quic/quartc/quartc_factory.h>
#include <net/quic/quartc/quartc_session_interface.h>
#include "task_runner_service.hpp"
#include "connection_visitor.hpp"
#include "clock.hpp"
#include "common.hpp"

namespace fast_asio {
namespace quic {
namespace detail {

using namespace net;

class session_handle;
typedef std::shared_ptr<session_handle> session_handle_ptr;

class session_handle
    : private QuartcSessionInterface::Delegate,
    private connection_visitor,
    public std::enable_shared_from_this<session_handle>
{
    boost::asio::io_context & ioc_;

    std::recursive_mutex mtx_;

    QuicSocketAddress peer_address_;
    std::function<void(boost::system::error_code, session_handle_ptr)> connect_handler_;

    // native quic session by libquic
    std::shared_ptr<QuartcSession> impl_;

    // shared udp socket
    std::shared_ptr<boost::asio::ip::udp::socket> udp_socket_;

    // transport by libquic
    packet_transport transport_;

    // new incoming streams
    std::list<QuartcStreamInterface*> incoming_streams_;

    typedef std::function<void(session_handle_ptr, QuartcStreamInterface*)> async_accept_handler;
    std::list<async_accept_handler> async_accept_handlers_;

    // close reason
    int close_reason_error_code_ = 0;
    bool close_reason_from_remote_ = false;

    bool syning_ = false;

    class packet_transport
       : public QuartcSessionInterface::PacketTransport
    {
        session_handle* session_;

    public:
        void bind(session_handle* session)
        {
            session_ = session;
        }

        // Called by the QuartcPacketWriter when writing packets to the network.
        // Return the number of written bytes. Return 0 if the write is blocked.
        int Write(const char* buffer, size_t buf_len) override
        {
            if (!session_->udp_socket_) return -1;
            if (!session_->impl_) return -1;

            QuicSocketAddress peer_address = session_->peer_address();
            if (!peer_address.IsInitialized()) return -1;

            boost::asio::ip::udp::endpoint endpoint(address_convert(peer_address));
            return session_->udp_socket_->send_to(boost::asio::buffer(buffer, buf_len), endpoint);
        }
    };

public:
    session_handle(boost::asio::io_context & ioc, bool isServer, QuicConnectionId id)
        : ioc_(ioc)
    {
        if (id == INVALID_QUIC_CONNECTION_ID)
            id = QuicRandom::GetInstance()->RandUint64();

        transport_.bind(this);

        QuartcFactory factory(
                QuartcFactoryConfig{&boost::asio::use_service<task_runner_service>(ioc),
                &clock::getInstance()});

        QuartcFactoryInterface::QuartcSessionConfig config;
        config.is_server = isServer;
        config.unique_remote_server_id = "";
        config.packet_transport = transport_;
        config.congestion_control = QuartcCongestionControl::kBBR;
        config.connection_id = id;
        config.max_idle_time_before_crypto_handshake_secs = 10;
        config.max_time_before_crypto_handshake_secs = 10;
        impl_.reset(factory.CreateQuartcSession(config).release());

        impl_->SetDelegate(this);
        connection_visitor::bind(impl_->connection(), &mtx_);
        connection_visitor::set_ack_timeout_secs(24);
        impl_->set_debug_visitor(this);
    }

    void set_native_socket(std::shared_ptr<boost::asio::ip::udp::socket> udp_socket) {
        udp_socket_ = udp_socket;
    }

    std::shared_ptr<boost::asio::ip::udp::socket> native_socket() {
        return udp_socket_;
    }

    void ProcessUdpPacket(const QuicSocketAddress& self_address,
            const QuicSocketAddress& peer_address,
            const QuicReceivedPacket& packet)
    {
        std::unique_lock<std::recursive_mutex> lock(mtx_);
        impl_->FlushWrites();
        impl_->ProcessUdpPacket(self_address, peer_address, packet);
    }

    // ----------------- QuartcSessionInterface::Delegate
    void OnCryptoHandshakeComplete() override;

    void OnIncomingStream(QuartcStreamInterface* stream) override;

    void OnConnectionClosed(int error_code, bool from_remote) override;
    // --------------------------------------------------

    void OnSyn(QuicSocketAddress peer_address);

    void async_accept(async_accept_handler const& handler)
    {
        std::unique_lock<std::recursive_mutex> lock(mtx_);
        if (!incoming_streams_.empty()) {
            QuartcStreamInterface* stream = incoming_streams_.front();
            incoming_streams_.pop_front();
            handler(shared_from_this(), stream);
            return ;
        }

        async_accept_handlers_.push_back(handler);
    }

    QuartcStreamInterface* create_stream()
    {
        std::unique_lock<std::recursive_mutex> lock(mtx_);
        return impl_->CreateOutgoingDynamicStream();
    }

    void close()
    {
        std::unique_lock<std::recursive_mutex> lock(mtx_);
        impl_->CloseConnection("close");
    }

    std::size_t available(boost::system::error_code& ec) const
    {
        std::unique_lock<std::recursive_mutex> lock(mtx_);
        return incoming_streams_.size();
    }

    std::recursive_mutex & mutex() {
        return mtx_;
    }

    boost::asio::io_context& get_io_context() {
        return ioc_;
    }

    boost::asio::io_context const& get_io_context() const {
        return ioc_;
    }

    boost::asio::ip::udp::endpoint remote_endpoint() {
        return address_convert(peer_address());
    }

    boost::asio::ip::udp::endpoint local_endpoint() {
        boost::system::error_code ignore_ec;
        if (!native_socket()) return boost::asio::ip::udp::endpoint();
        return native_socket()->local_endpoint(ignore_ec);
    }

    QuicSocketAddress peer_address() {
        QuicSocketAddress address = impl_->peer_address();
        if (!address.IsInitialized()) {
            address = peer_address_;
        }
        return address;
    }

    template <typename ConnectHandler>
        BOOST_ASIO_INITFN_RESULT_TYPE(ConnectHandler,
                void (boost::system::error_code, session_handle_ptr))
        async_connect(const endpoint_type& peer_endpoint,
                BOOST_ASIO_MOVE_ARG(ConnectHandler) handler)
        {
            if (peer_address_.IsInitialized()) {
                session_handle_ptr self = shared_from_this();
                get_io_context().post([=]{ handler(boost::asio::error::in_progress, self); });
                return ;
            }

            peer_address_ = address_convert(peer_endpoint);
            connect_handler_ = handler;
            impl_->OnTransportCanWrite();
            impl_->Initialize();
            syning_ = true;
            impl_->StartCryptoHandshake();
        }
};

} // namespace detail
} // namespace quic
} // namespace fast_asio

