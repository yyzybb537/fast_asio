#pragma once

#include "../../asio_include.h"
#include <net/quic/platform/api/quic_socket_address.h>

namespace fast_asio {
namespace quic {
namespace detail {

using namespace net;

QuicSocketAddress address_convert(boost::asio::ip::udp::endpoint endpoint) {
    QuicIpAddress addr;
    addr.FromString(endpoint.address().to_string());
    return QuicSocketAddress(addr, endpoint.port());
}

boost::asio::ip::udp::endpoint address_convert(QuicSocketAddress address) {
    return boost::asio::ip::udp::endpoint(
            boost::asio::ip::make_address(address.host().ToString()),
            address.port());
}


} // namespace detail
} // namespace quic
} // namespace fast_asio

