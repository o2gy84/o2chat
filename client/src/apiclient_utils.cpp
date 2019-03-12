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
using namespace o2logger;


namespace input
{

std::string cmd2string(cmd_t cmd)
{
    switch (cmd)
    {
        case cmd_t::HELP:              return "HELP";
        case cmd_t::UNCHAT:            return "UNCHAT";
        case cmd_t::LOGIN:             return "LOGIN";
        case cmd_t::REGISTER_USER:     return "REGISTER";
        case cmd_t::REGISTER_GROUP:    return "REGISTER_G";
        case cmd_t::CHAT_WITH_USER:    return "CHAT_WITH";
        case cmd_t::ADD_TO_GROUP:      return "ADD_TO_G";
        case cmd_t::HISTORY_USER:      return "HISTORY";
        case cmd_t::HISTORY_GROUP:     return "HISTORY_G";
        case cmd_t::DIRECT_MSG_USER:   return "DIRECT_MSG";
        case cmd_t::DIRECT_MSG_GROUP:  return "DIRECT_MSG_G";
        case cmd_t::STATUS_USER:       return "STATUS";
        case cmd_t::STATUS_GROUP:      return "STATUS_G";
        case cmd_t::QUIT:              return "QUIT";
        case cmd_t::NONE:              return "none";
    }
    return "";
}

std::string get_msg_after_command(const std::string &line)
// need skip cmd and to
{
    std::regex regex(R"(^\s*\w+\s*\w+\s*(.*)$)");
    std::smatch match;

    if (std::regex_match(line, match, regex))
    {
        std::ssub_match sub_match = match[1];
        std::string str = sub_match.str();
        return str;
    }
    return "";
}

args_t parse(const std::string &string)
{
    args_t ret;
    std::vector<std::string> parts = utils::split(string, " ");
    if (parts.empty())
    {
        return ret;
    }

    std::string cmd = utils::lowercased(parts[0]);
    if (cmd == "help")
    {
        ret.cmd = cmd_t::HELP;
        return ret;
    }
    if (cmd == "quit")
    {
        ret.cmd = cmd_t::QUIT;
        return ret;
    }
    if (cmd == "unchat")
    {
        ret.cmd = cmd_t::UNCHAT;
        return ret;
    }

    if (parts.size() < 2)
    {
        return ret;
    }
    if (cmd == "chat_with")
    {
        ret.cmd = cmd_t::CHAT_WITH_USER;
        ret.chat.name = parts[1];
        return ret;
    }
    if (cmd == "register_g")
    {
        ret.cmd = cmd_t::REGISTER_GROUP;
        ret.group.groupname = parts[1];
        return ret;
    }
    if (cmd == "history")
    {
        ret.cmd = cmd_t::HISTORY_USER;
        ret.history.name = parts[1];
        if (parts.size() > 2)
        {
            try
            {
                ret.history.count = std::stoi(parts[2]);
            }
            catch (...)
            {
            }
        }

        return ret;
    }

    if (cmd == "history_g")
    {
        ret.cmd = cmd_t::HISTORY_GROUP;
        ret.history.name = parts[1];
        if (parts.size() > 2)
        {
            try
            {
                ret.history.count = std::stoi(parts[2]);
            }
            catch (...)
            {
            }
        }

        return ret;
    }
    if (cmd == "status")
    {
        ret.cmd = cmd_t::STATUS_USER;
        ret.status.name = parts[1];
        return ret;
    }
    if (cmd == "status_g")
    {
        ret.cmd = cmd_t::STATUS_GROUP;
        ret.status.name = parts[1];
        return ret;
    }

    if (parts.size() < 3)
    {
        return ret;
    }
    if (cmd == "login")
    {
        ret.cmd = cmd_t::LOGIN;
        ret.login.name = parts[1];
        ret.login.pass = parts[2];
        return ret;
    }
    if (cmd == "register")
    {
        ret.cmd = cmd_t::REGISTER_USER;
        ret.reg.name = parts[1];
        ret.reg.pass = parts[2];
        return ret;
    }

    if (cmd == "direct_msg")
    {
        ret.cmd = cmd_t::DIRECT_MSG_USER;
        ret.direct.name = parts[1];
        ret.direct.message = get_msg_after_command(string);
        return ret;
    }
    if (cmd == "add_to_g")
    {
        ret.cmd = cmd_t::ADD_TO_GROUP;
        ret.group.username = parts[1];
        ret.group.groupname = parts[2];
        return ret;
    }
    
    if (cmd == "direct_msg_g")
    {
        ret.cmd = cmd_t::DIRECT_MSG_GROUP;
        ret.direct.name = parts[1];
        ret.direct.message = get_msg_after_command(string);;
        return ret;
    }

    return ret;
}

}   // namespace input


