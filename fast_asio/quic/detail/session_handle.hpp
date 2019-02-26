#pragma once

#include "../../asio_include.h"
#include <net/quic/quartc/quartc_session.h>
#include <net/quic/quartc/quartc_factory.h>
#include <net/quic/quartc/quartc_session_interface.h>
#include "task_runner_service.hpp"
#include "connection_visitor.hpp"
#include "clock.hpp"

namespace fast_asio {
namespace quic {
namespace detail {

using namespace net;

class session_handle;
typedef std::shared_ptr<session_handle> session_handle_ptr;

class session_handle
    : private QuartcSessionInterface::Delegate,
    private connection_visitor
{
    boost::asio::io_context & ioc_;

    std::recursive_mutex mtx_;

    // native quic session by libquic
    std::shared_ptr<QuartcSession> impl_;

    // shared udp socket
    std::shared_ptr<boost::asio::ip::udp::socket> udp_socket_;

    // transport by libquic
    packet_transport transport_;

    // new incoming streams
    std::list<QuartcStreamInterface*> incoming_streams_;

    // close reason
    int close_reason_error_code_ = 0;
    bool close_reason_from_remote_ = false;

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
            if (!impl_) return -1;

            QuicSocketAddress peer_address = impl_->peer_address();
            if (!peer_address.IsInitialized()) return -1;

            boost::asio::ip::udp::endpoint endpoint(
                    boost::asio::ip::make_address(peer_address.host().ToString()),
                    peer_address.port());
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

    QuartcStreamInterface* accept_stream()
    {
        std::unique_lock<std::recursive_mutex> lock(mtx_);
        if (incoming_streams_.empty()) return nullptr;
        QuartcStreamInterface* stream = incoming_streams_.front();
        incoming_streams_.pop_front();
        return stream;
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
};

} // namespace detail
} // namespace quic
} // namespace fast_asio

