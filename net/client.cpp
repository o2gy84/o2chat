#include <boost/asio/steady_timer.hpp>
#include <zlib.h>
#include <mutex>

#include "net/client.hpp"
#include "net/sync_connect.hpp"
#include "common/utils.hpp"

#include "o2logger/src/o2logger.hpp"
using namespace o2logger; // NOLINT


std::vector<boost::asio::ip::tcp::endpoint> resolve(const std::string& host, int port)
{
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::resolver resolver(io_service);

    boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), host, std::to_string(port));    // IPv4 only
    boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

    std::vector<boost::asio::ip::tcp::endpoint> valid_domains;

    int ips_found = 0;
    for ( ; iterator != boost::asio::ip::tcp::resolver::iterator(); ++iterator)
    {
        ++ips_found;
        valid_domains.push_back(iterator->endpoint());
    }
    std::random_shuffle(valid_domains.begin(), valid_domains.end());

    if (valid_domains.empty() && ips_found != 0)
    {
        throw std::runtime_error("connect failed : all resolved IPs are blacklisted");
    }
    else if (ips_found == 0)
    {
        throw std::runtime_error("connect failed : " + boost::system::error_code(boost::asio::error::not_found).message());
    }

    return valid_domains;
}

namespace
{

std::string addressToString(const boost::asio::ip::tcp::endpoint &endpoint, boost::system::error_code &ec)
{
    return endpoint.address().to_string(ec);
}

void read_completed(boost::asio::io_service& local_io,
                    const boost::system::error_code& error,
                    size_t transferred,
                    size_t &bytes,
                    boost::optional<boost::system::error_code> *result,
                    boost::asio::steady_timer &timer)
{
    if (error == boost::asio::error::operation_aborted)
    {
        return;
    }

    timer.cancel();

    result->reset(error);
    if (!error)
    {
        bytes = transferred;
    }

    local_io.post([]() { /*dummy handler, just to notify local_io about read event*/ });
}

void timeout_expired(const boost::system::error_code &error,
                     boost::asio::ip::tcp::socket::lowest_layer_type &socket,
                     boost::optional<boost::system::error_code> *result,
                     boost::asio::io_service &local_io)
{
    if (error == boost::asio::error::operation_aborted)
    {
        return;
    }

    socket.cancel();            // <-- do not do this concurrently with read handler
    result->reset(error);
    local_io.post([]() { /*dummy handler, just to notify local_io about timeout event*/ });
}

void read_from_buffer(boost::asio::streambuf& sbuf, std::string& resp, std::string const& delimiter, size_t bytes)
{
    boost::asio::streambuf::const_buffers_type bufs = sbuf.data();
    resp.assign(boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs) + bytes - delimiter.size());
    sbuf.consume(bytes);
}

void sync_connect(boost::asio::ip::tcp::socket::lowest_layer_type& socket, const std::string& host, int port, int timeout)
{
    boost::asio::steady_timer timer(socket.get_io_service());
    timer.expires_from_now(std::chrono::seconds(timeout));

    SyncConnectContext context(socket, timer);
    context.endpoints = resolve(host, port);

    auto timer_callback = [&context](auto ec)
    {
        std::unique_lock<std::mutex> lock(context.mutex);
        context.timer_finished = true;
        if (!context.connect_finished)
        {
            context.socket.cancel(ec);
        }
        context.cv.notify_all();
    };

    {
        std::unique_lock<std::mutex> lock(context.mutex);
        timer.async_wait(timer_callback);
        SyncConnector connector(context);
        connector.start();
    }

    {
        std::unique_lock<std::mutex> lock(context.mutex);

        while (!context.timer_finished || !context.connect_finished)
        {
            context.cv.wait(lock);

            // Внешшний io_service могли остановить?
            // boost::asio::io_service &io = socket.get_io_service();
            // if (io.stopped())
            // {
                // break;
            // }
        }
    }

    if (context.connect_result)
    {
        throw std::runtime_error("connect failed : " + context.connect_result.message());
    }
}

}  // namespace


BasicTcpClient::BasicTcpClient(boost::asio::io_service &io, bool ssl, unsigned int timeout) :
    m_Timeout(timeout),
    m_IsOpen(false),
    m_Ssl(ssl),
    m_Backend(io),
    m_SslContext(boost::asio::ssl::context::sslv23_client),
    m_SslBackend(io, m_SslContext)
{
    logd5("+BasicTcpClient created");
}

