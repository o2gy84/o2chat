#include <chrono>
#include <thread>
#include <memory>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <atomic>
#include <map>
#include <tuple>

#include <sys/types.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "libproperty/src/libproperty.hpp"

#include "apiclient.hpp"
#include "database.hpp"

#include "o2logger/src/o2logger.hpp"

using namespace o2logger;


namespace
{

std::string generate_session_id(const std::string &local_addr)
{
    std::stringstream ss;
    ss << std::this_thread::get_id() << local_addr;
    ss << std::chrono::steady_clock::now().time_since_epoch().count();
    ss << utils::gen_random(10);

    std::stringstream hex;
    hex << std::hex << utils::crc32(ss.str());

    std::string hex_str = hex.str();

    for (size_t i = 0; i < hex_str.size(); ++i)
    {
        if (rand()%2 && isalpha(hex_str[i]))    // NOLINT
        {
            hex_str[i] = toupper(hex_str[i]);
        }
    }

    return hex_str + utils::gen_random(2);
}

uint64_t max_timestamp(std::vector<apiclient_utils::Message> &msgs)
{
    uint64_t ts = 0;
    for (const auto &msg : msgs)
    {
        if (msg.ts > ts)
        {
            ts = msg.ts;
        }
    }
    return ts;
}

rapidjson::GenericMemberIterator<true, rapidjson::UTF8<>, rapidjson::MemoryPoolAllocator<>>
find_required_string_param(const rapidjson::Document &doc, const std::string &pattern, std::string &error)
{
    auto it = doc.FindMember(pattern.c_str());
    if (it == doc.MemberEnd())
    {
        error = std::string("bad request, ") + pattern + " required";
        return it;
    }

    if (!it->value.IsString())
    {
        error = std::string("bad request, ") + pattern + " is not string";
        return it;
    }

    if (std::string(it->value.GetString()).empty())
    {
        error = "bad request, " + pattern + " is empty";
        return it;
    }
    return it;
}

rapidjson::GenericMemberIterator<true, rapidjson::UTF8<>, rapidjson::MemoryPoolAllocator<>>
find_required_uint_param(const rapidjson::Document &doc, const std::string &pattern, std::string &error)
{
    auto it = doc.FindMember(pattern.c_str());
    if (it == doc.MemberEnd())
    {
        error = std::string("bad request, ") + pattern + " required";
        return it;
    }

    if (!it->value.IsUint())
    {
        error = std::string("bad request, ") + pattern + " is not uint";
        return it;
    }

    return it;
}


std::string parse_meta(RequestDetails &details,
                       const std::string &json,
                       common::cmd_t command)
{
    logd2("parse json: ", json);

    rapidjson::Document document;
    if (document.Parse(json.data()).HasParseError())
    {
        loge("invalid meta json");
        std::string api_response = "bad request, invalid json";
        return api_response;
    }

    {
        std::string fatal_error;
        auto it = find_required_string_param(document, "password", fatal_error);
        if (!fatal_error.empty())
        {
            f::loge("[{0}] parse meta: {1}", details.sessid, fatal_error);
            return fatal_error;
        }
        details.params.password = it->value.GetString();
    }


    if (   command == common::cmd_t::USER_CREATE
        || command == common::cmd_t::USER_LOGIN
        )
    {
        std::string fatal_error;
        auto it = find_required_string_param(document, "user", fatal_error);
        if (!fatal_error.empty())
        {
            f::loge("[{0}] parse meta: {1}", details.sessid, fatal_error);
            return fatal_error;
        }
        details.params.user = it->value.GetString();
 
        if (command == common::cmd_t::USER_CREATE)
        {
            int min_len = libproperty::Options::impl()->get<int>("pass_len");
            if (min_len < 0)
            {
                min_len = 8;
            }
            if (!apiclient_utils::password_check(details.params.password, min_len))
            {
                std::string err = "weak password";
                return err;
            }
        }

        return "";
    }

    {
        // self id required 
        std::string fatal_error;
        auto it = find_required_uint_param(document, "uid", fatal_error);
        if (!fatal_error.empty())
        {
            f::loge("[{0}] parse meta: {1}", details.sessid, fatal_error);
            return fatal_error;
        }
        details.params.uid = it->value.GetUint();
    }

    if (   command == common::cmd_t::CHAT_CREATE
        || command == common::cmd_t::CHAT_ADDUSER
        )
    {
        std::string fatal_error;
        auto it = find_required_string_param(document, "chatname", fatal_error);
        if (!fatal_error.empty())
        {
            f::loge("[{0}] parse meta: {1}", details.sessid, fatal_error);
            return fatal_error;
        }
        details.params.chat.name = it->value.GetString();

        if (command == common::cmd_t::CHAT_ADDUSER)
        {
            it = find_required_string_param(document, "adduser", fatal_error);
            if (!fatal_error.empty())
            {
                f::loge("[{0}] parse meta: {1}", details.sessid, fatal_error);
                return fatal_error;
            }
            details.params.chat.adduser = it->value.GetString();
        }

        return "";
    }

    if (command == common::cmd_t::IDLE)
    {
        std::string fatal_error;
        auto it = find_required_uint_param(document, "heartbit", fatal_error);
        if (fatal_error.empty())
        {
            details.params.ts = it->value.GetUint();
        }
        return "";
    }
    
    if (   command == common::cmd_t::USER_HISTORY
        || command == common::cmd_t::USER_STATUS
        )
    {
        std::string fatal_error;
        auto it = find_required_string_param(document, "user", fatal_error);
        if (!fatal_error.empty())
        {
            f::loge("[{0}] parse meta: {1}", details.sessid, fatal_error);
            return fatal_error;
        }
        details.params.user = it->value.GetString();
        
        it = find_required_uint_param(document, "count", fatal_error);
        if (fatal_error.empty())
        {
            details.params.count = it->value.GetUint();
        }
        return "";
    }
       
    if (   command == common::cmd_t::MESSAGE_SEND
        || command == common::cmd_t::MESSAGE_SEND_CHAT
        )
    {
        std::string fatal_error;
        auto it = find_required_string_param(document, "message", fatal_error);
        if (!fatal_error.empty())
        {
            f::loge("[{0}] parse meta: {1}", details.sessid, fatal_error);
            return fatal_error;
        }
        details.params.message = it->value.GetString();

        if (command == common::cmd_t::MESSAGE_SEND)
        {
            it = find_required_string_param(document, "to", fatal_error);
            if (!fatal_error.empty())
            {
                f::loge("[{0}] parse meta: {1}", details.sessid, fatal_error);
                return fatal_error;
            }
            details.params.to_user = it->value.GetString();
        }
        else
        {
            it = find_required_string_param(document, "chatname", fatal_error);
            if (!fatal_error.empty())
            {
                f::loge("[{0}] parse meta: {1}", details.sessid, fatal_error);
                return fatal_error;
            }

            details.params.chat.name = it->value.GetString();
        }
        return "";
    }

    return "";
}

std::string http_code_to_string(int code)
{
    switch (code)
    {
        case 200: return "200 OK";
        case 400: return "400 Bad Request";
        case 403: return "403 Forbidden";
        case 404: return "404 Not Found";
        case 409: return "409 Conflict";
        case 429: return "429 Too Many Requests";
        case 500: return "500 Internal Server Error";
        case 503: return "503 Service Unavailable";
    }
    return "200 OK";
}

std::string build_api_error_rsponse(int http_code, common::ApiStatusCode api_code, const std::string &desc)
{
    std::string api_response = apiclient_utils::make_api_error(api_code, desc);

    std::string response = "HTTP/1.1 " + http_code_to_string(http_code) + "\r\n";
    response += (std::string("Content-Length: ") + std::to_string(api_response.size()) + std::string("\r\n"));
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Content-Type: application/json\r\n";
    response += "\r\n";
    response += api_response;

    return response;
}

}   // namespace


