#pragma once
#include <boost/asio.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/buffer.hpp>
#include <string>
#include <cstddef>

namespace boost {
namespace asio {
        
template <typename Iter>
inline Iter buffer_sequence_begin(std::pair<Iter, Iter> const& p)
{
  return p.first;
}

template <typename Iter>
inline Iter buffer_sequence_end(std::pair<Iter, Iter> const& p)
{
  return p.second;
}

} //namespace asio
} //namespace boost

namespace fast_asio {

using namespace boost::asio;

template <typename Buffer>
struct buffer_adapter;

template <typename Alloc>
struct buffer_adapter<basic_streambuf<Alloc>>
{
    typedef basic_streambuf<Alloc> Buffer;

    static size_t size(Buffer const& buf) {
        return buf.size();
    }

    static const_buffers_1 data(Buffer & buf) {
        return buf.data();
    }

    static void consume(Buffer & buf, size_t n) {
        buf.consume(n);
    }

    static mutable_buffers_1 prepare(Buffer & buf, size_t n) {
        return buf.prepare(n);
    }

    static void commit(Buffer & buf, size_t n) {
        buf.commit(n);
    }

    static void swap(Buffer & lhs, Buffer & rhs) {
        std::swap(lhs, rhs);
    }
};

} //namespace fast_asio
