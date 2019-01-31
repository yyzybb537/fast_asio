#pragma once
#include <boost/asio/buffer.hpp>
#include <string>
#include <cstddef>
#include "buffer_adapter.hpp"

namespace fast_asio {

struct default_packet_policy
{
    static size_t split(const char* buf, size_t len) {
        if (len < 4)
            return (size_t)split_result::not_enough_packet;

        uint32_t packet_size = ntohl(*reinterpret_cast<uint32_t*>(buf));
        if (len < (size_t)packet_size)
            return (size_t)split_result::not_enough_packet;

        return packet_size;
    }

    template <typename StreamBuf, typename ConstBuffer>
    StreamBuf && serialize(ConstBuffer const& buf) {
        StreamBuf sb;
        uint32_t len = buf.size() + sizeof(uint32_t);

        char* out = buffer_adapter<StreamBuf>::prepare(sb, len);
        *reinterpret_cast<uint32_t*>(out) = htonl(len);
        out += sizeof(uint32_t);
        ::memcpy(out, buf.data(), buf.size());
        return std::move(sb);
    }
};

} //namespace fast_asio