ApiClient::ApiClient(boost::shared_ptr<TcpClient> socket, DatabaseWorker &db) :
    m_HttpCode(200),
    m_StoragePollingIntervalMs(200),
    m_Timer(socket->ioService()),
    m_Db(db)
{
    m_Client = boost::make_shared<AsyncHttpClient>(socket);
}

ApiClient::~ApiClient()
{
}

void ApiClient::serveSslClient()
{
    m_Client->asyncHandshakeAsServer([self = shared_from_this()](const ConnectionError &error)
    {
        self->processClientRequest(error);
    });
}

void ApiClient::readCmd()
{
    m_Client->asyncResponse([self = shared_from_this()](const ConnectionError &error, const HttpReply &reply)
    {
        self->requestFromClientReadHandler(error, reply);
    });
}

void ApiClient::processClientRequest(const ConnectionError &error)
{
    m_RequestDetails.sessid = generate_session_id(m_Client->localAddr());
    f::logd2("[{0}] new client {1}", m_RequestDetails.sessid, "");

    if (error.code)
    {
        requestFromClientReadHandler(error, {});
        return;
    }

    readCmd();
}

std::string ApiClient::buildApiOkResponse(const std::string &body)
{
    std::string response = "HTTP/1.1 200 OK\r\n";
    response += (std::string("Content-Length: ") + std::to_string(body.size()) + std::string("\r\n"));
    response += "Content-Type: application/json\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "\r\n";
    response += body;

    logd4("HTTP response:\n", response);
    return response;
}

