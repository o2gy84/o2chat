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

#include "client.hpp"
#include "common/utils.hpp"
#include "common/sysutils.hpp"

#include "o2logger/src/o2logger.hpp"
using namespace o2logger; // NOLINT


std::atomic<bool> g_NeedStop(false);

Client::Client(const std::string& host, int port, const std::string &user, const std::string &password) :
    m_IoThread(std::make_unique<IoThread>("")),
    m_Timeout(m_IoThread->ioService()),
    m_Host(host),
    m_Port(port),
    m_EvWorker(1),
    m_User(user),
    m_Password(password)
{
    m_Ui = std::unique_ptr<AbstractUi>(new ConsoleUi(m_EvWorker));

    m_Ssl = true;
    m_Http = createConnect(m_Ssl);
    m_HttpIdle = createConnect(m_Ssl);
    m_TimedOut = false;

    auto on_input = [this](Event &&e)
    {
        onUserInput(std::move(e));
    };
    m_EvWorker.registerOnInputCallback(std::move(on_input));
}

Client::~Client()
{
}

void Client::sendRequest(input::cmd_t cmd, const std::string &req)
{
    m_LastCmd = cmd;
    m_Http->asyncRequest(req, [this](const ConnectionError &error)
                              {
                                onHttpWrite(error);
                              });
}

void Client::onUserInput(Event &&event)
{
    // TODO: actually, it needs to be buffered

    logd1("input: ", event.msg);
    input::args_t args = input::parse(event.msg);

    if (!event.msg.empty() && m_State.mode == mode_t::CHAT_U)
    {
        if (args.cmd == input::cmd_t::UNCHAT || args.cmd == input::cmd_t::DIRECT_MSG_USER)
        {
            // allow these commands even in chat mode
        }
        else
        {
            std::string body = cli_utils::build_user_send_msg_body(m_SelfId, 0, m_State.user,
                                    event.msg, m_Password);
            std::string req = cli_utils::build_request(common::cmd2string(common::cmd_t::MESSAGE_SEND),
                                                    "application/json", body);
            sendRequest(input::cmd_t::DIRECT_MSG_USER, req);
            return;
        }
    }

    if (args.cmd == input::cmd_t::NONE)
    {
        m_Ui->showMsg("unrecognized command. type \"HELP\" for help");
    }
    else if (args.cmd == input::cmd_t::UNCHAT)
    {
        m_Ui->showMsg("switch to usual mode");
        m_State.mode = mode_t::USUAL;
        m_State.user = "";
    }
    else if (args.cmd == input::cmd_t::QUIT)
    {
        m_Ui->showMsg("Bye! Client will exit NOW");
        kill(getpid(), SIGINT);
    }
    else if (args.cmd == input::cmd_t::HELP)
    {
        m_Ui->showMsg(input::help);
    }
    else if (args.cmd == input::cmd_t::REGISTER_USER)
    {
        std::string body = cli_utils::build_user_pass_body(args.reg.name, args.reg.pass);
        std::string req = cli_utils::build_request(common::cmd2string(common::cmd_t::USER_CREATE),
                                                    "application/json", body);
        sendRequest(input::cmd_t::REGISTER_USER, req);
    }
    else if (args.cmd == input::cmd_t::LOGIN)
    {
        std::string body = cli_utils::build_user_pass_body(args.login.name, args.login.pass);
        std::string req = cli_utils::build_request(common::cmd2string(common::cmd_t::USER_LOGIN),
                                                    "application/json", body);
        sendRequest(input::cmd_t::LOGIN, req);
        m_Password = args.login.pass;
    }
    else if (args.cmd == input::cmd_t::DIRECT_MSG_USER)
    {
        std::string body = cli_utils::build_user_send_msg_body(m_SelfId, 0, args.direct.name,
                                args.direct.message, m_Password);
        std::string req = cli_utils::build_request(common::cmd2string(common::cmd_t::MESSAGE_SEND),
                                                    "application/json", body);
        sendRequest(input::cmd_t::DIRECT_MSG_USER, req);
    }
    else if (args.cmd == input::cmd_t::HISTORY_USER)
    {
        std::string body = cli_utils::build_user_history_msg_body(m_SelfId, args.history.name, args.history.count,
                                m_Password);
        std::string req = cli_utils::build_request(common::cmd2string(common::cmd_t::USER_HISTORY),
                                                    "application/json", body);
        sendRequest(input::cmd_t::HISTORY_USER, req);
    }
    else if (args.cmd == input::cmd_t::STATUS_USER)
    {
        std::string body = cli_utils::build_user_history_msg_body(m_SelfId, args.status.name, 0, m_Password);
        std::string req = cli_utils::build_request(common::cmd2string(common::cmd_t::USER_STATUS),
                                                    "application/json", body);
        sendRequest(input::cmd_t::STATUS_USER, req);
    }
    else if (args.cmd == input::cmd_t::CHAT_WITH_USER)
    {
        m_State.mode = mode_t::CHAT_U;
        m_State.user = args.chat.name;
        m_Ui->showMsg("chat with: " + m_State.user);
    }
    else if (args.cmd == input::cmd_t::REGISTER_GROUP)
    {
        std::string body = cli_utils::build_groups_msg_body(m_SelfId, args.group.groupname, "", "", m_Password);
        std::string req = cli_utils::build_request(common::cmd2string(common::cmd_t::CHAT_CREATE),
                                                    "application/json", body);
        sendRequest(input::cmd_t::REGISTER_GROUP, req);
    }
    else if (args.cmd == input::cmd_t::ADD_TO_GROUP)
    {
        std::string body = cli_utils::build_groups_msg_body(m_SelfId, args.group.groupname, args.group.username, "",
                                m_Password);
        std::string req = cli_utils::build_request(common::cmd2string(common::cmd_t::CHAT_ADDUSER),
                                                    "application/json", body);
        sendRequest(input::cmd_t::ADD_TO_GROUP, req);
    }
    else if (args.cmd == input::cmd_t::DIRECT_MSG_GROUP)
    {
        std::string body = cli_utils::build_groups_msg_body(m_SelfId, args.direct.name, "", args.direct.message,
                                m_Password);
        std::string req = cli_utils::build_request(common::cmd2string(common::cmd_t::MESSAGE_SEND_CHAT),
                                                    "application/json", body);
        sendRequest(input::cmd_t::DIRECT_MSG_GROUP, req);
    }
    else
    {
        m_Ui->showMsg("unsupported command");
    }
}

