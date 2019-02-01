#pragma once
#include <boost/system/error_code.hpp>

namespace fast_asio {

enum class e_fast_asio_error_code : int
{
    ec_ok = 0,
    ec_parse_error = 1,
    ec_close_error = 2,
};

class fast_asio_error_category
    : public boost::system::error_category
{
public:
    virtual const char* name() const throw() {
        return "fast_asio_error";
    }

    virtual std::string message(int code) const {
        switch (code) {
            case (int)e_fast_asio_error_code::ec_ok:
                return "success";

            case (int)e_fast_asio_error_code::ec_parse_error:
                return "packet parse error";

            case (int)e_fast_asio_error_code::ec_close_error:
                return "close error";
        }

        return "unkown fast asio error code";
    }

    static const fast_asio_error_category & instance() {
        static fast_asio_error_category obj;
        return obj;
    }
};

inline boost::system::error_code create_fast_asio_error(int code) {
    return boost::system::error_code(code, fast_asio_error_category::instance());
}

inline boost::system::error_code create_fast_asio_error(e_fast_asio_error_code code) {
    return create_fast_asio_error((int)code);
}

inline boost::system::error_code packet_parse_error() {
    return create_fast_asio_error(e_fast_asio_error_code::ec_parse_error);
}

} //namespace fast_asio