BasicTcpClient::BasicTcpClient(boost::asio::io_service &io, boost::asio::ssl::context &ctx) :
    m_IsOpen(false),
    m_Ssl(true),
    m_Backend(io),
    m_SslContext(boost::asio::ssl::context::sslv23_client),
    m_SslBackend(io, ctx)
{
    logd5("+BasicTcpClient created");
}

BasicTcpClient::~BasicTcpClient()
{
    logd5("~BasicTcpClient destroyed");
}

boost::asio::io_service& BasicTcpClient::ioService()
{
    if (m_Ssl) return m_SslBackend.get_io_service();
    return m_Backend.get_io_service();
}

boost::asio::ip::tcp::socket::lowest_layer_type& BasicTcpClient::lowestLayer()
{
    if (m_Ssl) return m_SslBackend.lowest_layer();
    return m_Backend.lowest_layer();
}
const boost::asio::ip::tcp::socket::lowest_layer_type& BasicTcpClient::lowestLayer() const
{
    if (m_Ssl) return m_SslBackend.lowest_layer();
    return m_Backend.lowest_layer();
}

unsigned int BasicTcpClient::setTimeout(unsigned int val)
{
    unsigned int tmp = m_Timeout;
    m_Timeout = val;
    return tmp;
}

std::size_t BasicTcpClient::read_until(const std::string &delimiter, int timeout)
{
    if (timeout < 0)
    {
        timeout = m_Timeout;
    }

    size_t bytes = 0;
    m_Response.clear();

    if (m_Streambuf.size())
    {
        boost::asio::streambuf::const_buffers_type bufs = m_Streambuf.data();
        auto const beg = boost::asio::buffers_begin(bufs);
        auto const end = boost::asio::buffers_end(bufs);
        auto const pos = std::search(beg, end, delimiter.begin(), delimiter.end());

        if (pos != end)
        {
            m_Response.assign(beg, pos);
            m_Streambuf.consume(std::distance(beg, pos) + delimiter.size());
            return m_Response.size();
        }
    }

    boost::asio::io_service local_io;
    local_io.reset();

    boost::optional<boost::system::error_code> timer_result;
    boost::optional<boost::system::error_code> read_result;

    boost::asio::ip::tcp::socket::lowest_layer_type &sock = lowestLayer();
    auto timeout_cb = boost::bind(timeout_expired, boost::asio::placeholders::error, std::ref(sock), &timer_result, std::ref(local_io));

    boost::asio::io_service &external_io = ioService();
    boost::asio::io_service::strand strand(external_io);

    boost::asio::steady_timer timer(external_io);
    timer.expires_from_now(std::chrono::seconds(timeout));
    timer.async_wait(strand.wrap(timeout_cb));

    auto read_callback = strand.wrap(boost::bind(read_completed,
                                                      std::ref(local_io),
                                                      boost::asio::placeholders::error,
                                                      boost::asio::placeholders::bytes_transferred,
                                                      std::ref(bytes),
                                                      &read_result,
                                                      std::ref(timer)));

    if (m_Ssl)
    {
        boost::asio::async_read_until(m_SslBackend, m_Streambuf, delimiter, read_callback);
    }
    else
    {
        boost::asio::async_read_until(m_Backend, m_Streambuf, delimiter, read_callback);
    }

    boost::asio::io_service::work work(local_io);   // чтобы дождаться хендлера чтения или таймера
    local_io.run_one();

    if (timer_result)
        throw std::runtime_error("async read timeout");

    if (read_result && *read_result)
        throw std::runtime_error("read failed: " + read_result->message());

    read_from_buffer(m_Streambuf, m_Response, delimiter, bytes);
    return bytes;
}

