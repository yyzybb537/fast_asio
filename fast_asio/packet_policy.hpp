#pragma once
#include "asio_include.h"
#include <string>
#include <cstddef>
#include "buffer_adapter.hpp"
#include "packet_read_stream_base.hpp"

namespace fast_asio {

struct default_packet_policy
{
    static size_t split(const char* buf, size_t len) {
        if (len < 4)
            return (size_t)packet_read_stream_base::split_result::not_enough_packet;

        uint32_t packet_size = ntohl(*reinterpret_cast<const uint32_t*>(buf));
        if (len < (size_t)packet_size)
            return (size_t)packet_read_stream_base::split_result::not_enough_packet;

        return packet_size;
    }

    template <typename StreamBuf, typename ConstBuffer>
    static StreamBuf && serialize(ConstBuffer const& buf) {
        StreamBuf sb;
        uint32_t len = buf.size() + sizeof(uint32_t);

        char* out = reinterpret_cast<char*>(buffer_adapter<StreamBuf>::prepare(sb, len).data());
        *reinterpret_cast<uint32_t*>(out) = htonl(len);
        out += sizeof(uint32_t);
        ::memcpy(out, buf.data(), buf.size());
        return std::move(sb);
    }

    template <typename ConstBuffer>
    static std::string serialize_to_string(ConstBuffer const& buf) {
        std::string s;
        uint32_t len = buf.size() + sizeof(uint32_t);
        s.resize(len);

        char* out = &s[0];
        *reinterpret_cast<uint32_t*>(out) = htonl(len);
        out += sizeof(uint32_t);
        ::memcpy(out, buf.data(), buf.size());
        return std::move(s);
    }

    static const_buffer get_body(const_buffer buf) {
        return (buf += sizeof(uint32_t));
    }
};

} //namespace fast_asio
