// Copy from asio::quic_socket_service.hpp and change it.
#pragma once

#include "../asio_include.h"
#include "detail/session_handle.hpp"
#include "detail/stream_handle.hpp"
#include "detail/header_parser.hpp"
#include "detail/common.hpp"
#include "detail/clock.hpp"
#include <memory>
#include "quic.hpp"

namespace fast_asio {
namespace quic {

using namespace net;

/// Default service implementation for a stream socket.
class quic_socket_service
  : public boost::asio::detail::service_base<quic_socket_service>
{
public:
#if defined(GENERATING_DOCUMENTATION)
    /// The unique service identifier.
    static boost::asio::io_context::id id;
#endif

    /// The protocol type.
    typedef quic protocol_type;

    /// The endpoint type.
    typedef typename quic::endpoint endpoint_type;

public:
    typedef std::shared_ptr<boost::asio::ip::udp::socket> udp_socket_ptr;
    struct implementation_type;
    struct udp_handle {
        udp_socket_ptr udp_socket_;

        char buf_[65536];

        bool closed_ = false;

        endpoint_type local_endpoint_;

        endpoint_type peer_endpoint_;

        std::mutex mtx_;

        session_handle_ptr listener_;

        std::map<uint64_t, session_handle_ptr> connections_;

        std::set<session_handle_ptr> syn_list_;

        std::list<session_handle_ptr> accept_list_;

        std::list<std::function<void(boost::system::error_code, implementation_type&)> accept_handlers_;

        ~udp_handle() {
            // TODO: close udp and remove from quic_socket_service
        }
    };

    /// The native socket type.
    typedef std::shared_ptr<udp_handle> udp_handle_ptr;
    typedef udp_handle_ptr native_handle_type;

    /// The type of a stream socket implementation.
    struct implementation_type {
        native_handle_type udp_ptr;

        // quic socket or stream handle shared_ptr
        session_handle_ptr session;

        stream_handle_ptr stream;

        void reset() {
            udp_ptr.reset();
            session.reset();
            stream.reset();
        }
    };

private:
    std::mutex mtx_;

    // udp sockets
    std::map<endpoint_type, udp_handle_ptr> udp_handles_;

public:
    /// Construct a new stream socket service for the specified io_context.
    explicit quic_socket_service(boost::asio::io_context& io_context)
        : boost::asio::detail::service_base<quic_socket_service>(io_context)
    {
    }

    /// Construct a new stream socket implementation.
    void construct(implementation_type& impl)
    {
    }

    //#if defined(BOOST_ASIO_HAS_MOVE) || defined(GENERATING_DOCUMENTATION)
    //  /// Move-construct a new stream socket implementation.
    //  void move_construct(implementation_type& impl,
    //      implementation_type& other_impl)
    //  {
    //    service_impl_.move_construct(impl, other_impl);
    //  }
    //
    //  /// Move-assign from another stream socket implementation.
    //  void move_assign(implementation_type& impl,
    //      quic_socket_service& other_service,
    //      implementation_type& other_impl)
    //  {
    //    service_impl_.move_assign(impl, other_service.service_impl_, other_impl);
    //  }
    //
    //  // All socket services have access to each other's implementations.
    //  template <typename Protocol1> friend class quic_socket_service;
    //
    //  /// Move-construct a new stream socket implementation from another protocol
    //  /// type.
    //  template <typename Protocol1>
    //  void converting_move_construct(implementation_type& impl,
    //      quic_socket_service<Protocol1>& other_service,
    //      typename quic_socket_service<
    //        Protocol1>::implementation_type& other_impl,
    //      typename enable_if<is_convertible<
    //        Protocol1, Protocol>::value>::type* = 0)
    //  {
    //    service_impl_.template converting_move_construct<Protocol1>(
    //        impl, other_service.service_impl_, other_impl);
    //  }
    //#endif // defined(BOOST_ASIO_HAS_MOVE) || defined(GENERATING_DOCUMENTATION)

    /// Destroy a stream socket implementation.
    void destroy(implementation_type& impl)
    {
        impl.reset();
    }

    /// Open a stream socket.
    BOOST_ASIO_SYNC_OP_VOID open(implementation_type& impl,
            const protocol_type& protocol, boost::system::error_code& ec)
    {
        if (is_open(impl))
            return ;

        bind(impl, endpoint_type(), ec);
    }

    /// Assign an existing native socket to a stream socket.
    BOOST_ASIO_SYNC_OP_VOID assign(implementation_type& impl,
            const protocol_type& protocol, const native_handle_type& native_socket,
            boost::system::error_code& ec)
    {
        ec.clear();
        impl.udp_ptr = native_socket;
    }

    /// Determine whether the socket is open.
    bool is_open(const implementation_type& impl) const
    {
        return impl.udp_ptr;
    }

    /// Close a stream socket implementation.
    BOOST_ASIO_SYNC_OP_VOID close(implementation_type& impl,
            boost::system::error_code& ec)
    {
        ec.clear();

    }

    /// Release ownership of the underlying socket.
    native_handle_type release(implementation_type& impl,
            boost::system::error_code& ec)
    {
        // Not support release.
        return impl.udp_ptr;
    }

    /// Get the native socket implementation.
    native_handle_type native_handle(implementation_type& impl)
    {
        return impl.udp_ptr;
    }

    /// Cancel all asynchronous operations associated with the socket.
    BOOST_ASIO_SYNC_OP_VOID cancel(implementation_type& impl,
            boost::system::error_code& ec)
    {
        // TODO
    }

    /// Determine whether the socket is at the out-of-band data mark.
    bool at_mark(const implementation_type& impl,
            boost::system::error_code& ec) const
    {
//        return service_impl_.at_mark(impl, ec);
        return false;
    }

    /// Determine the number of bytes available for reading.
    std::size_t available(const implementation_type& impl,
            boost::system::error_code& ec) const
    {
        if (impl.stream) {
            return impl.stream->available(ec);
        }

        if (impl.session) {
            return impl.session->available(ec);
        }

        ec = boost::asio::error::bad_descriptor;
        return 0;
    }

    /// Bind the stream socket to the specified local endpoint.
    BOOST_ASIO_SYNC_OP_VOID bind(implementation_type& impl,
            const endpoint_type& endpoint, boost::system::error_code& ec)
    {
        if (impl.stream) {
            ec = boost::asio::error::bad_descriptor;
            return ;
        }

        if (impl.session) {
            ec = boost::asio::error::connection_refused;
            return ;
        }

        udp_handle_ptr udp;

        {
            std::unique_lock<std::mutex> lock(mtx_);
            auto it = (endpoint == endpoint_type()) ? udp_handles_.begin() : udp_handles_.find(endpoint);
            if (it == udp_handles_.end()) {
                udp.reset(new udp_handle);
                udp->udp_socket_.reset(new boost::asio::ip::udp::socket(get_io_context()));
                udp->udp_socket_->bind(endpoint, ec);
                if (ec)
                    return ;

                udp->local_endpoint_ = udp->udp_socket_->local_endpoint(ec);
                if (ec)
                    return ;

                udp_handles_[udp->local_endpoint_] = udp;
                start_udp_async_receive(udp);
            } else {
                udp = it->second;
            }
        }

        impl.udp_ptr = udp;
        ec.clear();
        return ;
    }

    BOOST_ASIO_SYNC_OP_VOID listen(implementation_type& impl,
            size_t backlog, boost::system::error_code& ec)
    {
        if (impl.stream) {
            ec = boost::asio::error::bad_descriptor;
            return ;
        }

        if (impl.session) {
            ec = boost::asio::error::connection_refused;
            return ;
        }

        std::unique_lock<std::mutex> lock(impl.udp_ptr->mtx_);
        if (impl.udp_ptr->listener_) {
            ec = boost::asio::error::connection_refused;
            return ;
        }

        impl.session.reset(new session_handle(get_io_context(), true));
        impl.udp_ptr->listener_ = impl.session;
        ec.clear();
        return ;
    }

    /// Connect the stream socket to the specified endpoint.
    BOOST_ASIO_SYNC_OP_VOID connect(implementation_type& impl,
            const endpoint_type& peer_endpoint, boost::system::error_code& ec)
    {
        return ;
    }

    /// Start an asynchronous connect.
    template <typename ConnectHandler>
        BOOST_ASIO_INITFN_RESULT_TYPE(ConnectHandler,
                void (boost::system::error_code))
        async_connect(implementation_type& impl,
                const endpoint_type& peer_endpoint,
                BOOST_ASIO_MOVE_ARG(ConnectHandler) handler)
        {
            if (impl.stream) {
                get_io_context().post([=]{ handler(boost::asio::error::bad_descriptor); });
                return ;
            }

            if (!impl.session) {
                impl.session.reset(new session_handle(get_io_context(), false));

                std::unique_lock<std::mutex> lock(impl.udp_ptr->mtx_);
                impl.udp_ptr->syn_list_.insert(impl.session);
            }

            auto udp = impl.udp_ptr;
            impl.session->async_connect(peer_endpoint,
                    [=](boost::system::error_code ec, session_handle_ptr session){
                        this->on_connect(ec, udp, session, handler);
                    });
            return ;
        }

    template <typename ConnectHandler>
        BOOST_ASIO_INITFN_RESULT_TYPE(ConnectHandler,
                void (boost::system::error_code))
        on_connect(boost::system::error_code ec, udp_handle_ptr udp,
                session_handle_ptr session, BOOST_ASIO_MOVE_ARG(ConnectHandler) handler)
        {
            std::unique_lock<std::mutex> lock(udp->mtx_);
            if (!udp->syn_list_.erase(session)) {
                handler(boost::asio::error::operation_aborted);
                return ;
            }

            if (ec) {
                handler(boost::asio::error::operation_aborted);
                return ;
            }

            udp->connections_[session->connection_id()] = session;
            handler(boost::system::error_code());
        }

    template <typename AcceptHandler>
        BOOST_ASIO_INITFN_RESULT_TYPE(AcceptHandler,
                void (boost::system::error_code))
        async_accept(implementation_type& listen_impl,
                implementation_type& peer_impl,
                BOOST_ASIO_MOVE_ARG(AcceptHandler) handler)
        {
            if (!listen_impl.udp_ptr) {
                get_io_context().post([=]{ handler(boost::asio::error::bad_descriptor); });
                return ;
            }

            if (!listen_impl.session) {
                get_io_context().post([=]{ handler(boost::asio::error::bad_descriptor); });
                return ;
            }

            if (listen_impl.session == listen_impl.udp_ptr->listener_) {
                // acceptor
                std::unique_lock<std::mutex> lock(listen_impl.udp_ptr->mtx_);
                implementation_type* out_impl = &peer_impl;
                udp->accept_handlers_.push_back([=](boost::system::error_code ec, implementation_type& new_impl){
                        if (!ec) {
                        *out_impl = new_impl;
                        }
                        handler(ec);
                        });
                auto udp = listen_impl.udp_ptr;
                if (!udp->accept_list_.empty())
                    get_io_context().post([this, udp]{ this->on_accept(udp); });
                return ;
            }

            // connection accept streams
            listen_impl.session->accept_stream();
        }

    void on_accept(boost::system::error_code ec, session_handle_ptr session)
    {
        udp_handle_ptr udp = find_udp_handle(session);
        if (!udp) return ;

        std::unique_lock<std::mutex> lock(udp->mtx_);
        if (!udp->syn_list_.erase(session)) {
            return ;
        }

        if (ec) {
            return ;
        }

        udp->connections_[session->connection_id()] = session;
        udp->accept_list_.push_back(session);
        lock.unlock();

        on_accept(udp);
    }

    void on_syn_error(boost::system::error_code ec, session_handle_ptr session)
    {
        // TODO
    }

    void on_accept(udp_handle_ptr udp)
    {
        std::unique_lock<std::mutex> lock(udp->mtx_);
        if (udp->accept_handlers_.empty() || udp->accept_list_.empty())
            return ;

        implementation_type impl;
        impl.udp_ptr = udp;
        impl.session = udp->accept_list_.front();
        auto handler = udp->accept_handlers_.front();
        udp->accept_list_.pop_front();
        udp->accept_handlers_.pop_front();
        lock.unlock();

        handler(boost::system::error_code(), impl);
    }

    udp_handle_ptr find_udp_handle(session_handle_ptr session)
    {
        auto endpoint = session->local_endpoint();

        std::unique_lock<std::mutex> lock(mtx_);
        auto it = udp_handles_.find(endpoint);
        return it == udp_handles_.end() ? udp_handle_ptr() : it->second;
    }

    /// Set a socket option.
    template <typename SettableSocketOption>
        BOOST_ASIO_SYNC_OP_VOID set_option(implementation_type& impl,
                const SettableSocketOption& option, boost::system::error_code& ec)
        {
        }

    /// Get a socket option.
    template <typename GettableSocketOption>
        BOOST_ASIO_SYNC_OP_VOID get_option(const implementation_type& impl,
                GettableSocketOption& option, boost::system::error_code& ec) const
        {
        }

    /// Get the local endpoint.
    endpoint_type local_endpoint(const implementation_type& impl,
            boost::system::error_code& ec) const
    {
        if (!impl.udp_ptr) {
            ec = boost::asio::error::invalid_argument;
            return ;
        }

        return impl.udp_ptr->local_endpoint_;
    }

    /// Get the remote endpoint.
    endpoint_type remote_endpoint(const implementation_type& impl,
            boost::system::error_code& ec) const
    {
        if (!impl.session) {
            ec = boost::asio::error::invalid_argument;
            return ;
        }

        return impl.session->remote_endpoint();
    }

    /// Disable sends or receives on the socket.
    BOOST_ASIO_SYNC_OP_VOID shutdown(implementation_type& impl,
            socket_base::shutdown_type what, boost::system::error_code& ec)
    {
    }

    /// Wait for the socket to become ready to read, ready to write, or to have
    /// pending error conditions.
    BOOST_ASIO_SYNC_OP_VOID wait(implementation_type& impl,
            socket_base::wait_type w, boost::system::error_code& ec)
    {
    }

    /// Asynchronously wait for the socket to become ready to read, ready to
    /// write, or to have pending error conditions.
    template <typename WaitHandler>
        BOOST_ASIO_INITFN_RESULT_TYPE(WaitHandler,
                void (boost::system::error_code))
        async_wait(implementation_type& impl, socket_base::wait_type w,
                BOOST_ASIO_MOVE_ARG(WaitHandler) handler)
        {
        }

    /// Start an asynchronous send.
    template <typename ConstBufferSequence, typename WriteHandler>
        BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
                void (boost::system::error_code, std::size_t))
        async_write_some(implementation_type& impl,
                const ConstBufferSequence& buffers,
                bool fin,
                BOOST_ASIO_MOVE_ARG(WriteHandler) handler)
        {
            if (!impl.stream) {
                get_io_context().post([=]{ handler(boost::asio::error::bad_descriptor, 0); });
                return ;
            }

            stream_handle* stream = impl.stream.get();
            auto write_handler = [=](bool async){
                boost::system::error_code ec;
                struct iovec iov[MAX_IOV];
                size_t iov_count = 0;
                for (auto buf = buffer_sequence_begin(buffers); buf != buffer_sequence_end(buffers); ++buf) {
                    iov.iov_base = buf->data();
                    iov.iov_len = buf->size();
                    iov_count++;
                }
                ssize_t bytes = stream->writev(iov, iov_count, fin, ec);
                if (bytes < 0 && ec == boost::asio::error::try_again)
                    return false;

                if (async)
                    stream->reset_async_write_handler();
                handler(ec, bytes);
                return true;
            };

            impl.stream->async_write_some(write_handler);
        }

