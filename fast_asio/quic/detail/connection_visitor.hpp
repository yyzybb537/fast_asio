#pragma once

#include "../../asio_include.h"
#include <net/quic/core/quic_connection.h>

namespace fast_asio {
namespace quic {
namespace detail {

class connection_visitor
    : public net::QuicConnectionDebugVisitor
{
public:
    explicit connection_visitor();
    virtual ~connection_visitor();

    void bind(QuicConnection * connection, std::recursive_mutex* mtx);

    void set_ack_timeout_secs(int secs);

    void CheckForNoAckTimeout();

    void SetNoAckAlarm();

    void CancelNoAckAlarm();

    // Called when a packet has been sent.
    void OnPacketSent(const SerializedPacket& serialized_packet,
            QuicPacketNumber original_packet_number,
            TransmissionType transmission_type,
            QuicTime sent_time) override;

    // Called when a PING frame has been sent.
    void OnPingSent() override;

    // Called when a AckFrame has been parsed.
    void OnAckFrame(const QuicAckFrame& frame) override;

private:
    // Called when a packet has been received, but before it is
    // validated or parsed.
    void OnPacketReceived(const QuicSocketAddress& self_address,
            const QuicSocketAddress& peer_address,
            const QuicEncryptedPacket& packet) override;

    // Called when the unauthenticated portion of the header has been parsed.
    void OnUnauthenticatedHeader(const QuicPacketHeader& header) override;

    // Called when a packet is received with a connection id that does not
    // match the ID of this connection.
    void OnIncorrectConnectionId(QuicConnectionId connection_id) override;

    // Called when an undecryptable packet has been received.
    void OnUndecryptablePacket() override;

    // Called when a duplicate packet has been received.
    void OnDuplicatePacket(QuicPacketNumber packet_number) override;

    // Called when the protocol version on the received packet doensn't match
    // current protocol version of the connection.
    void OnProtocolVersionMismatch(ParsedQuicVersion version) override;

    // Called when the complete header of a packet has been parsed.
    void OnPacketHeader(const QuicPacketHeader& header) override;

    // Called when a StreamFrame has been parsed.
    void OnStreamFrame(const QuicStreamFrame& frame) override;

    // Called when a StopWaitingFrame has been parsed.
    void OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override;

    // Called when a QuicPaddingFrame has been parsed.
    void OnPaddingFrame(const QuicPaddingFrame& frame) override;

    // Called when a Ping has been parsed.
    void OnPingFrame(const QuicPingFrame& frame) override;

    // Called when a GoAway has been parsed.
    void OnGoAwayFrame(const QuicGoAwayFrame& frame) override;

    // Called when a RstStreamFrame has been parsed.
    void OnRstStreamFrame(const QuicRstStreamFrame& frame) override;

    // Called when a ConnectionCloseFrame has been parsed.
    void OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame) override;

    // Called when a WindowUpdate has been parsed.
    void OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame,
            const QuicTime& receive_time) override;

    // Called when a BlockedFrame has been parsed.
    void OnBlockedFrame(const QuicBlockedFrame& frame) override;

    // Called when a public reset packet has been received.
    void OnPublicResetPacket(const QuicPublicResetPacket& packet) override;

    // Called when a version negotiation packet has been received.
    void OnVersionNegotiationPacket(
            const QuicVersionNegotiationPacket& packet) override;

    // Called when the connection is closed.
    void OnConnectionClosed(QuicErrorCode error,
            const QuicString& error_details,
            ConnectionCloseSource source) override;

    // Called when the version negotiation is successful.
    void OnSuccessfulVersionNegotiation(
            const ParsedQuicVersion& version) override;

    // Called when a CachedNetworkParameters is sent to the client.
    void OnSendConnectionState(
            const CachedNetworkParameters& cached_network_params) override;

    // Called when a CachedNetworkParameters are received from the client.
    void OnReceiveConnectionState(
            const CachedNetworkParameters& cached_network_params) override;

    // Called when the connection parameters are set from the supplied
    // |config|.
    void OnSetFromConfig(const QuicConfig& config) override;

    // Called when RTT may have changed, including when an RTT is read from
    // the config.
    void OnRttChanged(QuicTime::Delta rtt) const override;

private:
    QuicConnection* connection_ = nullptr;

    std::recursive_mutex *mtx_ = nullptr;

    int ack_timeout_secs_ = 0;

    std::unique_ptr<QuicAlarm> no_ack_alarm_;

    QuicTime last_ack_time_;
    QuicTime last_send_time_;
};

} // namespace detail
} // namespace quic
} // namespace fast_asio

#include "connection_visitor.ipp"
