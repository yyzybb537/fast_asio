#include <fast_asio/fast_beast.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <memory>

using namespace boost::asio;
using namespace boost::asio::ip;

// socket type
typedef boost::beast::websocket::stream<tcp::socket> basic_websocket_t;
typedef fast_asio::beast_websocket_adapter<basic_websocket_t> wrapper_websocket_t;
typedef fast_asio::packet_stream<wrapper_websocket_t> socket_t;
typedef std::shared_ptr<socket_t> socket_ptr;

void onReceive(socket_ptr socket, boost::system::error_code ec, const_buffer* buf_begin, const_buffer* buf_end) {
    if (ec) {
        std::cout << "disconnected, reason:" << ec.message() << std::endl;
        return ;
    }

    for (const_buffer* it = buf_begin; it != buf_end; ++it) {
        const_buffer body = fast_asio::default_packet_policy::get_body(*it);
        std::cout << "onReceive:" << std::string((const char*)body.data(), body.size()) << std::endl;
    }

    // 关闭连接
    boost::system::error_code ignore_ec;
    socket->close(ignore_ec);
}

int main() {

    io_context ioc;

    socket_ptr socket(new socket_t(ioc));

    // 2.连接
    tcp::endpoint addr(address::from_string("127.0.0.1"), 1234);
    socket->next_layer().next_layer().async_connect(addr,
            [socket](boost::system::error_code ec) {
                if (ec) {
                    std::cout << "connect error:" << ec.message() << std::endl;
                    return ;
                }

                std::cout << "connect success" << std::endl;

                // 握手(为了方便写个同步的)
                socket->next_layer().handshake("127.0.0.1", "/", ec);
                if (ec) {
                    std::cout << "handshake error:" << ec.message() << std::endl;
                    return ;
                }

                // 3.连接成功, 发起读操作
                socket->async_read_some(std::bind(&onReceive, socket,
                            std::placeholders::_1,
                            std::placeholders::_2,
                            std::placeholders::_3));

                // 4.发一个包
                char buf[] = "Hello fast_asio!";
                std::string packet = fast_asio::default_packet_policy::serialize_to_string(buffer(buf, sizeof(buf)));

                socket->async_write_some(buffer(packet), [](boost::system::error_code ec, size_t){
                            std::cout << "ping " << ec.message() << std::endl;
                        });
            });

    ioc.run();

    std::cout << socket.use_count() << std::endl;
}

