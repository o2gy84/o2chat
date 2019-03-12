#include <chrono>
#include <atomic>
#include <thread>
#include <iostream>

#include "event_worker.hpp"
#include "common/common.hpp"

#include "o2logger/src/o2logger.hpp"
using namespace o2logger;


extern std::atomic<bool> g_NeedStop;


std::ostream& operator<<(std::ostream &os, const Event &ev)
{
    return os << ev.from << " --> " << ev.to << ": " << ev.msg << std::endl;
}

EventsWorker::EventsWorker(size_t workers) :
    m_Workers(workers)
{
}

void EventsWorker::registerOnInputCallback(std::function<void(Event &&event)> &&callback)
{
    m_OnUserInput = std::move(callback);
}

void EventsWorker::putEvent(Event &&event)
{
    m_Queue.push(std::move(event));
}

void EventsWorker::processQueue()
{
    while (!g_NeedStop)
    {
        Event event;
        if (!m_Queue.getTask(event))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        //logd("GET EV: ", event);

        if (event.type == event::type_t::PEER)
        {
            // interface->show();
        }
        else if (event.type == event::type_t::INTERFACE)
        {
            m_OnUserInput(std::move(event));
        }
    }
}

void EventsWorker::run()
{
    for (size_t i = 0; i < m_Workers; ++i)
    {
        m_Threads.push_back(std::thread(std::bind(&EventsWorker::processQueue, this)));
    }
}

void EventsWorker::join()
{
    for (auto &thread : m_Threads)
    {
        thread.join();
    }
}

