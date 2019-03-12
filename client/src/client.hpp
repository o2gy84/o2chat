#pragma once

#include <boost/noncopyable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/make_shared.hpp>

#include "net/client.hpp"

#include "event_worker.hpp"
#include "ui.hpp"
#include "apiclient_utils.hpp"
#include "common/io_thread.hpp"


class Client: private boost::noncopyable
{
public:
    Client(const std::string& address, int port, const std::string &user, const std::string &password);
    ~Client();

    void run();

private:
    enum class mode_t : uint8_t
    {
        USUAL,
        CHAT_U,
        CHAT_G
    };
    struct state_t
    {
        std::string user;
        mode_t mode = mode_t::USUAL;
    };

private:
    void handleStop();

private:
    void onUserInput(Event &&e);
    void sendRequest(input::cmd_t cmd, const std::string &req);

private:
    void onHttpConnect(const ConnectionError &e);
    void onHttpWrite(const ConnectionError &error);
    void onHttpRead(const ConnectionError &error, const HttpReply &reply);

    void onHttpIdleConnect(const ConnectionError &e);
    void onHttpIdleWrite(const ConnectionError &error);
    void onHttpIdleRead(const ConnectionError &error, const HttpReply &reply);

private:
    boost::shared_ptr<AsyncHttpClient> createConnect(bool use_ssl);

private:
    state_t m_State;

    std::unique_ptr<IoThread> m_IoThread;
    boost::asio::steady_timer m_Timeout;

    std::string m_Host;
    int m_Port;
    bool m_TimedOut;
    bool m_Ssl       = true;
    bool m_IdleState = false;

    std::unique_ptr<AbstractUi> m_Ui;
    EventsWorker m_EvWorker;

    boost::shared_ptr<AsyncHttpClient> m_Http;
    boost::shared_ptr<AsyncHttpClient> m_HttpIdle;

    input::cmd_t m_LastCmd = input::cmd_t::NONE;
    std::string m_User;
    std::string m_Password;
    uint64_t m_SelfId     = 0;
    uint64_t m_SelfChatId = 0;
    uint64_t m_HeartBit   = 0;
};