    /// Start an asynchronous receive.
    template <typename MutableBufferSequence, typename ReadHandler>
        BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
                void (boost::system::error_code, std::size_t))
        async_read_some(implementation_type& impl,
                const MutableBufferSequence& buffers,
                BOOST_ASIO_MOVE_ARG(ReadHandler) handler)
        {
            if (!impl.stream) {
                get_io_context().post([=]{ handler(boost::asio::error::bad_descriptor, 0); });
                return ;
            }

            stream_handle* stream = impl.stream.get();
            auto read_handler = [=](bool async){
                boost::system::error_code ec;
                struct iovec iov[MAX_IOV];
                size_t iov_count = 0;
                for (auto buf = buffer_sequence_begin(buffers); buf != buffer_sequence_end(buffers); ++buf) {
                    iov.iov_base = buf->data();
                    iov.iov_len = buf->size();
                    iov_count++;
                }
                ssize_t bytes = stream->readv(iov, iov_count, ec);
                if (bytes < 0 && ec == boost::asio::error::try_again)
                    return false;

                if (async)
                    stream->reset_async_read_handler();
                handler(ec, bytes);
                return true;
            };

            impl.stream->async_read_some(read_handler);
        }

private:
    // Destroy all user-defined handler objects owned by the service.
    void shutdown()
    {
        
    }

