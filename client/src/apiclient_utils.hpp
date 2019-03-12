#pragma once

#include <vector>

#include <boost/optional.hpp>

#include "common/common.hpp"


namespace input
{

static std::string help = 
"Commands are:\n"
"   HELP\n"
"   LOGIN <user> <pass>\n"
"   REGISTER <user> <pass>\n"
"   REGISTER_G <group>\n"
"   CHAT_WITH <user>\n"
"   ADD_TO_G <user> <group>\n"
"   UNCHAT\n"
"   HISTORY <user> [N]\n"
"   HISTORY_G <group> [N]\n"
"   DIRECT_MSG <user> <msg>\n"
"   DIRECT_MSG_G <group> <msg>\n"
"   STATUS <user>\n"
"   STATUS_G <group>\n"
"   QUIT\n";

enum class cmd_t : uint8_t
{
    NONE = 1,
    HELP,
    UNCHAT,
    LOGIN,
    REGISTER_USER,
    REGISTER_GROUP,
    CHAT_WITH_USER,
    ADD_TO_GROUP,
    HISTORY_USER,
    HISTORY_GROUP,
    DIRECT_MSG_USER,
    DIRECT_MSG_GROUP,
    STATUS_USER,
    STATUS_GROUP,
    QUIT
};

std::string cmd2string(cmd_t cmd);

struct args_t
{
    cmd_t cmd = cmd_t::NONE;

    struct login
    {
        std::string name;
        std::string pass;
    } login;

    struct reg
    {
        std::string name;
        std::string pass;
    } reg;

    struct chat
    {
        std::string name;
    } chat;

    struct group
    {
        std::string groupname;
        std::string username;
    } group;

    struct history
    {
        std::string name;
        size_t count = 10;
    } history;

    struct direct
    {
        std::string name;
        std::string message;
    } direct;

    struct status
    {
        std::string name;
    } status;
};

args_t parse(const std::string &string);

}   // namespace input


namespace cli_utils
{

struct response_t
{
    uint64_t uid       = 0;
    uint64_t chatid    = 0;
    uint64_t heartbit  = 0;
    uint64_t server_ts = 0;
};

struct msg_response_t
{
    uint64_t ts     = 0;
    std::string from;
    std::string to;
    std::string msg;
};


std::string parse_response_aswer(input::cmd_t cmd, const std::string &json, response_t &response);
std::string parse_msg_response(input::cmd_t cmd, const std::string &json, std::vector<msg_response_t> &response);

std::string build_request(const std::string &resource,
                          const std::string &content_type,
                          const std::string &body);

std::string build_user_pass_body(const std::string &user,
                                 const std::string &pass);

std::string build_user_send_msg_body(uint64_t from,
                                     uint64_t ts,
                                     const std::string &to,
                                     const std::string &msg,
                                     const std::string &pass);

std::string build_user_history_msg_body(uint64_t from, const std::string &name, uint64_t count, const std::string &pass);


std::string build_groups_msg_body(uint64_t from,
                                  const std::string &groupname,
                                  const std::string &username,
                                  const std::string &message,
                                  const std::string &pass);


}  // namespace cli_utils
