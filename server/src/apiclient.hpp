#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/noncopyable.hpp>
#include <boost/make_shared.hpp>

#include "apiclient_utils.hpp"
#include "request.hpp"
#include "database_worker.hpp"
#include "net/client.hpp"
#include "common/common.hpp"


class ApiClient: public boost::enable_shared_from_this<ApiClient>, private boost::noncopyable
{
public:
    ApiClient(boost::shared_ptr<TcpClient> socket, DatabaseWorker &db);
    ~ApiClient();

    void serveSslClient();
    void sendOkResponse(const std::string &body);
    void sendMessagesToIdleConn(std::vector<apiclient_utils::Message> &&msgs);
    void sendMessages(std::vector<apiclient_utils::Message> &&msgs);

    void sendErrorResponse(int http_code, common::ApiStatusCode api_code, const std::string &desc);

private:
    void sendOkResponseAndStartIdle();

private:
    void processClientRequest(const ConnectionError &error);
    void readCmd();

private:
    void timerWaitTaskResultHandler(const boost::system::error_code& e);

private:
    void v1_handler(const HttpReply &req, common::cmd_t cmd);
    void handler_impl(const HttpReply &req, common::cmd_t command);

private:
    void requestFromClientReadHandler(const ConnectionError &error, const HttpReply &reply);
    void responseToClientWroteHandler(const ConnectionError &error);

private:
    std::string buildApiOkResponse(const std::string &body);

private:
    int m_HttpCode;                                          // http response code
    int m_StoragePollingIntervalMs;
    uint64_t m_NewestMsgTimestamp = 0;                       // notify client only about new messages
    uint64_t m_LastClientPing = 0;                           // from time to time we need to ping client

    RequestDetails m_RequestDetails;

    boost::shared_ptr<AsyncHttpClient> m_Client;             // http socket, request from client
    boost::asio::steady_timer m_Timer;

    std::chrono::time_point<std::chrono::steady_clock> m_Start;

    DatabaseWorker &m_Db;
};
