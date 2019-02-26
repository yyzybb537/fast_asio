#pragma once

#include "../asio_include.h"
#include "basic_multi_stream_socket.hpp"
#include "quic_socket_service.hpp"

namespace fast_asio {
namespace quic {

// Protocol: quic
class quic
{
public:
  typedef boost::asio::ip::udp::endpoint endpoint;

  /// Construct to represent the IPv4 UDP protocol.
  static quic v4()
  {
    return quic(BOOST_ASIO_OS_DEF(AF_INET));
  }

  /// Construct to represent the IPv6 UDP protocol.
  static quic v6()
  {
    return quic(BOOST_ASIO_OS_DEF(AF_INET6));
  }

  /// Obtain an identifier for the type of the protocol.
  int type() const
  {
    return BOOST_ASIO_OS_DEF(SOCK_DGRAM);
  }

  /// Obtain an identifier for the protocol.
  int protocol() const
  {
    return BOOST_ASIO_OS_DEF(IPPROTO_UDP);
  }

  /// Obtain an identifier for the protocol family.
  int family() const
  {
    return family_;
  }

  /// The Quic socket type.
  typedef basic_multi_stream_socket<quic, quic_socket_service<quic>> session;

  /// The UDP resolver type.
  typedef basic_resolver<boost::asio::ip::udp> resolver;

  typedef boost::asio::ip::udp::socket native_handle_type;

  typedef shared_ptr<native_handle_type> native_handle_ptr;

  /// Compare two protocols for equality.
  friend bool operator==(const quic& p1, const quic& p2)
  {
    return p1.family_ == p2.family_;
  }

  /// Compare two protocols for inequality.
  friend bool operator!=(const quic& p1, const quic& p2)
  {
    return p1.family_ != p2.family_;
  }

private:
  // Construct with a specific family.
  explicit quic(int protocol_family)
    : family_(protocol_family)
  {
  }

  int family_;
};

} // namespace quic
} // namespace fast_asio
