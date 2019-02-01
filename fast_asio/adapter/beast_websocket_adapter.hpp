#pragma once
#include "../asio_include.h"
#include "../error_code.hpp"
#include <boost/beast/websocket.hpp>

namespace fast_asio {

template <typename BeastWebsocketStream>
class beast_websocket_adapter;

template <typename NextLayer, bool deflateSupported>
class beast_websocket_adapter<boost::beast::websocket::stream<NextLayer, deflateSupported>>
    : public boost::beast::websocket::stream<NextLayer, deflateSupported>
{
public:
    using websocket_t = boost::beast::websocket::stream<NextLayer, deflateSupported>;

    template <typename ... Args>
    beast_websocket_adapter(Args&& ... args)
        : websocket_t(std::forward<Args>(args)...)
    {
    }

    websocket_t & native() {
        return *this;
    }

    websocket_t const& native() const {
        return *this;
    }

    template<class ConstBufferSequence>                     
    std::size_t write_some(ConstBufferSequence const& buffers, boost::system::error_code& ec, bool fin = false)
    {
        return websocket_t::write_some(fin, buffers, ec);
    }

    template<class ConstBufferSequence>                     
    std::size_t write_some(ConstBufferSequence const& buffers, bool fin = false)
    {
        return websocket_t::write_some(fin, buffers);
    }

    template<class ConstBufferSequence, class WriteHandler>         
    BOOST_ASIO_INITFN_RESULT_TYPE(                                  
            WriteHandler, void(boost::system::error_code, std::size_t))                
    async_write_some(ConstBufferSequence const& buffers, WriteHandler&& handler, bool fin = false)
    {
        return websocket_t::async_write_some(fin, buffers, std::forward<WriteHandler>(handler));
    }

    void close() {
        boost::beast::websocket::close_reason cr;
        websocket_t::close(cr);
    }

    void close(boost::system::error_code & ec) {
        boost::beast::websocket::close_reason cr;
        websocket_t::close(cr, ec);
    }
};

} //namespace fast_asio