namespace cli_utils
{

std::string parse_msg_response(input::cmd_t cmd, const std::string &json, std::vector<msg_response_t> &response)
{
    unused_args(cmd);

    rapidjson::Document document;
    if (document.Parse(json.data()).HasParseError())
    {
        std::string err = "bad request, invalid json";
        return err;
    }

    auto it = document.FindMember("msgs");
    if (it == document.MemberEnd())
    {
        std::string err = "bad message";
        return err;
    }

    if (!it->value.IsArray())
    {
        std::string err = "bad format";
        return err;
    }

    for (rapidjson::SizeType i = 0; i < it->value.Size(); i++)
    {
        try
        {
            const rapidjson::Value &v = it->value[i];

            if (!v.IsObject())
            {
                std::string err = "bad format";
                return err;
            }

            msg_response_t r;
            auto it2 = v.FindMember("ts");
            if (it2 != v.MemberEnd())
            {
                r.ts = it2->value.GetUint64();
            }

            it2 = v.FindMember("from");
            if (it2 != v.MemberEnd())
            {
                r.from = it2->value.GetString();
            }

            it2 = v.FindMember("to");
            if (it2 != v.MemberEnd())
            {
                r.to = it2->value.GetString();
            }

            it2 = v.FindMember("message");
            if (it2 != v.MemberEnd())
            {
                r.msg = it2->value.GetString();
            }

            response.emplace_back(r);
        }
        catch (const std::exception &e)
        {
            return e.what();
        }
    }

    return "";
}

std::string parse_response_aswer(input::cmd_t cmd, const std::string &json, response_t &response)
{
    unused_args(cmd);

    rapidjson::Document document;
    if (document.Parse(json.data()).HasParseError())
    {
        std::string response = "bad request, invalid json";
        return response;
    }

    auto it = document.FindMember("id");
    if (it != document.MemberEnd())
    {
        response.uid = it->value.GetUint64();
    }

    it = document.FindMember("chatid");
    if (it != document.MemberEnd())
    {
        response.chatid = it->value.GetUint64();
    }

    it = document.FindMember("heartbit");
    if (it != document.MemberEnd())
    {
        response.heartbit = it->value.GetUint64();
    }

    it = document.FindMember("server_ts");
    if (it != document.MemberEnd())
    {
        response.server_ts = it->value.GetUint64();
    }

    return "";
}

std::string build_request(const std::string &resource, const std::string &content_type, const std::string &body)
{
    std::string request = "POST " + resource + " HTTP/1.1\r\n";
    request += (std::string("Content-Length: ") + std::to_string(body.size()) + std::string("\r\n"));
    request += "Content-Type: " + content_type + "\r\n";
    request += "\r\n";
    request += body;
    return request;
}

std::string build_user_history_msg_body(uint64_t from, const std::string &name, uint64_t count, const std::string &pass)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();

    writer.Key("uid");
    writer.Uint64(from);

    writer.Key("user");
    writer.String(name.c_str());

    writer.Key("count");
    writer.Uint64(count);

    writer.Key("password");
    writer.String(pass.c_str());

    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());

}

std::string build_user_pass_body(const std::string &user, const std::string &pass)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();

    writer.Key("user");
    writer.String(user.c_str());

    writer.Key("password");
    writer.String(pass.c_str());

    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());
}

std::string build_user_send_msg_body(uint64_t from,
                                     uint64_t ts,
                                     const std::string &to,
                                     const std::string &msg,
                                     const std::string &pass)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();

    writer.Key("uid");
    writer.Uint64(from);

    if (ts != 0)
    {
        writer.Key("heartbit");
        writer.Uint64(ts);
    }

    writer.Key("to");
    writer.String(to.c_str());

    writer.Key("message");
    writer.String(msg.c_str());

    writer.Key("password");
    writer.String(pass.c_str());

    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());
}

std::string build_groups_msg_body(uint64_t from,
                                  const std::string &groupname,
                                  const std::string &username,
                                  const std::string &message,
                                  const std::string &pass)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();

    writer.Key("uid");
    writer.Uint64(from);

    writer.Key("chatname");
    writer.String(groupname.c_str());

    if (!username.empty())
    {
        writer.Key("adduser");
        writer.String(username.c_str());
    }

    if (!message.empty())
    {
        writer.Key("message");
        writer.String(message.c_str());
    }

    writer.Key("password");
    writer.String(pass.c_str());

    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());
}


}  // namespace cli_utils