void ApiClient::sendOkResponse(const std::string &body)
{
    std::string response = buildApiOkResponse(body);

    m_Client->asyncRequest(response, [self = shared_from_this()](const ConnectionError &error)
    {
        self->responseToClientWroteHandler(error);
    });
}

void ApiClient::sendMessages(std::vector<apiclient_utils::Message> &&msgs)
{
    uint64_t max_ts = max_timestamp(msgs);

    std::string body = apiclient_utils::build_api_ok_response_body(std::move(msgs));
    std::string response = buildApiOkResponse(body);

    m_Client->asyncRequest(response, [self = shared_from_this(), max_ts](const ConnectionError &error)
    {
        self->responseToClientWroteHandler(error);
    });
}


void ApiClient::sendMessagesToIdleConn(std::vector<apiclient_utils::Message> &&msgs)
{
    uint64_t max_ts = max_timestamp(msgs);

    std::string body = apiclient_utils::build_api_ok_response_body(std::move(msgs));
    std::string response = buildApiOkResponse(body);

    m_Client->asyncRequest(response, [self = shared_from_this(), max_ts](const ConnectionError &error)
    {
        if (error.code)
        {
            loge("idle connect error: ", error.asString());
            self->m_Timer.cancel();
            self->m_Client->cancel();
            return;
        }
        self->m_NewestMsgTimestamp = max_ts;
    });
}

void ApiClient::sendOkResponseAndStartIdle()
{
    std::string body = apiclient_utils::build_api_ok_response_body(m_RequestDetails.command);
    std::string response = buildApiOkResponse(body);

    m_Client->asyncRequest(response, [self = shared_from_this()](const ConnectionError &error)
    {
        // TODO: process error
        unused_args(error);
    });

    // start polling storage for new messages
    // actually, it should be PUB/SUB sheme
    m_Timer.expires_from_now(std::chrono::milliseconds(m_StoragePollingIntervalMs));
    m_Timer.async_wait(boost::bind(&ApiClient::timerWaitTaskResultHandler, shared_from_this(), boost::asio::placeholders::error));
}

void ApiClient::sendErrorResponse(int http_code, common::ApiStatusCode api_code, const std::string &desc)
{
    m_HttpCode = http_code;

    std::string response = build_api_error_rsponse(m_HttpCode, api_code, desc);
    m_Client->asyncRequest(response,
                           [self = shared_from_this()](const ConnectionError &error)
                           {
                               self->responseToClientWroteHandler(error);
                           });
}

void ApiClient::v1_handler(const HttpReply &req, common::cmd_t cmd)
{
    m_RequestDetails.command = cmd;
    handler_impl(req, m_RequestDetails.command);

    f::logi("[{0}] new request [{1}, {2}]",
        m_RequestDetails.sessid, m_Client->remoteAddr(), m_RequestDetails.resource);
}

void ApiClient::handler_impl(const HttpReply &req, common::cmd_t command)
{
    std::string content_type = req.getHeader("Content-Type");

    if (utils::starts_with(content_type, "text/plain")
        || utils::starts_with(content_type, "application/json"))
    {
        std::string err = parse_meta(m_RequestDetails, req._body, command);
        if (!err.empty())
        {
            sendErrorResponse(400, common::ApiStatusCode::ERR_BAD_REQUEST, err);
            return;
        }
    }
    else
    {
        loge("not allowed content type: ", content_type);
        sendErrorResponse(400, common::ApiStatusCode::ERR_BAD_REQUEST, "not allowed content type");
        return;
    }

    db::Task task(m_RequestDetails);
    task.client = shared_from_this();
    if (command == common::cmd_t::USER_CREATE)
    {
        // need to select one of the storages.
        // currently - storage only one
        task.storage = "localhost:3306";
    }

    m_Db.putTask(std::move(task));
}

