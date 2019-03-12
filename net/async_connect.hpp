#pragma once

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/handler_invoke_hook.hpp>

#include <iterator>
#include <utility>

#include <unistd.h>
#include <fcntl.h>

#include "o2logger/src/o2logger.hpp"
using namespace o2logger; // NOLINT


template<class Socket, class Endpoints, class PrepareSocket, class Handler>
class RangeConnectOp
{
    typedef typename Endpoints::value_type endpoint_type;
public:
    RangeConnectOp(Socket &sock, Endpoints endpoints, PrepareSocket prepare, Handler handler) :
        m_Socket(sock),
        m_Endpoints(std::move(endpoints)),
        m_Index(0),
        m_Prepare(std::move(prepare)),
        m_Handler(std::move(handler))
    { }

    RangeConnectOp(const RangeConnectOp &other) = default;
    RangeConnectOp(RangeConnectOp &&other) = default;

    void start()
    {
        auto iter = m_Endpoints.begin();
        assert(iter != m_Endpoints.end());
        start_connect(*iter);
    }

    void operator()(boost::system::error_code ec)
    {
        auto iter = m_Endpoints.begin();
        std::advance(iter, m_Index);

        if (!ec || ec == boost::asio::error::operation_aborted)
        {
            m_Handler(ec, ec ? typename Socket::endpoint_type() : *iter);
            return;
        }

        if (ec.message() == "No buffer space available")
        {
            loge("Failed to async_connect: ", ec.value());
        }

        ++iter;
        ++m_Index;

        if (iter != m_Endpoints.end())
        {
            start_connect(*iter);
        }
        else
        {
            m_Handler(ec, typename Socket::endpoint_type());
        }
    }

    // See this: http://www.boost.org/doc/libs/1_45_0/doc/html/boost_asio/reference/asio_handler_invoke.html
    // and this: https://sourceforge.net/p/asio/mailman/message/26851957/
    // and this: http://stackoverflow.com/questions/32857101/when-to-use-asio-handler-invoke
    // We need to overload this to make all the intermediate handlers (RangeConnectOp objects)
    // be invoked via the strand used by the user-provided handler.
    template<class Callable>
    friend void asio_handler_invoke(Callable function, RangeConnectOp *context)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(function, &context->m_Handler);
    }

private:
    void start_connect(const endpoint_type &endpoint)
    {
        boost::system::error_code ec;
        m_Socket.close(ec);

        ec = boost::system::error_code();
        m_Socket.open(endpoint.protocol(), ec);

        if (ec)
        {
            if (ec.message() == "No buffer space available")
            {
                loge("Failed to open a socket: ", ec.value());
            }

            m_Socket.get_io_service().post([handler = std::move(m_Handler), ec]() mutable
            {
                handler(ec, typename Socket::endpoint_type());
            });
            return;
        }

        ::fcntl(m_Socket.native(), F_SETFD, FD_CLOEXEC);
        m_Prepare(m_Socket);
        m_Socket.async_connect(endpoint, std::move(*this));
    }

private:
    Socket &m_Socket;
    Endpoints m_Endpoints;
    std::size_t m_Index;
    PrepareSocket m_Prepare;
    Handler m_Handler;
};

// `endpoints` must be non-empty.
template<class Socket, class Endpoints, class PrepareSocket, class Handler>
void asyncConnect(Socket &socket, Endpoints endpoints, PrepareSocket prepare, Handler handler)
{
    assert(endpoints.begin() != endpoints.end());

    RangeConnectOp<Socket, Endpoints, PrepareSocket, Handler> op(
        socket, std::move(endpoints), std::move(prepare), std::move(handler));

    op.start();
}
