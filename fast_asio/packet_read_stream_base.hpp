#pragma once
#include "asio_include.h"

namespace fast_asio {

using namespace boost::asio;

class packet_read_stream_base
{
public:
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
};

} //namespace fast_asio
