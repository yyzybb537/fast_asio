#pragma once
#include "asio_include.h"
#include <numeric>
#include "buffer_adapter.hpp"
#include "async_guard.hpp"
#include "packet_policy.hpp"
#include "error_code.hpp"

namespace fast_asio {

using namespace boost::asio;

template <typename NextLayer,
          typename PacketBuffer = streambuf>
class packet_read_stream
    : public packet_read_stream_base
    , public forward_close<packet_read_stream<NextLayer, PacketBuffer>, NextLayer>
    , public forward_shutdown<packet_read_stream<NextLayer, PacketBuffer>, NextLayer>
{
public:
    /// The type of the next layer.
    using next_layer_type = typename std::remove_reference<NextLayer>::type;

    /// The type of the lowest layer.
    using lowest_layer_type = typename next_layer_type::lowest_layer_type;

    /// The type of the executor associated with the object.
    using executor_type = typename next_layer_type::executor_type;

    using packet_buffer_type = PacketBuffer;

private:
    // next layer stream
    next_layer_type stream_;

    // receive buffer
    streambuf recv_buffer_;

    // is in async_read_some handler context
    volatile size_t handled_block_bytes_ = 0;

    // packet splitter
    packet_splitter packet_splitter_;

    option opt_;

    boost::system::error_code ec_;

    struct handler_context_scoped {
        packet_read_stream* self_;
        handler_context_scoped(packet_read_stream* self, size_t bytes) : self_(self) {
            self_->handled_block_bytes_ = bytes;
        }
        ~handler_context_scoped() {
            self_->handled_block_bytes_ = 0;
        }
    };

public:
    template <typename ... Args>
    explicit packet_read_stream(Args && ... args)
        : stream_(std::forward<Args>(args)...), packet_splitter_(&default_packet_policy::split)
    {
    }

    virtual ~packet_read_stream()
    {
    }

    void set_option(option const& opt) {
        opt_ = opt;
    }

    void set_packet_splitter(packet_splitter const& ps) {
        packet_splitter_ = ps;
    }

    io_context& get_io_context() {
        return lowest_layer().get_io_context();
    }

    next_layer_type & next_layer() {
        return stream_;
    }

    next_layer_type const& next_layer() const {
        return stream_;
    }

    lowest_layer_type & lowest_layer() {
        return stream_.lowest_layer();
    }

    lowest_layer_type const& lowest_layer() const {
        return stream_.lowest_layer();
    }

    /// ------------------- write_some
    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers, boost::system::error_code & ec)
    {
        return stream_.write_some(buffers, ec);
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers)
    {
        return stream_.write_some(buffers);
    }

    template <typename ConstBufferSequence, typename WriteHandler = cb_type>
        BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
            void (boost::system::error_code, std::size_t))
    async_write_some(const ConstBufferSequence& buffers,
            BOOST_ASIO_MOVE_ARG(WriteHandler) handler = nullptr)
    {
        stream_.async_write_some(buffers, std::forward<WriteHandler>(handler));
    }

    template <typename WriteHandler = cb_type>
        BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
            void (boost::system::error_code, std::size_t))
    async_write_some(PacketBuffer && buffer,
            BOOST_ASIO_MOVE_ARG(WriteHandler) handler = nullptr)
    {
        stream_.async_write_some(std::move(buffer), std::forward<WriteHandler>(handler));
    }

    /// ------------------- read_some
    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers) {
        boost::system::error_code ec;
        std::size_t bytes_transferred = read_some(buffers, ec);
        if (ec)
            BOOST_THROW_EXCEPTION(boost::system::system_error{ec});
        return bytes_transferred;
    }

    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers, boost::system::error_code & ec) {
        if (ec_) {
            ec = ec_;
            return 0;
        }

        size_t capacity = 0;
        for (auto it = buffer_sequence_begin(buffers); it != buffer_sequence_end(buffers); ++it) {
            capacity += it->size();
        }

        for (;;) {
            const_buffer buffers[128];
            const_buffer* buf_begin = &buffers[0];
            const_buffer* buf_end = buffers + 1;

            size_t bytes = peek_packets(buf_begin, buf_end, ec, capacity);
            if (bytes > 0) {
                ec = boost::system::error_code();
                boost::asio::buffer_copy(buffers, buffers_ref(buf_begin, buf_end));
                recv_buffer_.consume(bytes);
                return bytes;
            }

            if (ec) {
                if (!ec_) ec_ = ec;
                return 0;
            }

            std::size_t bytes_transferred = stream_.read_some(recv_buffer_.prepare(opt_.per_read_size), ec);
            if (ec) {
                return 0;
            }

            recv_buffer_.commit(bytes_transferred);
        }
    }

    template <typename ReadHandler>
        BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
            void(boost::system::error_code, const_buffer*, const_buffer*))
    async_read_some(BOOST_ASIO_MOVE_ARG(ReadHandler) handler)
    {
        if (ec_) {
            post_handler(handler, ec_);
            return ;
        }

        if (handled_block_bytes_) {
            post_async_write_some(handler);
            return ;
        }

        boost::system::error_code ec;
        const_buffer buffers[128];
        const_buffer* buf_begin = &buffers[0];
        const_buffer* buf_end = buffers + 1;

        size_t bytes = peek_packets(buf_begin, buf_end, ec);
        if (bytes > 0) {
            handler_context_scoped scoped(this, bytes);
            handler(boost::system::error_code(), buf_begin, buf_end);
            recv_buffer_.consume(bytes);
            return ;
        }

        if (ec) {
            if (!ec_) ec_ = ec;
            post_handler(handler, ec);
            return ;
        }

        read_reply(handler);
    }

private:
    template <typename ReadHandler>
    void post_async_write_some(ReadHandler && handler) {
        get_io_context().post([this, handler]{
                this->async_read_some(handler);
                });
    }

    template <typename ReadHandler>
    void read_reply(ReadHandler && handler) {
        stream_.async_read_some(recv_buffer_.prepare(opt_.per_read_size),
                [this, handler](boost::system::error_code ec, size_t bytes_transferred)
                {
                    if (ec) {
                        if (!ec_) ec_ = ec;
                        post_handler(handler, ec);
                        return ;
                    }

                    recv_buffer_.commit(bytes_transferred);
                    async_read_some(handler);
                });
    }

    size_t peek_packets(const_buffer* buf_begin, const_buffer*& buf_end,
            boost::system::error_code & ec,
            size_t max_size = std::numeric_limits<size_t>::max())
    {
        ec = boost::system::error_code();
        size_t bytes = 0;
        for (auto it = buf_begin; it != buf_end; ++it) {
            size_t packet_size = packet_splitter_((const char*)recv_buffer_.data().data() + bytes, recv_buffer_.size() - bytes);
            if (packet_size == (size_t)split_result::error) {
                ec = packet_parse_error();
                buf_end = it;
                return bytes;
            }

            if (packet_size == (size_t)split_result::not_enough_packet) {
                buf_end = it;
                return bytes;
            }

            if (bytes + packet_size > max_size) {
                buf_end = it;
                return bytes;
            }

            *it = const_buffer((const char*)recv_buffer_.data().data() + bytes, packet_size);
            bytes += packet_size;
        }

        return bytes;
    }

    void post_handler(read_handler const& handler, boost::system::error_code const& ec) {
        if (handler) {
            get_io_context().post([handler, ec]
                    {
                        const_buffer buf;
                        handler(ec, &buf, &buf);
                    });
        }
    }
};

} //namespace fast_asio
