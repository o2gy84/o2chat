#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "libproperty/src/libproperty.hpp"

#include <boost/make_shared.hpp>

#include <chrono>
#include <queue>
#include <thread>

#include "server.hpp"
#include "apiclient.hpp"
#include "common/utils.hpp"
#include "common/sysutils.hpp"

#include "o2logger/src/o2logger.hpp"
using namespace o2logger;


std::atomic<bool> g_NeedStop(false);


Server::Server(int port, int io_thread_pool_size) :
    m_IoPoolSize(io_thread_pool_size),
    m_MainIo(std::make_unique<IoThread>(libproperty::Options::impl()->get<std::string>("sert"))),
    m_Signals(m_MainIo->ioService()),
    m_HupSignals(m_MainIo->ioService()),
    m_Acceptor(m_MainIo->ioService()),
    m_Db(db::type_t::MEMORY, 5)
{
    m_Signals.add(SIGINT);
    m_Signals.add(SIGTERM);
    m_Signals.add(SIGQUIT);
    m_Signals.async_wait(std::bind(&Server::handleStop, this));

    m_HupSignals.add(SIGHUP);
    m_HupSignals.async_wait(std::bind(&Server::handleHUP, this));

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);

    m_Acceptor.open(endpoint.protocol());
    m_Acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    ::fcntl(m_Acceptor.native(), F_SETFD, FD_CLOEXEC);
    m_Acceptor.bind(endpoint);
    m_Acceptor.listen(2048);
}

Server::~Server()
{
}

void Server::handleStop()
{
    logi("stopped by signal");
    g_NeedStop = true;
    m_MainIo->ioService().stop();

    for (const auto &thread : m_IoThreads)
    {
        thread->stop();
    }
}

void Server::handleHUP()
{
    logi("sighup ignored");

    // TODO:
    // Config::reload();
    m_HupSignals.async_wait(std::bind(&Server::handleHUP, this));
}

void Server::run()
{
    m_MainIo->start();

    m_IoThreads.clear();
    for (size_t i = 0; i < m_IoPoolSize; ++i)
    {
        m_IoThreads.push_back(std::make_unique<IoThread>(libproperty::Options::impl()->get<std::string>("sert")));
        m_IoThreads.back()->start();
    }

    m_Db.run();

    startAccept();

    logi("chat server started");

    m_Db.join();
    m_MainIo->join();
    for (const auto &thread : m_IoThreads)
    {
        thread->join();
    }
}

void Server::startAccept()
{
    static unsigned int seedp = 42;
    auto &io_thread = m_IoThreads.at(rand_r(&seedp) % m_IoThreads.size());
    boost::shared_ptr<TcpClient> socket = boost::make_shared<TcpClient>(
        io_thread->ioService(), io_thread->sslContext());

    auto handler = [this, socket](const boost::system::error_code &e)
    {
        if (!e)
        {
            boost::system::error_code tmp;
            socket->makeConnected(tmp);
            if (!tmp)
            {
                boost::shared_ptr<ApiClient> c = boost::make_shared<ApiClient>(socket, m_Db);
                c->serveSslClient();
            }
        }
        startAccept();
    };

    m_Acceptor.async_accept(socket->lowestLayer(), handler);
}