std::size_t BasicTcpClient::read(std::size_t need_bytes, int timeout)
{
    if (timeout < 0)
    {
        timeout = m_Timeout;
    }

    m_Response.clear();
    m_Response.resize(need_bytes);
    char *resp = const_cast<char *>(m_Response.data());

    size_t already_in_stream = std::min(need_bytes, m_Streambuf.size());
    if (already_in_stream)
    {
        boost::asio::streambuf::const_buffers_type bufs = m_Streambuf.data();
        std::copy(boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs) + already_in_stream, resp);
        m_Streambuf.consume(already_in_stream);
    }
    if (already_in_stream == need_bytes) return need_bytes;

    boost::asio::io_service local_io;
    local_io.reset();

    boost::optional<boost::system::error_code> timer_result;
    boost::optional<boost::system::error_code> read_result;

    boost::asio::ip::tcp::socket::lowest_layer_type &sock = lowestLayer();
    auto timeout_cb = boost::bind(timeout_expired, boost::asio::placeholders::error, std::ref(sock), &timer_result, std::ref(local_io));

    boost::asio::io_service &external_io = ioService();
    boost::asio::io_service::strand strand(external_io);

    boost::asio::steady_timer timer(external_io);
    timer.expires_from_now(std::chrono::seconds(timeout));
    timer.async_wait(strand.wrap(timeout_cb));

    auto buffer = boost::asio::buffer(resp + already_in_stream, need_bytes - already_in_stream);
    auto condition = boost::asio::transfer_exactly(need_bytes - already_in_stream);

    size_t bytes = 0;
    auto read_callback = strand.wrap(boost::bind(read_completed,
                                                      std::ref(local_io),
                                                      boost::asio::placeholders::error,
                                                      boost::asio::placeholders::bytes_transferred,
                                                      std::ref(bytes),
                                                      &read_result,
                                                      std::ref(timer)));

    if (m_Ssl)
    {
        boost::asio::async_read(m_SslBackend, buffer, condition, read_callback);
    }
    else
    {
        boost::asio::async_read(m_Backend, buffer, condition, read_callback);
    }

    boost::asio::io_service::work work(local_io);   // чтобы дождаться хендлера чтения или таймера
    local_io.run_one();

    if (timer_result)
        throw std::runtime_error("async read timeout");

    if (read_result && *read_result)
        throw std::runtime_error("read failed: " + read_result->message());

    return need_bytes;
}

std::size_t BasicTcpClient::write(std::string const& str)
{
    boost::system::error_code ec;
    std::size_t bytes = 0;
    while (bytes < str.size())
    {
        std::size_t w = 0;
        if (m_Ssl)
        {
            w = m_SslBackend.write_some(boost::asio::buffer(str.data() + bytes, str.size() - bytes), ec);
        }
        else
        {
            w = m_Backend.write_some(boost::asio::buffer(str.data() + bytes, str.size() - bytes), ec);
        }
        if (ec) break;
        bytes += w;
    }

    if (ec) throw std::runtime_error("write failed: " + ec.message());
    return bytes;
}

void BasicTcpClient::asyncRead(std::function<std::pair<stream_iterator, bool>(stream_iterator, stream_iterator)> condition,
                               std::function<void(const ConnectionError &err, size_t)> handler)
{
    if (m_Ssl)
    {
        boost::asio::async_read_until(
            m_SslBackend,
            m_Streambuf,
            std::move(condition),
            [self_life = this->shared_from_this(), handler = std::move(handler)](boost::system::error_code ec, size_t transferred) mutable
            {
                ConnectionError conn_err(ec);
                handler(conn_err, transferred);
            });
    }
    else
    {
        boost::asio::async_read_until(
            m_Backend,
            m_Streambuf,
            std::move(condition),
            [self_life = this->shared_from_this(), handler = std::move(handler)](boost::system::error_code ec, size_t transferred) mutable
            {
                ConnectionError conn_err(ec);
                handler(conn_err, transferred);
            });
    }
}

void BasicTcpClient::asyncWrite(std::string data, std::function<void(const ConnectionError &err, size_t bytes)> handler)
{
    m_WriteBuffer = std::move(data);

    if (m_Ssl)
    {
        boost::asio::async_write(
            m_SslBackend,
            boost::asio::buffer(m_WriteBuffer.data(), m_WriteBuffer.size()),
            [self = this->shared_from_this(), handler = std::move(handler)](boost::system::error_code ec, size_t transferred) mutable
            {
                handler(ConnectionError(ec), transferred);
            });
    }
    else
    {
        boost::asio::async_write(
            m_Backend,
            boost::asio::buffer(m_WriteBuffer.data(), m_WriteBuffer.size()),
            [self = this->shared_from_this(), handler = std::move(handler)](boost::system::error_code ec, size_t transferred) mutable
            {
                handler(ConnectionError(ec), transferred);
            });
    }
}

std::string BasicTcpClient::localAddr() const
{
    return m_LocalAddr;
}

std::string BasicTcpClient::remoteAddr() const
{
    return m_RemoteAddr;
}

void BasicTcpClient::shrinkToFit()
{
    m_WriteBuffer = std::string();
}

bool BasicTcpClient::isOpen()
{
    if (!m_IsOpen)
        return false;

    try
    {
        return lowestLayer().is_open();
    }
    catch (std::exception& e)
    {
        loge("[PIZDEC] %s", e.what());
    }

    return false;
}