    void start_udp_async_receive(udp_handle_ptr udp) {
        if (udp->closed_) return ;

        udp->udp_socket_->async_receive_from(boost::asio::buffer(udp->buf_),
                udp->peer_endpoint_,
                [this, udp](boost::system::error_code ec, size_t bytes) {
                    this->on_udp_receive(udp, ec, bytes);
                });
    }

    void on_udp_receive(udp_handle_ptr udp, boost::system::error_code ec, size_t bytes) {
        start_udp_async_receive(udp);

        if (ec) return ;

        // TODO: check connection-id and process bytes.
        header_parser parser;
        QuicConnectionId connection_id = parser.parse(&udp->buf_[0], bytes);
        if (connection_id == QuicConnectionId(-1)) {
            // invalid connection id;
            return ;
        }

        QuicSocketAddress self_address = detail::address_convert(udp->local_endpoint_);
        QuicSocketAddress peer_address = detail::address_convert(udp->peer_endpoint_);

        session_handle_ptr session;

        std::unique_lock<std::mutex> lock(udp->mtx_);
        auto it = udp->connections_.find(connection_id);
        if (it == udp->connections_.end()) {
            if (!udp->listener_)
                return ;

            // new connection
            session.reset(new session_handle(get_io_context(), true, connection_id));
            udp->connections_[connection_id] = session;
            udp->syn_list_.insert(session);
            session->OnSyn(peer_address);
        }

        session->ProcessUdpPacket(self_address, peer_address,
                QuicReceivedPacket(&udp->buf_[0], bytes,
                    detail::clock::getInstance().Now()));
    }
};

} // namespace quic
} // namespace fast_asio

#include "detail/session_handle.ipp"
#include "detail/stream_handle.ipp"