void ApiClient::timerWaitTaskResultHandler(const boost::system::error_code& e)
/*
 *  check result
 */
{
    if (e == boost::asio::error::operation_aborted)
    {
        return;
    }

    if (m_NewestMsgTimestamp > m_RequestDetails.params.ts)
    {
        m_RequestDetails.params.ts = m_NewestMsgTimestamp;
    }

    db::Task task(m_RequestDetails);
    task.client = shared_from_this();

    if ((time(NULL) - m_LastClientPing) > 5)
    {
        m_LastClientPing = time(NULL);
        task.ping = true;
    }

    // TODO: need to check pass once after connect! not each time
    m_Db.putTask(std::move(task));

    m_Timer.expires_from_now(std::chrono::milliseconds(m_StoragePollingIntervalMs));
    m_Timer.async_wait(boost::bind(&ApiClient::timerWaitTaskResultHandler, shared_from_this(), boost::asio::placeholders::error));
}

void ApiClient::requestFromClientReadHandler(const ConnectionError &error, const HttpReply &reply)
{
    m_Start = std::chrono::steady_clock::now();

    if (error.code)
    {
        f::loge("request from client [r: {0}, error: {1}]", cmd2string(m_RequestDetails.command), error.asString());
        // close connect
        return;
    }

    m_RequestDetails.remote_address = m_Client->remoteAddr();
    m_RequestDetails.resource = utils::lowercased(reply._resource);
    m_RequestDetails.method = reply._method;

    logd3(reply._headers);
    logd4("body: ", reply._body);

    if (m_RequestDetails.resource == "/v1/idle")
    {
        m_RequestDetails.command = common::cmd_t::IDLE;
        std::string err = parse_meta(m_RequestDetails, reply._body, common::cmd_t::IDLE);
        if (!err.empty())
        {
            sendErrorResponse(400, common::ApiStatusCode::ERR_BAD_REQUEST, err);
            return;
        }

        sendOkResponseAndStartIdle();
        return;
    }

    // TODO: router
    if (m_RequestDetails.resource == "/v1/message/send")
    {
        v1_handler(reply, common::cmd_t::MESSAGE_SEND);
    }
    else if (m_RequestDetails.resource == "/v1/message/sendchat")
    {
        v1_handler(reply, common::cmd_t::MESSAGE_SEND_CHAT);
    }
    else if (m_RequestDetails.resource == "/v1/user/login")
    {
        v1_handler(reply, common::cmd_t::USER_LOGIN);
    }
    else if (m_RequestDetails.resource == "/v1/user/create")
    {
        v1_handler(reply, common::cmd_t::USER_CREATE);
    }
    else if (m_RequestDetails.resource == "/v1/chat/create")
    {
        v1_handler(reply, common::cmd_t::CHAT_CREATE);
    }
    else if (m_RequestDetails.resource == "/v1/user/history")
    {
        v1_handler(reply, common::cmd_t::USER_HISTORY);
    }
    else if (m_RequestDetails.resource == "/v1/group/history")
    {
        v1_handler(reply, common::cmd_t::CHAT_HISTORY);
    }
    else if (m_RequestDetails.resource == "/v1/user/status")
    {
        v1_handler(reply, common::cmd_t::USER_STATUS);
    }
    else if (m_RequestDetails.resource == "/v1/chat/adduser")
    {
        v1_handler(reply, common::cmd_t::CHAT_ADDUSER);
    }
    else
    {
        std::string api_response = "bad request";
        sendErrorResponse(404, common::ApiStatusCode::ERR_NOT_FOUND, api_response);
        return;
    }
}

void ApiClient::responseToClientWroteHandler(const ConnectionError &error)
{
    std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::steady_clock::now();
    int ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_Start).count();


    std::string e = error.code ? error.asString() : "";

    apiclient_utils::log_task_done(e, m_RequestDetails.sessid, m_RequestDetails.remote_address,
                                      m_RequestDetails.method, m_RequestDetails.resource, m_HttpCode, ms);

    m_HttpCode = 200;   // will change if next request will be "bad"

    // don't close connect
    readCmd();
}
