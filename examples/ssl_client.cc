#include <fast_asio/fast_asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <memory>
#include "root_certificates.hpp"

using namespace boost::asio;
using namespace boost::asio::ip;

// socket type
typedef ssl::stream<tcp::socket> ssl_socket;
typedef fast_asio::packet_stream<ssl_socket> socket_t;
typedef std::shared_ptr<socket_t> socket_ptr;

ssl::context ctx{ssl::context::sslv23_client};

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
//    boost::system::error_code ignore_ec;
//    socket->shutdown(ignore_ec);
}

int main() {
    load_root_certificates(ctx);
    ctx.set_verify_mode(ssl::verify_none);

    io_context ioc;

    socket_ptr socket(new socket_t(ioc, ctx));

    // 1.设置拆包函数 (默认函数就是这个, 可以不写这一行)
    socket->set_packet_splitter(&fast_asio::default_packet_policy::split);

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
                socket->next_layer().handshake(boost::asio::ssl::stream_base::handshake_type::client, ec);
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

