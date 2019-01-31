#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include "stat.hpp"

using namespace boost::asio;
using namespace boost::asio::ip;

// socket type
typedef tcp::socket socket_t;
typedef std::shared_ptr<socket_t> socket_ptr;
typedef std::shared_ptr<streambuf> streambuf_ptr;

void onReceive(socket_ptr socket, streambuf_ptr sb, streambuf_ptr wsb, boost::system::error_code ec, size_t bytes) {
    if (ec) {
        std::cout << "disconnected, reason:" << ec.message() << std::endl;
        return ;
    }

//    std::cout << "onReceive" << std::endl;
    stats::instance().inc(stats::qps, 1);
    stats::instance().inc(stats::bytes, bytes);

    // ps:此处假设不会被拆包, 仅用于测试原生asio的性能做对比
    sb->commit(bytes);

    wsb->consume(wsb->size());
    buffer_copy(wsb->prepare(bytes), sb->data());
    wsb->commit(bytes);

    // ping-pong
    socket->async_write_some(wsb->data(), [wsb](boost::system::error_code ec, size_t bytes){});

    sb->consume(bytes);

    socket->async_read_some(sb->prepare(4096), std::bind(&onReceive, socket, sb, wsb,
                std::placeholders::_1,
                std::placeholders::_2));
}

void onAccept(tcp::acceptor* acceptor, socket_ptr socket, boost::system::error_code ec) {
    if (ec) {
        std::cout << "accept error:" << ec.message() << std::endl;
        return ;
    }

    std::cout << "accept success" << std::endl;

    // 2.连接成功, 发起读操作
    streambuf_ptr sb(new streambuf);
    streambuf_ptr wsb(new streambuf);
    socket->async_read_some(sb->prepare(4096), std::bind(&onReceive, socket, sb, wsb,
                std::placeholders::_1,
                std::placeholders::_2));

    // accept接力
    socket_ptr new_socket(new socket_t(acceptor->get_io_context()));
    acceptor->async_accept(*new_socket, std::bind(&onAccept, acceptor, new_socket, std::placeholders::_1));
}

int main() {

    io_context ioc;

    tcp::endpoint addr(address::from_string("127.0.0.1"), 1234);
    tcp::acceptor acceptor(ioc, addr);

    socket_ptr socket(new socket_t(ioc));
    acceptor.async_accept(*socket, std::bind(&onAccept, &acceptor, socket, std::placeholders::_1));

    ioc.run();
}