void BasicTcpClient::cancel()
{
    boost::system::error_code ec;
    lowestLayer().cancel(ec);
}

void BasicTcpClient::close()
{
    m_IsOpen = false;
    boost::system::error_code ignored_ec;
    lowestLayer().close(ignored_ec);
}

void BasicTcpClient::makeConnected(boost::system::error_code &ec)
{
    m_IsOpen = true;

    auto remote_endpoint = lowestLayer().remote_endpoint(ec);

    if (ec)
    {
        return;
    }

    m_RemoteAddr = addressToString(remote_endpoint, ec);

    if (ec)
    {
        return;
    }

    auto local_endpoint = lowestLayer().local_endpoint(ec);

    if (ec)
    {
        return;
    }

    m_LocalAddr = addressToString(local_endpoint, ec);
}


TcpClient::TcpClient(boost::asio::io_service &io, bool ssl, unsigned int timeout):
    BasicTcpClient(io, ssl, timeout)
{
}

TcpClient::TcpClient(boost::asio::io_service &io, boost::asio::ssl::context &ctx):
    BasicTcpClient(io, ctx)
{
}

TcpClient::~TcpClient()
{
}

void TcpClient::connect(const std::string& host, int port)
{
    ::sync_connect(lowestLayer(), host, port, m_Timeout);

    boost::system::error_code ec;
    makeConnected(ec);

    if (ec)
    {
        throw std::runtime_error("make connected failed: " + ec.message());
    }
}


namespace
{

template<typename T> static bool parseHeaderLine(const std::string &line, T &parsed_headers)
{
    std::string::size_type pos = line.find_first_of(':');
    if ((pos == std::string::npos) || (pos == 0))
        return false;

    std::string key = line.substr(0, pos);
    if (pos == line.size() - 1)
    {
        parsed_headers[key] = "";
        return true;
    }

    ++pos;
    while (isspace(line[pos])) ++pos;
    std::string value = line.substr(pos, std::string::npos);

    parsed_headers[key] = value;
    return true;
}

bool gzipInflate(std::string const& compressedBytes, std::string& uncompressedBytes)
{
    uncompressedBytes.clear();

    if (compressedBytes.size() == 0)
        return true;

    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    strm.next_in = (Bytef *)compressedBytes.c_str();  // This can be unsafe ?   NOLINT
    strm.avail_in = compressedBytes.size();

    if (inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK)
    {
        loge("[GZIP] Can't init stream: %s", strm.msg ? strm.msg : "no error");
        return false;
    }

    size_t half_length = compressedBytes.size() / 2;
    std::vector<char> buffer(half_length * 3);

    int status = Z_OK;
    do
    {
        if (strm.total_out >= buffer.size())
            buffer.resize(buffer.size() + half_length);

        strm.next_out = (Bytef *)(&buffer[0] + strm.total_out);  // NOLINT
        strm.avail_out = buffer.size() - strm.total_out;

        status = inflate(&strm, Z_SYNC_FLUSH);  // will return Z_STREAM_END at end
    }
    while (status == Z_OK);  // NOLINT

    if (inflateEnd(&strm) != Z_OK && status != Z_STREAM_END)
    {
        loge("[GZIP] Can't decode stream: %s", strm.msg ? strm.msg : "no error");
        return false;
    }

    uncompressedBytes = std::string(&buffer[0], strm.total_out);
    return true;
}

struct contains_t
{
    std::string delimiter;

    template<class It>
    std::pair<It, bool> operator()(It begin, It end) const
    {
        auto pos = std::search(begin, end, delimiter.begin(), delimiter.end());
        return std::make_pair((pos != end) ? pos : begin, pos != end);
    }
};

struct exactly_t
{
    explicit exactly_t(size_t size) : _size(size) {}
    size_t _size;

    template<class It>
    std::pair<It, bool> operator()(It begin, It end) const
    {
        size_t size = end - begin;
        if (size >= _size)
        {
            return std::make_pair(begin + size, true);
        }
        return std::make_pair(begin, false);
    }
};


}  // namespace

AsyncHttpClient::AsyncHttpClient(boost::shared_ptr<TcpClient> &socket) :
        m_Socket(socket)
{
    logd5("+AsyncHttpClient created");
}


AsyncHttpClient::~AsyncHttpClient()
{
    logd5("~AsyncHttpClient destroyed");
}


