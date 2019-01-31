#include <fast_asio/fast_asio.hpp>
#include <iostream>
#include <memory>
#include "stat.hpp"

//struct Timer { Timer() : tp(system_clock::now()) {} virtual ~Timer() { auto dur = system_clock::now() - tp; O("Cost " << duration_cast<milliseconds>(dur).count() << " ms"); } system_clock::time_point tp; };
//struct Bench : public Timer { Bench() : val(0) {} virtual ~Bench() { stop(); } void stop() { auto dur = system_clock::now() - tp; O("Per op: " << duration_cast<nanoseconds>(dur).count() / std::max(val, 1L) << " ns"); auto perf = (double)val / duration_cast<milliseconds>(dur).count() / 10; if (perf < 1) O("Performance: " << std::setprecision(3) << perf << " w/s"); else O("Performance: " << perf << " w/s"); } Bench& operator++() { ++val; return *this; } Bench& operator++(int) { ++val; return *this; } Bench& add(long v) { val += v; return *this; } long val; };

using namespace boost::asio;
using namespace boost::asio::ip;

// socket type
typedef fast_asio::packet_stream<tcp::socket> socket_t;
//typedef fast_asio::packet_read_stream<tcp::socket> socket_t;
typedef std::shared_ptr<socket_t> socket_ptr;

void onReceive(socket_ptr socket, boost::system::error_code ec, const_buffer* buf_begin, const_buffer* buf_end) {
    if (ec) {
        std::cout << "disconnected, reason:" << ec.message() << std::endl;
        return ;
    }

    size_t bytes = 0;
    for (auto it = buf_begin; it != buf_end; ++it)
        bytes += it->size();

    stats::instance().inc(stats::qps, 1);
    stats::instance().inc(stats::bytes, bytes);
//    std::cout << "onReceive" << std::endl;

    // copy test
//    streambuf sb;
//    for (auto it = buf_begin; it != buf_end; ++it) {
//        auto mb = sb.prepare(it->size());
//        ::memcpy(mb.data(), it->data(), it->size());
//        sb.commit(it->size());
//    }

    // ping-pong
    socket->async_write_some(fast_asio::buffers_ref(buf_begin, buf_end), [](boost::system::error_code, size_t){});

    socket->async_read_some(std::bind(&onReceive, socket,
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3));
}

int main() {

    io_context ioc;

    socket_ptr socket(new socket_t(ioc));

    // 1.设置拆包函数 (默认函数就是这个, 可以不写这一行)
    socket->set_packet_splitter(&fast_asio::default_packet_policy::split);

    // 2.连接
    tcp::endpoint addr(address::from_string("127.0.0.1"), 1234);
    socket->next_layer().async_connect(addr,
            [socket](boost::system::error_code ec) {
                if (ec) {
                    std::cout << "connect error:" << ec.message() << std::endl;
                    return ;
                }

                std::cout << "connect success" << std::endl;

                // 3.连接成功, 发起读操作
                socket->async_read_some(std::bind(&onReceive, socket,
                            std::placeholders::_1,
                            std::placeholders::_2,
                            std::placeholders::_3));

                // 4.发一个包
//                char buf[15 * 1024] = {};
                char buf[15] = {};
                std::string packet = fast_asio::default_packet_policy::serialize_to_string(buffer(buf, sizeof(buf)));
                socket->async_write_some(buffer(packet), [](boost::system::error_code ec, size_t){
                            std::cout << "ping " << ec.message() << std::endl;
                        });
            });

    ioc.run();
}

