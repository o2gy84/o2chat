#include "apiclient_utils.hpp"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <set>
#include <chrono>
#include <queue>
#include <regex>

#include "common/utils.hpp"

#include "o2logger/src/o2logger.hpp"
using namespace o2logger; // NOLINT // NOLINT


namespace apiclient_utils
{

bool password_check(const std::string &password, size_t min_len)
{
    if (password.size() < min_len)
    {
        return false;
    }

    if (std::regex_match(password, std::regex("^[a-zA-Z0-9_]*$")))
    {
        return true;
    }

    return false;
}

std::string make_api_error(common::ApiStatusCode api_code, const std::string &desc)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();

    writer.Key("status");
    writer.Uint64(static_cast<int>(api_code));

    writer.Key("error");
    writer.String(desc.c_str());

    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());
}

std::string build_api_ok_response_body(common::cmd_t command)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();

    writer.Key("cmd");
    writer.Uint64(static_cast<int>(command));

    writer.Key("server_ts");
    writer.Uint64(time(NULL));

    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());
}

std::string build_api_ok_response_body(const db::User &user)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();

    writer.Key("id");
    writer.Uint64(user.id);

    writer.Key("chatid");
    writer.Uint64(user.self_chat_id);

    writer.Key("heartbit");
    writer.Uint64(user.heartbit);

    writer.Key("name");
    writer.String(user.name.c_str());

    writer.Key("server_ts");
    writer.Uint64(time(NULL));

    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());
}

std::string build_api_ok_response_body(const db::Chat &chat)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();

    writer.Key("chatid");
    writer.Uint64(chat.id);

    writer.Key("name");
    writer.String(chat.name.c_str());

    writer.Key("server_ts");
    writer.Uint64(time(NULL));

    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());
}

std::string build_api_ok_response_body(std::vector<apiclient_utils::Message> &&msgs)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();

    writer.Key("server_ts");
    writer.Uint64(time(NULL));

    writer.Key("msgs");
    writer.StartArray();
    for (const auto &msg : msgs)
    {
        writer.StartObject();

        writer.Key("from");
        writer.String(msg.from.c_str());

        writer.Key("to");
        writer.String(msg.to.c_str());

        writer.Key("message");
        writer.String(msg.msg.c_str());

        writer.Key("ts");
        writer.Uint64(msg.ts);

        writer.EndObject();
    }
    writer.EndArray();

    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());
}


void log_task_done(const std::string &error,
                   const std::string &sessid,
                   const std::string &remote_address,
                   const std::string &method,
                   const std::string &resource,
                   int http_code,
                   int total_req_processing_ms)
{
    std::chrono::system_clock::time_point p = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(p);

    std::stringstream ss;
    ss << "[" << sessid << "] ";
    ss << "[" << remote_address << "] [" << t << "] ["
              << method << "] " << resource << " ["
              << http_code << "] [" << total_req_processing_ms << "ms]";

    if (!error.empty())
    {
        std::string err_msg = std::string(" e: error write response to client - ") + error;
        loge(ss.str(), err_msg);
        return;
    }
    logi(ss.str());
}

}   // namespace apiclient_utils