boost::shared_ptr<AsyncHttpClient> Client::createConnect(bool use_ssl)
{
    boost::shared_ptr<TcpClient> socket;
    if (use_ssl)
    {
        socket = boost::make_shared<TcpClient>(m_IoThread->ioService(), true, /*timeout*/ 10);
    }
    else
    {
        socket = boost::make_shared<TcpClient>(m_IoThread->ioService(), false, /*timeout*/ 10);
    }
    return boost::make_shared<AsyncHttpClient>(socket);
}

void Client::handleStop()
{
    logi("stopped by signal");
    g_NeedStop = true;
    m_IoThread->stop();
}

void Client::run()
{
    m_EvWorker.run();
    m_Ui->run();

    logi("chat client started");

    m_Http->asyncConnect(m_Host, m_Port,
            boost::bind(&Client::onHttpConnect, this, boost::asio::placeholders::error));

    // TODO: timeouts :(
    m_Timeout.expires_from_now(std::chrono::milliseconds(5000));
    m_Timeout.async_wait([this](auto ec)
    {
        if (!ec)
        {
            m_TimedOut = true;
            this->m_Http->cancel();
        }
    });

    m_IoThread->start();
    m_Ui->join();
    m_EvWorker.join();
    m_IoThread->join();
}

void Client::onHttpConnect(const ConnectionError &e)
{
    if (e.code)
    {
        f::loge("http connect error [host: {0}:{1}, e: {2}] ", m_Host, m_Port, e.code.message());

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        m_Http->asyncConnect(m_Host, m_Port,
                boost::bind(&Client::onHttpConnect, this, boost::asio::placeholders::error));
        return;
    }

    m_Ui->showMsg("connected to server: " + m_Host + ":" + std::to_string(m_Port));

    if (!m_User.empty() && !m_Password.empty())
    {
        std::string body = cli_utils::build_user_pass_body(m_User, m_Password);
        std::string req = cli_utils::build_request(common::cmd2string(common::cmd_t::USER_LOGIN),
                "application/json", body);
        sendRequest(input::cmd_t::LOGIN, req);
    }
}

void Client::onHttpWrite(const ConnectionError &error)
{
    if (error.code)
    {
        m_Timeout.cancel();
        loge("http request error: ", error.asString());
        return;
    }

    m_Http->asyncResponse(
        [this](const ConnectionError &error, const HttpReply &reply)
        {
            onHttpRead(error, reply);
        });
}

