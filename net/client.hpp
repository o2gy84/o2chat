#pragma once

#include <functional>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/system/error_code.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include "async_connect.hpp"
#include "common/http.hpp"
#include "common/utils.hpp"


std::vector<boost::asio::ip::tcp::endpoint> resolve(const std::string& host, int port);

struct ConnectionError
{
    boost::system::error_code code;
    std::string asString() const { return code.message(); }

    explicit ConnectionError(const boost::system::error_code &ec) :
        code(ec)
    {
    }
};

class BasicTcpClient: public boost::enable_shared_from_this<BasicTcpClient>
{
public:
    typedef boost::asio::buffers_iterator<boost::asio::streambuf::const_buffers_type> stream_iterator;

public:
    BasicTcpClient(boost::asio::io_service &io, bool ssl, unsigned int timeout);
    BasicTcpClient(boost::asio::io_service &io, boost::asio::ssl::context &ctx);

    ~BasicTcpClient();

    unsigned int timeout() const { return m_Timeout; }
    unsigned int setTimeout(unsigned int val);

    bool isSsl() const { return m_Ssl; }
    bool isOpen();
    void cancel();
    void close();

    std::size_t read(std::size_t need_bytes, int timeout = -1);
    std::size_t read_until(const std::string &delimiter, int timeout = -1);
    std::size_t write(std::string const& str);

    // stream_iterator - is typedef from abcstract client
    void asyncRead(std::function<std::pair<stream_iterator, bool>(stream_iterator, stream_iterator)> condition,
                   std::function<void(const ConnectionError &err, size_t)> handler);

    void asyncWrite(std::string data, std::function<void(const ConnectionError &err, size_t)> handler);

    std::string localAddr() const;
    std::string remoteAddr() const;

    const std::string& response() const { return m_Response; }
    void swapResponse(std::string &res) { std::swap(m_Response, res); }
    boost::asio::streambuf& response_stream() { return m_Streambuf; }
    void shrinkToFit();

    void makeConnected(boost::system::error_code &ec);

    boost::asio::io_service &ioService();

    boost::asio::ip::tcp::socket::lowest_layer_type &lowestLayer();
    const boost::asio::ip::tcp::socket::lowest_layer_type &lowestLayer() const;

    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> &ssl_stream() { return m_SslBackend; }
    boost::asio::ip::tcp::socket &socket() { return m_Backend; }

protected:
    int m_Timeout;
    bool m_IsOpen;
    bool m_Ssl;

    boost::asio::ip::tcp::socket m_Backend;
    boost::asio::ssl::context m_SslContext;
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_SslBackend;

    std::string m_WriteBuffer;
    boost::asio::streambuf m_Streambuf;
    std::string m_Response;

    std::string m_RemoteAddr;
    std::string m_LocalAddr;
};


class TcpClient: public BasicTcpClient
{
public:
    TcpClient(boost::asio::io_service &io, bool ssl, unsigned int timeout);
    TcpClient(boost::asio::io_service &io, boost::asio::ssl::context &ctx);
    ~TcpClient();

    void connect(const std::string& host, int port);

    template<class Prepare, class Handler>
    void asyncConnect(const std::string& host, int port, Prepare prepare, Handler handler)
    {
        std::vector<boost::asio::ip::tcp::endpoint> hosts;
        try
        {
            hosts = ::resolve(host, port);
        }
        catch (const std::exception& e)
        {
            ConnectionError error(boost::asio::error::no_recovery);
            handler(error);
            return;
        }

        ::asyncConnect(
            lowestLayer(),
            hosts,
            prepare,
            [this, self = shared_from_this(), handler = std::move(handler)](auto ec, auto endpoint) mutable
            {
                unused_args(endpoint);
                if (!ec) this->makeConnected(ec);
                handler(ConnectionError(ec));
            });
    }
};


class AsyncHttpClient
{
public:
    explicit AsyncHttpClient(boost::shared_ptr<TcpClient> &socket);
    ~AsyncHttpClient();

    boost::asio::io_service &ioService()
    {
        if (!m_Socket) throw std::runtime_error("not initialized tcp client");
        return m_Socket->ioService();
    }

    bool isOpen() const { return m_Socket && m_Socket->isOpen(); }
    void cancel() { if (m_Socket) m_Socket->cancel(); }
    void close() { if (m_Socket) m_Socket->close(); }

    std::string remoteAddr() const { return m_Socket->remoteAddr(); }
    std::string localAddr()  const { return m_Socket->localAddr();  }

    void asyncConnect(const std::string &host, uint32_t port, std::function<void(const ConnectionError &err)> handler);
    void asyncResponse(std::function<void(const ConnectionError &err, const HttpReply &r)> handler);
    void asyncRequest(std::string request, std::function<void(const ConnectionError &err)> handler);
    void asyncHandshakeAsServer(std::function<void(const ConnectionError &err)> handler);
private:
    template<class Handler>
    void handleConnect(ConnectionError error, Handler handler);

    template<class Handler>
    void handleHandshake(ConnectionError error, Handler handler);

    template<class Handler>
    void handleHeaders(ConnectionError error, Handler handler);

    template<class Handler>
    void processReply(HttpReply reply, Handler handler);

    template<class Handler>
    void startProcessChunk(ConnectionError error, HttpReply reply, Handler handler);

    template<class Handler>
    void processChunk(ConnectionError error, size_t chunk_size, HttpReply reply, Handler handler);

private:
    boost::shared_ptr<TcpClient> m_Socket;
};

