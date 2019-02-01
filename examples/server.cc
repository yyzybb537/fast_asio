#include <fast_asio/fast_asio.hpp>
#include <iostream>
#include <memory>

using namespace boost::asio;
using namespace boost::asio::ip;

// socket type
typedef fast_asio::packet_stream<tcp::socket> socket_t;
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

    // ping-pong
    socket->async_write_some(fast_asio::buffers_ref(buf_begin, buf_end), [](boost::system::error_code, size_t){});

    // 接力读请求
    socket->async_read_some(std::bind(&onReceive, socket,
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3));
}

void onAccept(tcp::acceptor* acceptor, socket_ptr socket, boost::system::error_code ec) {
    if (ec) {
        std::cout << "accept error:" << ec.message() << std::endl;
        return ;
    }

    std::cout << "accept success" << std::endl;

    // 1.设置拆包函数 (默认函数就是这个, 可以不写这一行)
    socket->set_packet_splitter(&fast_asio::default_packet_policy::split);

    // 2.连接成功, 发起读操作
    socket->async_read_some(std::bind(&onReceive, socket,
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3));

    // accept接力
    socket_ptr new_socket(new socket_t(acceptor->get_io_context()));
    acceptor->async_accept(new_socket->next_layer(), std::bind(&onAccept, acceptor, new_socket, std::placeholders::_1));
}

int main() {

    io_context ioc;

    tcp::endpoint addr(address::from_string("127.0.0.1"), 1234);
    tcp::acceptor acceptor(ioc, addr);

    socket_ptr socket(new socket_t(ioc));
    acceptor.async_accept(socket->next_layer(), std::bind(&onAccept, &acceptor, socket, std::placeholders::_1));

    ioc.run();
}

