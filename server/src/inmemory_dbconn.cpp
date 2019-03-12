#include "database.hpp"

#include "o2logger/src/o2logger.hpp"

using namespace o2logger;


InMemoryConnection::Storage InMemoryConnection::m_Storage;
std::mutex InMemoryConnection::m_Mutex;

void InMemoryConnection::updateUserHeartBit(const db::User &user, uint64_t ts)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    for (auto &u : m_Storage.users)
    {
        if (u.id == user.id)
        {
            u.heartbit = ts;
            return;
        }
    }
}

db::User InMemoryConnection::createUser(const std::string &name, const std::string &pass, const std::string &stpath)
/*
    transaction start
    insert into chat
    into user ^^ right chat_id
    into chatuser ^^ user_id + chat_id !!!
    commit
*/
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    for (const auto &user : m_Storage.users)
    {
        if (user.name == name)
        {
            return {};
        }
    }

    uint64_t chat_id = m_Storage.chat_autoincrement++;
    uint64_t user_id = m_Storage.user_autoincrement++;

    m_Storage.chats.emplace_back(db::Chat(chat_id, name));
    db::User user(user_id, chat_id, name, pass, stpath);
    
    m_Storage.users.emplace_back(user);

    m_Storage.chatuser.emplace_back(db::Chatuser(chat_id, user_id));
    return user;
}

db::Chat InMemoryConnection::createChat(const std::string &name, uint64_t uid)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    for (const auto &chat : m_Storage.chats)
    {
        if (chat.name == name)
        {
            return {};
        }
    }

    uint64_t chat_id = m_Storage.chat_autoincrement++;

    db::Chat chat(chat_id, name);
    m_Storage.chats.emplace_back(chat);

    if (uid > 0)
    {
        m_Storage.chatuser.emplace_back(db::Chatuser(chat_id, uid));
    }

    return chat;
}

std::vector<db::User> InMemoryConnection::lookupUserByName(const std::string &name) const
{
    std::vector<db::User> ret;
    std::lock_guard<std::mutex> lock(m_Mutex);

    for (const auto &user : m_Storage.users)
    {
        if (user.name == name)
        {
            ret.push_back(user);
        }
    }
    return ret;
}

db::User InMemoryConnection::lookupUserById(uint64_t id) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (const auto &user : m_Storage.users)
    {
        if (user.id == id)
        {
            return user;
        }
    }
    return {};
}

std::vector<db::Chat> InMemoryConnection::lookupChatsForUserId(uint64_t uid) const
{
    std::vector<uint64_t> chats;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (const auto &chatuser : m_Storage.chatuser)
        {
            if (chatuser.uid == uid)
            {
                chats.push_back(chatuser.chatid);
            }
        }
    }

    std::vector<db::Chat> ret;
    for (uint64_t chatid : chats)
    {
        db::Chat chat = lookupChatById(chatid);
        ret.push_back(chat);
    }

    return ret;
}

std::vector<db::Chat> InMemoryConnection::lookupChatByName(const std::string &name) const
{
    std::vector<db::Chat> ret;
    std::lock_guard<std::mutex> lock(m_Mutex);

    for (const auto &chat : m_Storage.chats)
    {
        if (chat.name == name)
        {
            ret.push_back(chat);
        }
    }
    return ret;
}

db::Chat InMemoryConnection::lookupChatById(uint64_t chatid) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (const auto &chat : m_Storage.chats)
    {
        if (chat.id == chatid)
        {
            return chat;
        }
    }
    return {};
}

std::vector<db::User> InMemoryConnection::lookupUsersForChatId(uint64_t chatid) const
{
    std::vector<uint64_t> uids;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (const auto &chatuser : m_Storage.chatuser)
        {
            if (chatuser.chatid == chatid)
            {
                uids.push_back(chatuser.uid);
            }
        }
    }

    std::vector<db::User> ret;
    for (uint64_t uid: uids)
    {
        db::User user = lookupUserById(uid);
        ret.push_back(user);
    }

    return ret;
}

void InMemoryConnection::addUserToChat(const db::Chat &chat, const db::User &user)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    for (const auto &chatuser : m_Storage.chatuser)
    {
        if (chatuser.chatid == chat.id && chatuser.uid == user.id)
        {
            return;
        }
    }

    m_Storage.chatuser.emplace_back(db::Chatuser(chat.id, user.id));
}

void InMemoryConnection::saveMessage(const db::Message &msg)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Storage.messages.push_back(msg);
}

std::vector<db::Message> InMemoryConnection::getMessages(uint64_t chatid, const db::get_msg_opt_t &opt) const
// go from recent messages to oldest
{
    std::vector<db::Message> ret;
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_Storage.messages.empty())
    {
        return {};
    }

    for (size_t i = m_Storage.messages.size() - 1; ; --i)
    {
        if (m_Storage.messages[i].chat_to == chatid)
        {
            if (m_Storage.messages[i].ts > opt.ts)
            {
                ret.push_back(m_Storage.messages[i]);
                m_Storage.messages[i].flags = db::Message::flags_t::READ;
            }
        }

        if (opt.max_count && ret.size() >= opt.max_count)
        {
            break;
        }

        if (i == 0)
        {
            break;
        }
    }

    return ret;
}

std::vector<db::Message> InMemoryConnection::selectMessages(std::function<bool(const db::Message &)> &&pred, const db::get_msg_opt_t &opt) const
{
    std::vector<db::Message> ret;
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_Storage.messages.empty())
    {
        return {};
    }

    for (size_t i = m_Storage.messages.size() - 1; ; --i)
    {
        if (pred(m_Storage.messages[i]))
        {
            ret.push_back(m_Storage.messages[i]);
        }

        if (opt.max_count && ret.size() >= opt.max_count)
        {
            break;
        }

        if (i == 0)
        {
            break;
        }
    }

    return ret;
}

