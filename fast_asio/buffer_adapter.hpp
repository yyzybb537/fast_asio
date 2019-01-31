#pragma once
#include "asio_include.h"
#include <string>
#include <cstddef>

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

template <typename Buffer>
struct buffer_sequence_ref
{
    Buffer* begin_;
    Buffer* end_;

    buffer_sequence_ref(Buffer* begin, Buffer* end)
        : begin_(begin), end_(end) {}

    Buffer* begin() const {
        return begin_;
    }

    Buffer* end() const {
        return end_;
    }
};

template <typename Buffer>
inline buffer_sequence_ref<Buffer> buffers_ref(Buffer* begin, Buffer* end) {
    return buffer_sequence_ref<Buffer>(begin, end);
}

template <typename Buffer>
inline buffer_sequence_ref<Buffer> buffers_ref(Buffer* begin, size_t count) {
    return buffer_sequence_ref<Buffer>(begin, begin + count);
}

} //namespace fast_asio
