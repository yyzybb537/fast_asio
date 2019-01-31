#pragma once
#include "packet_read_stream.hpp"
#include "packet_write_stream.hpp"

namespace fast_asio {

using namespace boost::asio;

template <typename NextLayer,
          typename PacketBuffer = streambuf>
class packet_stream
    : public packet_read_stream<packet_write_stream<NextLayer, PacketBuffer>, PacketBuffer>
{
public:
    /// The type of the next layer.
    using next_layer_type = typename std::remove_reference<NextLayer>::type;

    /// The type of the lowest layer.
    using lowest_layer_type = typename next_layer_type::lowest_layer_type;

    /// The type of the executor associated with the object.
    using executor_type = typename next_layer_type::executor_type;

    using packet_buffer_type = PacketBuffer;

    using write_stream_type = packet_write_stream<NextLayer, PacketBuffer>;

    using read_stream_type = packet_read_stream<write_stream_type, PacketBuffer>;

    using base_type = read_stream_type;

    struct option : public write_stream_type::option, public read_stream_type::option {};

public:
    template <typename ... Args>
    explicit packet_stream(Args && ... args)
        : base_type(std::forward<Args>(args)...)
    {
    }

    void set_option(option const& opt) {
        base_type::set_option(opt);
        base_type::next_layer().set_option(opt);
    }

    next_layer_type & next_layer() {
        return base_type::next_layer().next_layer();
    }

    next_layer_type const& next_layer() const {
        return base_type::next_layer().next_layer();
    }

    packet_write_stream<NextLayer, PacketBuffer> & get_packet_write_stream_layer() {
        return base_type::next_layer();
    }

    packet_write_stream<NextLayer, PacketBuffer> const& get_packet_write_stream_layer() const {
        return base_type::next_layer();
    }
};

} //namespace fast_asio
