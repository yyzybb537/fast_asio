#pragma once
#include <boost/asio.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/system/error_code.hpp>
#include <numeric>
#include "buffer_adapter.hpp"
#include "async_guard.hpp"
#include "packet_policy.hpp"

namespace fast_asio {

using namespace boost::asio;

template <typename NextLayer,
          typename PacketBuffer = streambuf>
class packet_read_stream
{
public:
    /// The type of the next layer.
    using next_layer_type = typename std::remove_reference<NextLayer>::type;

    /// The type of the lowest layer.
    using lowest_layer_type = get_lowest_layer<next_layer_type>;

    /// The type of the executor associated with the object.
    using executor_type = typename next_layer_type::executor_type;

    typedef void (*cb_type)(boost::system::error_code, std::size_t);

    using read_handler = std::function<void(boost::system::error_code const& ec, const_buffer* buf_begin, const_buffer* buf_end)>;

    enum class split_result : size_t { error = (size_t)-1, not_enough_packet = 0 };

    using packet_splitter = std::function<size_t(const char* buf, size_t len)>;

    struct option {
        // max allow packet size
        size_t max_packet_size = 64 * 1024;

        // size of per read op
        size_t per_read_size = 16 * 1024;
    };

private:
    // next layer stream
    next_layer_type stream_;

    // receive buffer
    streambuf recv_buffer_;

    // packet splitter
    packet_splitter packet_splitter_;

    option opt_;

    boost::system::error_code ec_;

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

    void close() {
        stream_.close();
    }

    void close(boost::system::error_code & ec) {
        stream_.close(ec);
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
                boost::asio::buffer_copy(buffers, std::make_pair(buf_begin, buf_end));
//                boost::asio::buffer_copy(detail::multiple_buffers(),
//                        detail::multiple_buffers(),
//                        buffer_sequence_begin(buffers),
//                        buffer_sequence_end(buffers),
//                        buf_begin, buf_end);
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

    template <typename MutableBufferSequence, typename ReadHandler>
        BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
            void(boost::system::error_code, const_buffer))
    async_read_some(BOOST_ASIO_MOVE_ARG(ReadHandler) handler)
    {
        if (ec_) {
            post_handler(handler, ec_);
            return ;
        }

        boost::system::error_code ec;
        const_buffer buffers[128];
        const_buffer* buf_begin = &buffers[0];
        const_buffer* buf_end = buffers + 1;

        size_t bytes = peek_packets(buf_begin, buf_end, ec);
        if (bytes > 0) {
            handler(boost::system::error_code(), buf_begin, buf_end);
            recv_buffer_.consume(bytes);
            return ;
        }

        if (ec) {
            if (!ec_) ec_ = ec;
            post_handler(handler, ec);
            return ;
        }

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

private:
    size_t peek_packets(const_buffer* buf_begin, const_buffer*& buf_end,
            boost::system::error_code & ec,
            size_t max_size = std::numeric_limits<size_t>::max())
    {
        ec = boost::system::error_code();
        size_t bytes = 0;
        for (auto it = buf_begin; it != buf_end; ++it) {
            size_t packet_size = packet_splitter_(recv_buffer_.gptr() + bytes, recv_buffer_.size() - bytes);
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

            *it = const_buffer(recv_buffer_.gptr() + bytes, packet_size);
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
