// Copy from asio::quic_socket_service.hpp and change it.
#pragma once

#include "../asio_include.h"
#include "detail/session_handle.hpp"
#include "detail/stream_handle.hpp"
#include <memory>
#include "quic.hpp"

namespace fast_asio {
namespace quic {

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
    struct udp_handle {
        std::shared_ptr<boost::asio::ip::udp::socket> udp_socket;

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
    std::map<endpoint_type, udp_handle_ptr> udp_sockets_;

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
        ec.clear();
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
        return true;
    }

    /// Close a stream socket implementation.
    BOOST_ASIO_SYNC_OP_VOID close(implementation_type& impl,
            boost::system::error_code& ec)
    {
        ec.clear();
        if (stream) {
            stream->close();
            return ;
        }

        if (session) {
            session->close();
        }
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
        return service_impl_.at_mark(impl, ec);
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
        service_impl_.bind(impl, endpoint, ec);
        BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);
    }

    /// Connect the stream socket to the specified endpoint.
    BOOST_ASIO_SYNC_OP_VOID connect(implementation_type& impl,
            const endpoint_type& peer_endpoint, boost::system::error_code& ec)
    {
        service_impl_.connect(impl, peer_endpoint, ec);
        BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);
    }

    /// Start an asynchronous connect.
    template <typename ConnectHandler>
        BOOST_ASIO_INITFN_RESULT_TYPE(ConnectHandler,
                void (boost::system::error_code))
        async_connect(implementation_type& impl,
                const endpoint_type& peer_endpoint,
                BOOST_ASIO_MOVE_ARG(ConnectHandler) handler)
        {
            async_completion<ConnectHandler,
            void (boost::system::error_code)> init(handler);

            service_impl_.async_connect(impl, peer_endpoint, init.completion_handler);

            return init.result.get();
        }

    /// Set a socket option.
    template <typename SettableSocketOption>
        BOOST_ASIO_SYNC_OP_VOID set_option(implementation_type& impl,
                const SettableSocketOption& option, boost::system::error_code& ec)
        {
            service_impl_.set_option(impl, option, ec);
            BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);
        }

    /// Get a socket option.
    template <typename GettableSocketOption>
        BOOST_ASIO_SYNC_OP_VOID get_option(const implementation_type& impl,
                GettableSocketOption& option, boost::system::error_code& ec) const
        {
            service_impl_.get_option(impl, option, ec);
            BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);
        }

    /// Perform an IO control command on the socket.
    template <typename IoControlCommand>
        BOOST_ASIO_SYNC_OP_VOID io_control(implementation_type& impl,
                IoControlCommand& command, boost::system::error_code& ec)
        {
            service_impl_.io_control(impl, command, ec);
            BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);
        }

    /// Gets the non-blocking mode of the socket.
    bool non_blocking(const implementation_type& impl) const
    {
        return service_impl_.non_blocking(impl);
    }

    /// Sets the non-blocking mode of the socket.
    BOOST_ASIO_SYNC_OP_VOID non_blocking(implementation_type& impl,
            bool mode, boost::system::error_code& ec)
    {
        service_impl_.non_blocking(impl, mode, ec);
        BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);
    }

    /// Gets the non-blocking mode of the native socket implementation.
    bool native_non_blocking(const implementation_type& impl) const
    {
        return service_impl_.native_non_blocking(impl);
    }

    /// Sets the non-blocking mode of the native socket implementation.
    BOOST_ASIO_SYNC_OP_VOID native_non_blocking(implementation_type& impl,
            bool mode, boost::system::error_code& ec)
    {
        service_impl_.native_non_blocking(impl, mode, ec);
        BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);
    }

    /// Get the local endpoint.
    endpoint_type local_endpoint(const implementation_type& impl,
            boost::system::error_code& ec) const
    {
        return service_impl_.local_endpoint(impl, ec);
    }

    /// Get the remote endpoint.
    endpoint_type remote_endpoint(const implementation_type& impl,
            boost::system::error_code& ec) const
    {
        return service_impl_.remote_endpoint(impl, ec);
    }

    /// Disable sends or receives on the socket.
    BOOST_ASIO_SYNC_OP_VOID shutdown(implementation_type& impl,
            socket_base::shutdown_type what, boost::system::error_code& ec)
    {
        service_impl_.shutdown(impl, what, ec);
        BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);
    }

    /// Wait for the socket to become ready to read, ready to write, or to have
    /// pending error conditions.
    BOOST_ASIO_SYNC_OP_VOID wait(implementation_type& impl,
            socket_base::wait_type w, boost::system::error_code& ec)
    {
        service_impl_.wait(impl, w, ec);
        BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);
    }

    /// Asynchronously wait for the socket to become ready to read, ready to
    /// write, or to have pending error conditions.
    template <typename WaitHandler>
        BOOST_ASIO_INITFN_RESULT_TYPE(WaitHandler,
                void (boost::system::error_code))
        async_wait(implementation_type& impl, socket_base::wait_type w,
                BOOST_ASIO_MOVE_ARG(WaitHandler) handler)
        {
            async_completion<WaitHandler,
            void (boost::system::error_code)> init(handler);

            service_impl_.async_wait(impl, w, init.completion_handler);

            return init.result.get();
        }

    /// Send the given data to the peer.
    template <typename ConstBufferSequence>
        std::size_t send(implementation_type& impl,
                const ConstBufferSequence& buffers,
                socket_base::message_flags flags, boost::system::error_code& ec)
        {
            return service_impl_.send(impl, buffers, flags, ec);
        }

    /// Start an asynchronous send.
    template <typename ConstBufferSequence, typename WriteHandler>
        BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
                void (boost::system::error_code, std::size_t))
        async_send(implementation_type& impl,
                const ConstBufferSequence& buffers,
                socket_base::message_flags flags,
                BOOST_ASIO_MOVE_ARG(WriteHandler) handler)
        {
            async_completion<WriteHandler,
            void (boost::system::error_code, std::size_t)> init(handler);

            service_impl_.async_send(impl, buffers, flags, init.completion_handler);

            return init.result.get();
        }

    /// Receive some data from the peer.
    template <typename MutableBufferSequence>
        std::size_t receive(implementation_type& impl,
                const MutableBufferSequence& buffers,
                socket_base::message_flags flags, boost::system::error_code& ec)
        {
            return service_impl_.receive(impl, buffers, flags, ec);
        }

    /// Start an asynchronous receive.
    template <typename MutableBufferSequence, typename ReadHandler>
        BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
                void (boost::system::error_code, std::size_t))
        async_receive(implementation_type& impl,
                const MutableBufferSequence& buffers,
                socket_base::message_flags flags,
                BOOST_ASIO_MOVE_ARG(ReadHandler) handler)
        {
            async_completion<ReadHandler,
            void (boost::system::error_code, std::size_t)> init(handler);

            service_impl_.async_receive(impl, buffers, flags, init.completion_handler);

            return init.result.get();
        }

private:
    // Destroy all user-defined handler objects owned by the service.
    void shutdown()
    {
        service_impl_.shutdown();
    }
};

} // namespace quic
} // namespace fast_asio

#include "detail/session_handle.ipp"
#include "detail/stream_handle.ipp"