void AsyncHttpClient::asyncConnect(const std::string &host, uint32_t port, std::function<void(const ConnectionError &err)> handler)
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

    auto new_handler = [self = this, handler](const ConnectionError &read_error) mutable
    {
        self->handleConnect(read_error, handler);
    };

    ::asyncConnect(
        m_Socket->lowestLayer(),
        hosts,
        [](auto &socket) { unused_args(socket); },
        [this, handler = std::move(new_handler)](auto ec, auto endpoint) mutable
        {
            unused_args(endpoint);
            if (!ec) m_Socket->makeConnected(ec);
            handler(ConnectionError(ec));
        });
}

template<class Handler>
void AsyncHttpClient::handleConnect(ConnectionError error, Handler handler)
{
    if (error.code)
    {
        handler(error);
        return;
    }

    if (!m_Socket->isSsl())
    {
        handler(ConnectionError(boost::system::error_code()));
        return;
    }

    auto new_handler = [self = this, handler](const boost::system::error_code& error) mutable
    {
        self->handleHandshake(ConnectionError(error), handler);
    };

    m_Socket->ssl_stream().async_handshake(boost::asio::ssl::stream_base::client, new_handler);
}

template<class Handler>
void AsyncHttpClient::handleHandshake(ConnectionError error, Handler handler)
{
    if (error.code)
    {
        handler(error);
        return;
    }

    handler(ConnectionError(boost::system::error_code()));
}

void AsyncHttpClient::asyncHandshakeAsServer(std::function<void(const ConnectionError &err)> handler)
{
    if (!m_Socket->isSsl())
    {
        handler(ConnectionError(boost::system::error_code()));
        return;
    }

    auto new_handler = [self = this, handler](const boost::system::error_code& error) mutable
    {
        handler(ConnectionError(error));
    };

    m_Socket->ssl_stream().async_handshake(boost::asio::ssl::stream_base::server, new_handler);
}

void AsyncHttpClient::asyncResponse(std::function<void(const ConnectionError &err, const HttpReply &r)> handler)
{
    m_Socket->asyncRead(contains_t{"\r\n\r\n"}, [self = this, handler](const ConnectionError &read_error, size_t) mutable
    {
        self->handleHeaders(read_error, handler);
    });
}

void AsyncHttpClient::asyncRequest(std::string request, std::function<void(const ConnectionError &err)> handler)
{
    m_Socket->asyncWrite(request,
        [handler](const ConnectionError &write_error, size_t bytes) mutable
        {
            unused_args(bytes);
            handler(write_error);
            return;
        });
}

template<class Handler>
void AsyncHttpClient::handleHeaders(ConnectionError error, Handler handler)
{
    if (error.code)
    {
        handler(error, {});
        return;
    }

    HttpReply reply;

    // Читаем заголовки
    m_Socket->read_until("\r\n\r\n");
    m_Socket->swapResponse(reply._headers);

    std::vector<std::string> h_vector = utils::split(reply._headers, "\r\n");
    if (h_vector.empty() || h_vector[0].empty())
    {
        boost::system::error_code protocol_error(boost::system::errc::protocol_error, boost::system::generic_category());
        handler(ConnectionError(protocol_error), reply);
        return;
    }

    std::vector<std::string> response_params = utils::split(h_vector[0], " ");
    if (response_params.size() < 2)
    {
        boost::system::error_code protocol_error(boost::system::errc::protocol_error, boost::system::generic_category());
        handler(ConnectionError(protocol_error), reply);
        return;
    }

    std::string lc = utils::lowercased(response_params[0]);
    if (utils::starts_with(lc, "http/"))
    {
        // status string like: "HTTP/1.1 200 OK" or "HTTP/1.1 301 Moved Permanently"
        try
        {
            reply._status = std::stoi(response_params[1]);
        }
        catch (const std::exception &e)
        {
            f::loge("http status: {0}, error: {1}", response_params[1], e.what());
            boost::system::error_code protocol_error(boost::system::errc::protocol_error, boost::system::generic_category());
            handler(ConnectionError(protocol_error), reply);
            return;
        }
    }
    else
    {
        // POST / HTTP/1.1
        reply._method = lc;
        reply._resource = response_params[1];
    }

    for (size_t i = 1; i < h_vector.size(); ++i)
    {
        parseHeaderLine(h_vector[i], reply._parsedHeaders);
    }

    bool has_content_len = reply.hasHeader("Content-Length");
    size_t body_len = 0;
    if (has_content_len)
    {
        try
        {
            body_len = std::stoul(reply.getHeader("Content-Length"));
        }
        catch (const std::exception &e)
        {
            f::loge("content len: {0}, error: {1}", reply.getHeader("Content-Length"), e.what());
            boost::system::error_code protocol_error(boost::system::errc::protocol_error, boost::system::generic_category());
            handler(ConnectionError(protocol_error), reply);
            return;
        }
    }

    if (has_content_len)
    {
        m_Socket->asyncRead(
            [body_len](auto begin, auto end)
            {
                size_t size = end - begin;
                return std::make_pair((size >= body_len) ? begin + body_len : begin, size >= body_len);
            },

            [self = this, handler, reply = std::move(reply), body_len](const ConnectionError &read_error, size_t) mutable
            {
                if (read_error.code)
                {
                    handler(read_error, reply);
                }
                else
                {
                    self->m_Socket->read(body_len);
                    self->m_Socket->swapResponse(reply._body);
                    self->processReply(std::move(reply), handler);
                }
            });
    }
    else if (reply.getHeader("Transfer-Encoding") == "chunked")
    {
        m_Socket->asyncRead(contains_t{"\r\n"},
            [self = *this, reply = std::move(reply), handler](ConnectionError read_error, size_t) mutable
            {
                self.startProcessChunk(read_error, std::move(reply), handler);
            });
    }
    else
    {
        // request without body
        handler(ConnectionError(boost::system::error_code()), reply);
    }
}