void Client::onHttpRead(const ConnectionError &error, const HttpReply &reply)
{
    m_Timeout.cancel();

    if (error.code)
    {
        f::loge("http read [e: {0}]", error.asString());
        m_Http = createConnect(m_Ssl);
        m_Http->asyncConnect(m_Host, m_Port,
                boost::bind(&Client::onHttpConnect, this, boost::asio::placeholders::error));
        return;
    }

    if (reply._status != 200)
    {
        m_Ui->showMsg(std::to_string(reply._status));
        m_Ui->showMsg(reply._body);
        return;
    }

    logd2("reply: ", reply._body);
    m_Ui->showMsg("200 OK");

    if (m_LastCmd == input::cmd_t::LOGIN)
    {
        cli_utils::response_t resp;
        std::string err = cli_utils::parse_response_aswer(m_LastCmd, reply._body, resp);
        if (!err.empty())
        {
            m_Ui->showMsg(err);
            return;
        }
        m_SelfId     = resp.uid;
        m_SelfChatId = resp.chatid;
        m_HeartBit   = resp.heartbit;

        if (!m_HttpIdle->isOpen())
        {
            m_HttpIdle->asyncConnect(m_Host, m_Port,
                boost::bind(&Client::onHttpIdleConnect, this, boost::asio::placeholders::error));
        }
    }
    else if (m_LastCmd == input::cmd_t::DIRECT_MSG_USER)
    {
    }
    else if (m_LastCmd == input::cmd_t::HISTORY_USER)
    {
        std::vector<cli_utils::msg_response_t> response;
        std::string err = cli_utils::parse_msg_response(input::cmd_t::NONE, reply._body, response);
        if (!err.empty())
        {
            m_Ui->showMsg("error: " + err);
        }
        else
        {
            for (const auto &r : response)
            {
                ui::Message msg;
                msg.from = r.from;
                msg.to = r.to;
                msg.message = r.msg;
                m_Ui->showMsg(msg);
            }
        }
    }
    else if (m_LastCmd == input::cmd_t::STATUS_USER)
    {
        cli_utils::response_t resp;
        std::string err = cli_utils::parse_response_aswer(m_LastCmd, reply._body, resp);
        if (!err.empty())
        {
            m_Ui->showMsg(err);
            return;
        }

        uint64_t last_online = 0;
        if (resp.heartbit < resp.server_ts)
        {
            last_online = resp.server_ts - resp.heartbit;
        }

        if (last_online < 5)
        {
            m_Ui->showMsg("user online");
        }
        else
        {
            m_Ui->showMsg("user was online at about " + std::to_string(last_online) + " sceonds ago");
        }
    }
}

void Client::onHttpIdleConnect(const ConnectionError &e)
{
    if (e.code)
    {
        f::loge("http idle connect error [host: {0}:{1}, e: {2}] ", m_Host, m_Port, e.code.message());

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m_HttpIdle->asyncConnect(m_Host, m_Port,
                boost::bind(&Client::onHttpIdleConnect, this, boost::asio::placeholders::error));
        return;
    }

    std::string body = cli_utils::build_user_send_msg_body(m_SelfId, m_HeartBit, "", "", m_Password);
    std::string req = cli_utils::build_request(common::cmd2string(common::cmd_t::IDLE),
                                                    "application/json", body);

    m_HttpIdle->asyncRequest(req, [this](const ConnectionError &error)
                                  {
                                    onHttpIdleWrite(error);
                                  });
}

void Client::onHttpIdleWrite(const ConnectionError &error)
{
    if (error.code)
    {
        loge("http idle request error: ", error.asString());
        return;
    }

    m_HttpIdle->asyncResponse(
        [this](const ConnectionError &error, const HttpReply &reply)
        {
            onHttpIdleRead(error, reply);
        });
}

void Client::onHttpIdleRead(const ConnectionError &error, const HttpReply &reply)
{
    bool is_error = false;
    if (error.code)
    {
        f::loge("http idle read [e: {0}]", error.asString());
        is_error = true;
    }

    if (!is_error && reply._status != 200)
    {
        f::loge("http idle [status: {0}]", reply._status);
        is_error = true;
    }

    if (is_error)
    {
        m_Ui->showMsg("connection is broken, please relogin");
        m_IdleState = false;
        m_HttpIdle = createConnect(m_Ssl);
        m_Http = createConnect(m_Ssl);
        m_Http->asyncConnect(m_Host, m_Port,
                boost::bind(&Client::onHttpConnect, this, boost::asio::placeholders::error));
        return;
    }

    if (!m_IdleState)
    {
        // after connect, server should answer with 200 OK status,
        // so this is not message, just ack
        m_IdleState = true;
        logd2("+IDLE: ", reply._body);
    }
    else
    {
        logd2("+msg: ", reply._body);
        std::vector<cli_utils::msg_response_t> response;
        std::string err = cli_utils::parse_msg_response(input::cmd_t::NONE, reply._body, response);
        if (!err.empty())
        {
            m_Ui->showMsg("error: " + err);
        }
        else
        {
            for (const auto &r : response)
            {
                ui::Message msg;
                msg.from = r.from;
                msg.to = r.to;
                msg.message = r.msg;
                m_Ui->showMsg(msg);
            }
        }
    }

    // start idle
    m_HttpIdle->asyncResponse(
        [this](const ConnectionError &error, const HttpReply &reply)
        {
            onHttpIdleRead(error, reply);
        });
}

