#pragma once

#include <vector>
#include <boost/optional.hpp>

#include "common/common.hpp"
#include "database.hpp"


namespace apiclient_utils
{

struct Message
{
    Message(uint64_t ts, const std::string &from, const std::string &to, const std::string &msg) :
        ts(ts),
        from(from),
        to(to),
        msg(msg)
    {}
    uint64_t ts;
    std::string from;
    std::string to;
    std::string msg;
};


bool password_check(const std::string &password, size_t min_len);

std::string make_api_error(common::ApiStatusCode api_code, const std::string &desc);

std::string build_api_ok_response_body(common::cmd_t command);
std::string build_api_ok_response_body(const db::User &user);
std::string build_api_ok_response_body(const db::Chat &chat);
std::string build_api_ok_response_body(std::vector<apiclient_utils::Message> &&msgs);



void log_task_done(const std::string &error,
                   const std::string &sessid,
                   const std::string &remote_address,
                   const std::string &method,
                   const std::string &resource,
                   int http_code,
                   int total_req_processing_ms);

}   // namespace apiclient_utils
