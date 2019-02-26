#include "clock.hpp"
#include <net/quic/core/quic_alarm.h>

namespace fast_asio {
namespace quic {
namespace detail {

class NoAckAlarmDelegate : public QuicAlarm::Delegate {
public:
    explicit NoAckAlarmDelegate(connection_visitor *visitor)
        : visitor_(visitor) {}

    void OnAlarm() override {
        visitor_->CheckForNoAckTimeout();
    }

private:
    connection_visitor* visitor_;

    DISALLOW_COPY_AND_ASSIGN(NoAckAlarmDelegate);
};

connection_visitor::connection_visitor()
    : last_ack_time_(QuicTime::Zero()), last_send_time_(QuicTime::Zero())
{
}
connection_visitor::~connection_visitor()
{
}

void connection_visitor::CheckForNoAckTimeout()
{
    std::unique_lock<std::recursive_mutex> lock(mtx_);

    QuicTime::Delta allow_duration = QuicTime::Delta::FromSeconds(ack_timeout_secs_);
    if (allow_duration.IsZero())
        return ;

    if (last_send_time_ > last_ack_time_) {
        QuicTime now = QuicClockImpl::getInstance().Now();
        QuicTime::Delta ack_duration = now - last_send_time_;
        if (ack_duration > allow_duration) {
//            DebugPrint(dbg_close | dbg_ack_timeout, 
//                    "CloseConnection by ack timeout. fd = %d. now = %ld, lastAck = %ld, lastSend = %ld",
//                    parent_->Fd(),
//                    (long)(now - QuicTime::Zero()).ToMilliseconds(),
//                    (long)(last_ack_time_ - QuicTime::Zero()).ToMilliseconds(),
//                    (long)(last_send_time_ - QuicTime::Zero()).ToMilliseconds());
            connection_->CloseConnection(net::QUIC_NETWORK_ACK_TIMEOUT, "ack timeout",
                      net::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET_WITH_NO_ACK);
            return ;
        }
    }

    SetNoAckAlarm();
}

void connection_visitor::SetNoAckAlarm()
{
    QuicTime::Delta allow_duration = QuicTime::Delta::FromSeconds(ack_timeout_secs_);
    if (allow_duration.IsZero())
        return ;

    QuicTime timeOfLast = last_send_time_ > last_ack_time_ ? last_send_time_ : QuicClockImpl::getInstance().Now();
    QuicTime deadline = timeOfLast + allow_duration;

//    DebugPrint(dbg_ack_timeout, "fd = %d, secs = %d", parent_->Fd(), secs);
    no_ack_alarm_->Update(deadline, QuicTime::Delta::Zero());
}

void connection_visitor::CancelNoAckAlarm()
{
    no_ack_alarm_->Cancel();
}

void connection_visitor::bind(QuicConnection * connection, std::recursive_mutex* mtx)
{
    connection_ = connection;
    mtx_ = mtx;
    no_ack_alarm_.reset(connection_->alarm_factory()->CreateAlarm(new NoAckAlarmDelegate(this)));
}

void connection_visitor::set_ack_timeout_secs(int secs)
{
    std::unique_lock<std::recursive_mutex> lock(mtx_);
    ack_timeout_secs_ = secs;
    SetNoAckAlarm();
}

// Called when a packet has been sent.
void connection_visitor::OnPacketSent(const SerializedPacket& serialized_packet,
        QuicPacketNumber original_packet_number,
        TransmissionType transmission_type,
        QuicTime sent_time)
{
//    char frameTypes[(int)net::QuicFrameType::NUM_FRAME_TYPES + 1] = {};
//    memset(frameTypes, '-', sizeof(frameTypes) - 1);
//    for (auto const& frame : serialized_packet.retransmittable_frames) {
//        int type = (int)frame.type;
//        frameTypes[type] = '0' + type;
//    }
//    DebugPrint(dbg_conn_visitor | dbg_ack_timeout, "Visitor sent fd = %d, len=%d, has_ack=%d, transmission_type=%d, frames=%s",
//            parent_->Fd(), (int)serialized_packet.encrypted_length,
//            serialized_packet.has_ack, (int)transmission_type, frameTypes);

    if (TransmissionType::NOT_RETRANSMISSION == transmission_type) {
        if (last_send_time_ <= last_ack_time_)
            last_send_time_ = QuicClockImpl::getInstance().Now();
    }
}

// Called when a AckFrame has been parsed.
void connection_visitor::OnAckFrame(const QuicAckFrame& frame)
{
//    DebugPrint(dbg_conn_visitor | dbg_ack_timeout, "Visitor ack fd = %d", parent_->Fd());

    last_ack_time_ = QuicClockImpl::getInstance().Now();
}

// Called when a PING frame has been sent.
void connection_visitor::OnPingSent()
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a packet has been received, but before it is
// validated or parsed.
void connection_visitor::OnPacketReceived(const QuicSocketAddress& self_address,
        const QuicSocketAddress& peer_address,
        const QuicEncryptedPacket& packet)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when the unauthenticated portion of the header has been parsed.
void connection_visitor::OnUnauthenticatedHeader(const QuicPacketHeader& header)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a packet is received with a connection id that does not
// match the ID of this connection.
void connection_visitor::OnIncorrectConnectionId(QuicConnectionId connection_id)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when an undecryptable packet has been received.
void connection_visitor::OnUndecryptablePacket()
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a duplicate packet has been received.
void connection_visitor::OnDuplicatePacket(QuicPacketNumber packet_number)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when the protocol version on the received packet doensn't match
// current protocol version of the connection.
void connection_visitor::OnProtocolVersionMismatch(ParsedQuicVersion version)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when the complete header of a packet has been parsed.
void connection_visitor::OnPacketHeader(const QuicPacketHeader& header)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a StreamFrame has been parsed.
void connection_visitor::OnStreamFrame(const QuicStreamFrame& frame)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a StopWaitingFrame has been parsed.
void connection_visitor::OnStopWaitingFrame(const QuicStopWaitingFrame& frame)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a QuicPaddingFrame has been parsed.
void connection_visitor::OnPaddingFrame(const QuicPaddingFrame& frame)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a Ping has been parsed.
void connection_visitor::OnPingFrame(const QuicPingFrame& frame)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a GoAway has been parsed.
void connection_visitor::OnGoAwayFrame(const QuicGoAwayFrame& frame)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a RstStreamFrame has been parsed.
void connection_visitor::OnRstStreamFrame(const QuicRstStreamFrame& frame)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a ConnectionCloseFrame has been parsed.
void connection_visitor::OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a WindowUpdate has been parsed.
void connection_visitor::OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame,
        const QuicTime& receive_time)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a BlockedFrame has been parsed.
void connection_visitor::OnBlockedFrame(const QuicBlockedFrame& frame)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a public reset packet has been received.
void connection_visitor::OnPublicResetPacket(const QuicPublicResetPacket& packet)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a version negotiation packet has been received.
void connection_visitor::OnVersionNegotiationPacket(
        const QuicVersionNegotiationPacket& packet)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when the connection is closed.
void connection_visitor::OnConnectionClosed(QuicErrorCode error,
        const QuicString& error_details,
        ConnectionCloseSource source)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when the version negotiation is successful.
void connection_visitor::OnSuccessfulVersionNegotiation(
        const ParsedQuicVersion& version)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a CachedNetworkParameters is sent to the client.
void connection_visitor::OnSendConnectionState(
        const CachedNetworkParameters& cached_network_params)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when a CachedNetworkParameters are received from the client.
void connection_visitor::OnReceiveConnectionState(
        const CachedNetworkParameters& cached_network_params)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when the connection parameters are set from the supplied
// |config|.
void connection_visitor::OnSetFromConfig(const QuicConfig& config)
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

// Called when RTT may have changed, including when an RTT is read from
// the config.
void connection_visitor::OnRttChanged(QuicTime::Delta rtt) const
{
//    DebugPrint(dbg_conn_visitor, "Visitor fd = %d", parent_->Fd());
}

} // namespace detail
} // namespace quic
} // namespace fast_asio
