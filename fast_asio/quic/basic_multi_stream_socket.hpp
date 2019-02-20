#pragma once

#include "../asio_include.h"
#include "basic_socket_acceptor.hpp"

namespace fast_asio {

//using namespace boost::asio;

template <typename Protocol,
         typename Service>
class basic_multi_stream_socket
    : public fast_asio::detail::basic_socket_acceptor<Protocol, Service>
{
public:
    typedef fast_asio::detail::basic_socket_acceptor<Protocol, Service> base_type;

    template <typename ... Args>
    basic_multi_stream_socket(Args && ... args)
        : base_type(std::forward<Args>(args)...)
    {
    }
};

} // namespace fast_asio
