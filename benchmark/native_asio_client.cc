#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include "stat.hpp"

//struct Timer { Timer() : tp(system_clock::now()) {} virtual ~Timer() { auto dur = system_clock::now() - tp; O("Cost " << duration_cast<milliseconds>(dur).count() << " ms"); } system_clock::time_point tp; };
//struct Bench : public Timer { Bench() : val(0) {} virtual ~Bench() { stop(); } void stop() { auto dur = system_clock::now() - tp; O("Per op: " << duration_cast<nanoseconds>(dur).count() / std::max(val, 1L) << " ns"); auto perf = (double)val / duration_cast<milliseconds>(dur).count() / 10; if (perf < 1) O("Performance: " << std::setprecision(3) << perf << " w/s"); else O("Performance: " << perf << " w/s"); } Bench& operator++() { ++val; return *this; } Bench& operator++(int) { ++val; return *this; } Bench& add(long v) { val += v; return *this; } long val; };

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

int main() {

    io_context ioc;

    socket_ptr socket(new socket_t(ioc));

    // 2.连接
    tcp::endpoint addr(address::from_string("127.0.0.1"), 1234);
    socket->async_connect(addr,
            [socket](boost::system::error_code ec) {
                if (ec) {
                    std::cout << "connect error:" << ec.message() << std::endl;
                    return ;
                }

                std::cout << "connect success" << std::endl;

                // 3.连接成功, 发起读操作
                streambuf_ptr sb(new streambuf);
                streambuf_ptr wsb(new streambuf);
                socket->async_read_some(sb->prepare(4096), std::bind(&onReceive, socket, sb, wsb,
                            std::placeholders::_1,
                            std::placeholders::_2));

                // 4.发一个包
//                char buf[15 * 1024] = {};
                char buf[15 + 4] = {};
                socket->async_write_some(buffer(buf), [](boost::system::error_code ec, size_t){
                            std::cout << "ping " << ec.message() << std::endl;
                        });
            });

    ioc.run();
}

