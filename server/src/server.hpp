#pragma once

#include <boost/noncopyable.hpp>
#include <boost/make_shared.hpp>

#include "net/client.hpp"
#include "database_worker.hpp"
#include "common/io_thread.hpp"


class Server: private boost::noncopyable
{
public:
    Server(int port, int io_thread_pool_size);
    ~Server();

    void run();

private:
    void startAccept();
    void handleStop();
    void handleHUP();

    void loop();

private:
    int m_IoPoolSize;

    std::unique_ptr<IoThread> m_MainIo;
    std::vector<std::unique_ptr<IoThread>> m_IoThreads;

    boost::asio::signal_set m_Signals;
    boost::asio::signal_set m_HupSignals;
    boost::asio::ip::tcp::acceptor m_Acceptor;

    DatabaseWorker m_Db;
};
