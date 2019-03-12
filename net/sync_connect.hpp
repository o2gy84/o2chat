#pragma once

#include <condition_variable>


struct SyncConnectContext
{
    std::vector<boost::asio::ip::tcp::endpoint> endpoints;
    size_t next_endpoint;

    std::condition_variable cv;
    std::mutex mutex;

    bool timer_finished;
    bool connect_finished;
    boost::system::error_code connect_result;

    boost::asio::ip::tcp::socket::lowest_layer_type &socket;
    boost::asio::steady_timer &timer;

    SyncConnectContext(boost::asio::ip::tcp::socket::lowest_layer_type &socket,
                       boost::asio::steady_timer &timer) :
        next_endpoint(0),
        timer_finished(false),
        connect_finished(false),
        socket(socket),
        timer(timer)
    { }
};

class SyncConnector
{
public:
    explicit SyncConnector(SyncConnectContext &context) : context(context) { }

    void start()
    {
        startConnect(boost::system::error_code());
    }

private:
    void async_connect_handler(boost::system::error_code ec)
    {
        std::unique_lock<std::mutex> lock(context.mutex);

        if (!ec)
        {
            finish_connect(boost::system::error_code());
            return;
        }

        if (context.timer_finished)
        {
            finish_connect(boost::asio::error::timed_out);
            return;
        }

        ++context.next_endpoint;
        startConnect(ec);
    }

    void startConnect(boost::system::error_code last_error)
    {
        if (context.next_endpoint >= context.endpoints.size())
        {
            finish_connect(last_error ? last_error : boost::asio::error::not_found);
            return;
        }

        auto &endpoint = context.endpoints[context.next_endpoint];

        boost::system::error_code ignored_error;
        context.socket.close(ignored_error);

        boost::system::error_code open_error;
        context.socket.open(endpoint.protocol(), open_error);

        if (open_error)
        {
            finish_connect(open_error);
            return;
        }

        ::fcntl(context.socket.native(), F_SETFD, FD_CLOEXEC);

        auto handler = [this](boost::system::error_code ec)
        {
            async_connect_handler(ec);
        };
        context.socket.async_connect(endpoint, handler);
    }

    void finish_connect(boost::system::error_code ec)
    {
        context.connect_result = ec;
        context.connect_finished = true;

        boost::system::error_code ignored_error;
        context.timer.cancel(ignored_error);

        context.cv.notify_all();
    }

private:
    SyncConnectContext &context;
};
