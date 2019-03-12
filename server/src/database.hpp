#pragma once

#include <string>
#include <mutex>
#include <vector>
#include <boost/shared_ptr.hpp>

#include "request.hpp"
#include "common/common.hpp"


class ApiClient;

namespace db
{

enum class type_t : uint8_t
{
    MEMORY,
    MYSQL
};

struct Task
{
    explicit Task() {}
    Task(const RequestDetails &req) :
        cmd(req.command),
        request(req.params)
    {}

    bool ping = false;
    common::cmd_t cmd;
    RequestDetails::Params request;
    boost::shared_ptr<ApiClient> client;
    std::string storage;
};

// NB: see mysql/init.sql
// TODO: need link chat_id and user_id, who created this chat.
//       need possibility to create many chats with same name
struct Chat
{
    Chat() {}
    Chat(uint64_t id, const std::string &name) : id(id), name(name) {}
    uint64_t id = 0;
    std::string name;
};

struct User
{
    User() {}
    User(uint64_t id, uint64_t chatid, const std::string &name, const std::string &pass, const std::string &path) :
        id(id),
        self_chat_id(chatid),
        name(name),
        password(pass),
        stpath(path)
    {}
	uint64_t id             = 0;
	uint64_t self_chat_id   = 0;
    uint64_t heartbit       = 0;
    std::string name;
    std::string password;
    std::string stpath;         // storage path
};

// many to many link -> chats with users
struct Chatuser
{
    Chatuser() {}
    Chatuser(uint64_t chatid, uint64_t uid) : chatid(chatid), uid(uid) {}
    uint64_t chatid = 0;    // <-- that is foreign key
    uint64_t uid = 0;       // <-- that is foreign key
};

struct get_msg_opt_t
{
    //bool only_unread = true;
    uint64_t ts = 0;           // from this time
    uint32_t max_count = 3;
};

struct Message
{
    enum class flags_t : uint8_t
    {
        UNREAD   = 0,
        READ     = 1,
        ANSWERED = 1 << 2
    };

    Message(uint64_t user_from, uint64_t chat_to, const std::string &msg) :
        user_from(user_from),
        chat_to(chat_to),
        message(msg)
    {}
    flags_t flags       = flags_t::UNREAD;
    uint64_t user_from  = 0;                    // FROM:
    uint64_t chat_to    = 0;                    // TO:
    uint64_t ts         = 0;
    std::string message;
};

}   // namespace db

std::ostream& operator<<(std::ostream &os, const db::Message &msg);

class AbstractConnection
{
public:
    AbstractConnection() {}
    virtual ~AbstractConnection() {}

    virtual void updateUserHeartBit(const db::User &user, uint64_t ts) = 0;
    virtual db::User createUser(const std::string &name, const std::string &pass, const std::string &stpath) = 0;
    virtual db::Chat createChat(const std::string &name, uint64_t uid) = 0;

    virtual std::vector<db::User> lookupUserByName(const std::string &name) const = 0;
    virtual db::User lookupUserById(uint64_t id) const = 0;
    virtual std::vector<db::Chat> lookupChatsForUserId(uint64_t uid) const = 0;

    virtual std::vector<db::Chat> lookupChatByName(const std::string &name) const = 0;
    virtual db::Chat lookupChatById(uint64_t chatid) const = 0;
    virtual std::vector<db::User> lookupUsersForChatId(uint64_t chatid) const = 0;
    virtual void addUserToChat(const db::Chat &chat, const db::User &user) = 0;

    virtual void saveMessage(const db::Message &msg) = 0;
    virtual std::vector<db::Message> getMessages(uint64_t chatid, const db::get_msg_opt_t &opt) const = 0;
    virtual std::vector<db::Message> selectMessages(std::function<bool(const db::Message &)> &&pred, const db::get_msg_opt_t &opt) const = 0;

protected:
};

// NB: FOR TEST ONLY
class InMemoryConnection : public AbstractConnection
{
public:
    InMemoryConnection() {}

    void updateUserHeartBit(const db::User &user, uint64_t ts) override;
    db::User createUser(const std::string &name, const std::string &pass, const std::string &stpath) override;
    db::Chat createChat(const std::string &name, uint64_t uid) override;

    std::vector<db::User> lookupUserByName(const std::string &name) const override;
    db::User lookupUserById(uint64_t id) const override;
    std::vector<db::Chat> lookupChatsForUserId(uint64_t uid) const override;

    std::vector<db::Chat> lookupChatByName(const std::string &name) const override;
    db::Chat lookupChatById(uint64_t chatid) const override;
    std::vector<db::User> lookupUsersForChatId(uint64_t chatid) const override;
    void addUserToChat(const db::Chat &chat, const db::User &user) override;

    void saveMessage(const db::Message &msg) override;
    std::vector<db::Message> getMessages(uint64_t chatid, const db::get_msg_opt_t &opt) const override;
    std::vector<db::Message> selectMessages(std::function<bool(const db::Message &)> &&pred, const db::get_msg_opt_t &opt) const override;

private:
    struct Storage
    {
        uint64_t user_autoincrement = 1;
        uint64_t chat_autoincrement = 1;

        std::vector<db::Chat> chats;            // TODO: is better to use <set>, because id are uniq
        std::vector<db::User> users;            // TODO: is better to use <set>
        std::vector<db::Chatuser> chatuser;
        std::vector<db::Message> messages;
    };
    static Storage m_Storage;
    static std::mutex m_Mutex;
};

class MysqlConnection : public AbstractConnection
{
public:
    MysqlConnection() {}

    void updateUserHeartBit(const db::User &, uint64_t ) override {}
    db::User createUser(const std::string &, const std::string &, const std::string &) override { return {}; }
    db::Chat createChat(const std::string &, uint64_t ) override { return {}; }

    std::vector<db::User> lookupUserByName(const std::string &) const override { return {}; }
    db::User lookupUserById(uint64_t ) const override { return {}; }
    std::vector<db::Chat> lookupChatsForUserId(uint64_t ) const override { return {}; }

    std::vector<db::Chat> lookupChatByName(const std::string &) const override { return {}; }
    db::Chat lookupChatById(uint64_t ) const override { return {}; }
    std::vector<db::User> lookupUsersForChatId(uint64_t ) const override { return {}; }
    void addUserToChat(const db::Chat &, const db::User &) override {};

    void saveMessage(const db::Message &) override {}
    std::vector<db::Message> getMessages(uint64_t, const db::get_msg_opt_t &) const override { return {}; }
    std::vector<db::Message> selectMessages(std::function<bool(const db::Message &)> &&, const db::get_msg_opt_t &) const override { return {}; }

private:
};

 
class AbstractDatabase
{
public:
    virtual ~AbstractDatabase() {}
    virtual std::unique_ptr<AbstractConnection> getConnection() = 0;
protected:

};

class InMemoryDataBase : public AbstractDatabase
{
public:
    std::unique_ptr<AbstractConnection> getConnection() override
    {
        return std::unique_ptr<AbstractConnection>(new InMemoryConnection());
    }

private:
};

class MysqlDataBase : public AbstractDatabase
{
public:
    std::unique_ptr<AbstractConnection> getConnection() override
    {
        return std::unique_ptr<AbstractConnection>(new MysqlConnection());
    }

private:
};
