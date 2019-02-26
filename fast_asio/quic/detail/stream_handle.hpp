#pragma once

#include "../../asio_include.h"
#include <net/quic/quartc/quartc_stream.h>
#include "clock.hpp"
#include "session_handle.hpp"

namespace fast_asio {
namespace quic {
namespace detail {

using namespace net;

class stream_handle;
typedef std::shared_ptr<stream_handle> stream_handle_ptr;

class stream_handle
    : private QuartcStreamInterface::Delegate
{
    boost::asio::io_context & ioc_;

    // native quic session by libquic
    session_handle_ptr session_;

    QuartcStream* stream_;

    // OnClose设为true, 之后stream_被删除.
    volatile bool closed_ = false;

    uint64_t threshold_ = 0;

    uint32_t stream_id_ = 0;

public:
    static stream_handle_ptr create_outgoing_stream(session_handle_ptr session)
    {
        QuartcStreamInterface* stream = session->create_stream();
        if (!stream) return stream_handle_ptr();
        return new stream_handle(session, stream);
    }

    static stream_handle_ptr accept_ingoing_stream(session_handle_ptr session)
    {
        QuartcStreamInterface* stream = session->accept_stream();
        if (!stream) return stream_handle_ptr();
        return new stream_handle(session, stream);
    }

public:
    stream_handle(session_handle_ptr session, QuartcStreamInterface* stream)
        : ioc_(session->get_io_context()), session_(session), stream_(static_cast<QuartcStream*>(stream))
    {
        stream_->SetDelegate(this);
        stream_id_ = stream_->stream_id();
    }

    void set_threshold(uint64_t threshold) { threshold_ = threshold; }

    uint64_t threshold() const { return threshold_; }

    ssize_t readv(const struct iovec* iov, size_t iov_count, boost::system::error_code & ec)
    {
        std::unique_lock<std::recursive_mutex> lock(session_->mutex());
        ec.clear();

        if (closed_) {
            ec = boost::asio::error::bad_descriptor;
            return -1;
        }

        if (stream_->fin_received()) {
            ec = boost::asio::error::shut_down;
            return 0;
        }

        int res = stream_->Readv(iov, iov_count);
        if (res == 0) {
            if (closed_) {
                ec = boost::asio::error::bad_descriptor;
                return -1;
            }

            if (stream_->fin_received()) {
                return 0;
            }

            ec = boost::asio::error::try_again;
            return -1;
        }

        return res;
    }

    ssize_t writev(const struct iovec* iov, size_t iov_count, bool fin)
    {
        std::unique_lock<std::recursive_mutex> lock(session_->mutex());
        ec.clear();

        if (closed_) {
            ec = boost::asio::error::bad_descriptor;
            return -1;
        }

        if (stream_->fin_sent()) {
            ec = boost::asio::error::shut_down;
            return 0;
        }

        int res = stream_->WritevData(iov, iov_count, fin);
        if (res == 0) {
            if (closed_) {
                ec = boost::asio::error::bad_descriptor;
                return -1;
            }

            if (stream_->fin_sent()) {
                ec = boost::asio::error::shut_down;
                return 0;
            }

            ec = boost::asio::error::try_again;
            return -1;
        }

        return res;
    }

    void shutdown(boost::asio::socket_base::shutdown_type type)
    {
        std::unique_lock<std::recursive_mutex> lock(session_->mutex());
        if (!stream_) return ;

        if (type == boost::asio::socket_base::shutdown_both) {
            stream_->FinishReading();
            stream_->FinishWriting();
        } else if (type == boost::asio::socket_base::shutdown_receive) {
            stream_->FinishReading();
        } else if (type == boost::asio::socket_base::shutdown_send) {
            stream_->FinishWriting();
        }
    }

    void close()
    {
        std::unique_lock<std::recursive_mutex> lock(session_->mutex());
        if (!stream_) return ;

        stream_->Close();
    }

    uint32_t stream_id()
    {
        return stream_id_;
    }

    std::size_t available(boost::system::error_code& ec) const
    {
        std::unique_lock<std::recursive_mutex> lock(session_->mutex());
        if (!stream_) {
            ec = boost::asio::error::bad_descriptor;
            return 0;
        }

        ec.clear();
        return stream_->ReadableBytes();
    }

private:
    // ------------------ Delegate
    void OnDataAvailable(QuartcStreamInterface* stream) override;

    void OnClose(QuartcStreamInterface* stream) override;

    // Called when the contents of the stream's buffer changes.
    void OnBufferChanged(QuartcStreamInterface* stream) override {}

    void OnCanWriteNewData(QuartcStreamInterface* stream) override;

    void OnFinRead(QuartcStreamInterface* stream) override {}

    QuicByteCount GetBufferedDataThreshold(QuicByteCount default_threshold) const override
    {
        return threshold_ ? threshold_ : default_threshold;
    }
    // ---------------------------
};

} // namespace detail
} // namespace quic
} // namespace fast_asio

