#pragma once

#include <string>

namespace common
{

enum class cmd_t : uint16_t
{
    MESSAGE_SEND = 1,
    MESSAGE_SEND_CHAT,
    USER_CREATE,
    USER_LOGIN,
    USER_HISTORY,
    USER_STATUS,

    CHAT_CREATE,
    CHAT_HISTORY,
    CHAT_STATUS,
    CHAT_ADDUSER,

    MESSAGE_RECENT,
    IDLE,

    CMD_LAST
};

inline std::string cmd2string(common::cmd_t cmd)
{
    switch (cmd)
    {
        case cmd_t::MESSAGE_SEND:        return "/v1/message/send";
        case cmd_t::MESSAGE_SEND_CHAT:   return "/v1/message/sendchat";
        case cmd_t::USER_CREATE:         return "/v1/user/create";
        case cmd_t::USER_LOGIN:          return "/v1/user/login";
        case cmd_t::CHAT_CREATE:         return "/v1/chat/create";
        case cmd_t::USER_HISTORY:        return "/v1/user/history";
        case cmd_t::CHAT_HISTORY:        return "/v1/group/history";
        case cmd_t::USER_STATUS:         return "/v1/user/status";
        case cmd_t::CHAT_STATUS:         return "/v1/chat/status";
        case cmd_t::CHAT_ADDUSER:        return "/v1/chat/adduser";
        case cmd_t::MESSAGE_RECENT:      return "/v1/message/recent";
        case cmd_t::IDLE:                return "/v1/idle";
        case cmd_t::CMD_LAST:            return "undefined_cmd";
    };
    return "undefined_cmd";
}

enum class ApiStatusCode: uint16_t
{
    ERR_NONE = 0,
    ERR_BAD_REQUEST,
    ERR_INTERNAL,
    ERR_NOT_FOUND,
    ERR_CONSTRAINT
};




}   // namespace common

