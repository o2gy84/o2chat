#pragma once

#include <thread>

#include "common/lock_queue.hpp"


namespace event
{

enum type_t
{
    NONE,
    PEER,
    INTERFACE
};

}   // namespace


struct Event
{
    Event() {}
    Event(event::type_t type, const std::string &f, const std::string &t, const std::string &m) :
        type(type),
        from(f),
        to(t),
        msg(m)
    {}
    event::type_t type = event::type_t::NONE;
    std::string from;
    std::string to;
    std::string msg;
};

std::ostream& operator<<(std::ostream &os, const Event &ev);

class Client;

class EventsWorker
{
public:
    EventsWorker(size_t workers);
    void putEvent(Event &&event);
    void run();
    void join();
    void registerOnInputCallback(std::function<void(Event &&event)> &&callback);

private:
    void processQueue();

private:
    size_t m_Workers;
    Queue<Event> m_Queue;
    std::vector<std::thread> m_Threads;

    std::function<void(Event &&event)> m_OnUserInput;
};
