#pragma once
#include "asio_include.h"
#include <type_traits>

namespace fast_asio {

template <typename T>
struct declare_type {
    static T declval();
};

#define FAST_ASIO_DEFINE_HAS_METHOD(name, method)                       \
    template<typename T>                                                \
    struct has_method_ ## name                                          \
    {                                                                   \
    private:                                                            \
        template<typename U>                                            \
        static auto check(int) -> decltype(                             \
                std::declval<U>().method(),                             \
                std::true_type());                                      \
                                                                        \
        template<typename U>                                            \
        static std::false_type check(...);                              \
                                                                        \
    public:                                                             \
        static const bool value = decltype(check<T>(0))::value;         \
    };

#define FAST_ASIO_DEFINE_HAS_METHOD_1(name, method, arg_type_1)         \
    template<typename T>                                                \
    struct has_method_ ## name                                          \
    {                                                                   \
    private:                                                            \
        template<typename U>                                            \
        static auto check(int) -> decltype(                             \
                std::declval<U>().method(                               \
                    std::declval<arg_type_1>()                          \
                    ),                                                  \
                std::true_type());                                      \
                                                                        \
        template<typename U>                                            \
        static std::false_type check(...);                              \
                                                                        \
    public:                                                             \
        static const bool value = decltype(check<T>(0))::value;         \
    };

#define FAST_ASIO_DEFINE_HAS_METHOD_2(name, method, arg_type_1, arg_type_2)   \
    template<typename T>                                                \
    struct has_method_ ## name                                          \
    {                                                                   \
    private:                                                            \
        template<typename U>                                            \
        static auto check(int) -> decltype(                             \
                std::declval<U>().method(                               \
                    std::declval<arg_type_1>()                          \
                    , std::declval<arg_type_2>()                        \
                    ),                                                  \
                std::true_type());                                      \
                                                                        \
        template<typename U>                                            \
        static std::false_type check(...);                              \
                                                                        \
    public:                                                             \
        static const bool value = decltype(check<T>(0))::value;         \
    };

FAST_ASIO_DEFINE_HAS_METHOD(shutdown, shutdown);
FAST_ASIO_DEFINE_HAS_METHOD_1(shutdown_ec, shutdown, boost::system::error_code&);
FAST_ASIO_DEFINE_HAS_METHOD_1(shutdown_type, shutdown, boost::asio::socket_base::shutdown_type);
FAST_ASIO_DEFINE_HAS_METHOD_2(shutdown_type_ec, shutdown, boost::asio::socket_base::shutdown_type, boost::system::error_code&);

FAST_ASIO_DEFINE_HAS_METHOD(close, close);
FAST_ASIO_DEFINE_HAS_METHOD_1(close_ec, close, boost::system::error_code&);

enum e_has_flags_shift {
    has_flag_void_shift = 0,
    has_flag_arg1_shift = 1,
    has_flag_arg2_shift = 2,
    has_flag_arg3_shift = 3,
    has_flag_arg4_shift = 4,
};

enum e_has_flags {
    has_flag_null = 0,
    has_flag_void = 1 << has_flag_void_shift,
    has_flag_arg1 = 1 << has_flag_arg1_shift,
    has_flag_arg2 = 1 << has_flag_arg2_shift,
    has_flag_arg3 = 1 << has_flag_arg3_shift,
    has_flag_arg4 = 1 << has_flag_arg4_shift,
};

// ------------ shutdown
template <typename T, typename Base, int flags>
struct forward_shutdown_helper {};

template <typename T, typename Base>
struct forward_shutdown_helper<T, Base, has_flag_void> {
    void shutdown() {
        static_cast<T&>(*this).next_layer().shutdown();
    }
};

template <typename T, typename Base>
struct forward_shutdown_helper<T, Base, has_flag_arg1> {
    void shutdown(boost::system::error_code & ec) {
        static_cast<T&>(*this).next_layer().shutdown(ec);
    }
};

template <typename T, typename Base>
struct forward_shutdown_helper<T, Base, has_flag_arg2> {
    void shutdown(boost::asio::socket_base::shutdown_type t) {
        static_cast<T&>(*this).next_layer().shutdown(t);
    }
};

template <typename T, typename Base>
struct forward_shutdown_helper<T, Base, has_flag_arg3> {
    void shutdown(boost::asio::socket_base::shutdown_type t, boost::system::error_code & ec) {
        static_cast<T&>(*this).next_layer().shutdown(t, ec);
    }
};

template <typename T, typename Base>
struct forward_shutdown_helper<T, Base, has_flag_void + has_flag_arg1> {
    void shutdown() {
        static_cast<T&>(*this).next_layer().shutdown();
    }

    void shutdown(boost::system::error_code & ec) {
        static_cast<T&>(*this).next_layer().shutdown(ec);
    }
};

template <typename T, typename Base>
struct forward_shutdown_helper<T, Base, has_flag_arg2 + has_flag_arg3> {
    void shutdown(boost::asio::socket_base::shutdown_type t) {
        static_cast<T&>(*this).next_layer().shutdown(t);
    }

    void shutdown(boost::asio::socket_base::shutdown_type t, boost::system::error_code & ec) {
        static_cast<T&>(*this).next_layer().shutdown(t, ec);
    }
};

template <typename T, typename Base>
struct forward_shutdown
    : public forward_shutdown_helper<T, Base,
        (has_method_shutdown<Base>::value << has_flag_void_shift)
        + (has_method_shutdown_ec<Base>::value << has_flag_arg1_shift)
        + (has_method_shutdown_type<Base>::value << has_flag_arg2_shift)
        + (has_method_shutdown_type_ec<Base>::value << has_flag_arg3_shift)
      > {};

// ------------ close
template <typename T, typename Base, int flags>
struct forward_close_helper {};

template <typename T, typename Base>
struct forward_close_helper<T, Base, has_flag_void> {
    void close() {
        static_cast<T&>(*this).next_layer().close();
    }
};

template <typename T, typename Base>
struct forward_close_helper<T, Base, has_flag_arg1> {
    void close(boost::system::error_code & ec) {
        static_cast<T&>(*this).next_layer().close(ec);
    }
};

template <typename T, typename Base>
struct forward_close_helper<T, Base, has_flag_void + has_flag_arg1> {
    void close() {
        static_cast<T&>(*this).next_layer().close();
    }

    void close(boost::system::error_code & ec) {
        static_cast<T&>(*this).next_layer().close(ec);
    }
};

template <typename T, typename Base>
struct forward_close
    : public forward_close_helper<T, Base,
        (has_method_close<Base>::value << has_flag_void_shift)
        + (has_method_close_ec<Base>::value << has_flag_arg1_shift)
      > {};


} //namespace fast_asio