template<class Handler>
void AsyncHttpClient::processReply(HttpReply reply, Handler handler)
{
    bool content_zipped = reply.hasHeader("Content-Encoding") &&
                          reply.getHeader("Content-Encoding").find("gzip") != std::string::npos;

    if (content_zipped && !reply._body.empty())
    {
        std::string r;
        if (!gzipInflate(reply._body, r))
        {
            boost::system::error_code protocol_error(boost::system::errc::protocol_error, boost::system::generic_category());
            handler(ConnectionError(protocol_error), reply);
            return;
        }
        reply._body = std::move(r);
    }

    handler(ConnectionError(boost::system::error_code()), reply);
}

template<class Handler>
void AsyncHttpClient::startProcessChunk(ConnectionError error, HttpReply reply, Handler handler)
{
    if (error.code)
    {
        handler(error, {});
        return;
    }

    std::string chunk_size_string;
    m_Socket->read_until("\r\n");
    m_Socket->swapResponse(chunk_size_string);

    size_t parsed = 0;
    size_t chunk_size = 0;

    try
    {
        chunk_size = std::stoul(chunk_size_string, &parsed, 16);
    }
    catch (const std::exception &e)
    {
        boost::system::error_code protocol_error(boost::system::errc::protocol_error, boost::system::generic_category());
        handler(ConnectionError(protocol_error), reply);
        return;
    }

    if (parsed != chunk_size_string.size())
    {
        boost::system::error_code protocol_error(boost::system::errc::protocol_error, boost::system::generic_category());
        handler(ConnectionError(protocol_error), reply);
        return;
    }

    m_Socket->asyncRead(
        [chunk_size](auto begin, auto end)
        {
            size_t size = end - begin;
            return std::make_pair((size >= chunk_size + 2) ? (begin + chunk_size + 2) : begin, size >= chunk_size + 2);
        },
        [self = *this, handler, reply = std::move(reply), chunk_size](auto read_error, size_t) mutable
        {
            self.processChunk(read_error, chunk_size, std::move(reply), handler);
        });
}

template<class Handler>
void AsyncHttpClient::processChunk(ConnectionError error, size_t chunk_size, HttpReply reply, Handler handler)
{
    if (error.code)
    {
        handler(error, reply);
        return;
    }

    m_Socket->read(chunk_size + 2);

    reply._body += m_Socket->response().substr(0, chunk_size);

    if (chunk_size == 0)
    {
        processReply(std::move(reply), handler);
    }
    else
    {
        m_Socket->asyncRead(contains_t{"\r\n"},
            [self = *this, reply = std::move(reply), handler](auto read_error, size_t) mutable
            {
                self.startProcessChunk(read_error, std::move(reply), handler);
            });
    }
}

bool HttpReply::hasHeader(const std::string &header) const
{
    return (_parsedHeaders.find(header) != _parsedHeaders.end());
}

std::string HttpReply::getHeader(const std::string &header) const
{
    auto it = _parsedHeaders.find(header);
    if (it != _parsedHeaders.end()) return it->second;
    return std::string("");
}

