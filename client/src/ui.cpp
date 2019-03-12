#include <atomic>
#include <iostream>

#include <event_worker.hpp>
#include "ui.hpp"
#include "common/utils.hpp"


extern std::atomic<bool> g_NeedStop;


void ConsoleUi::showMsg(const ui::Message &msg)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::cout << msg.from << " --> " << msg.to << ": " << msg.message << std::endl;
}

void ConsoleUi::showMsg(const std::string &msg)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::cout << ">> " << msg << std::endl;
}

void ConsoleUi::pollingUserInput()
{
    std::string from = "dummy";
    std::string to = "dummy";

    while (!g_NeedStop)
    {
        std::string msg;
        getline(std::cin, msg);
        
        msg = utils::trimmed(msg);
        if (msg.empty())
        {
            continue;
        }

        Event e(event::type_t::INTERFACE, from, to, msg);
        m_EventWorker.putEvent(std::move(e));
    }
}

void ConsoleUi::run()
{
    m_Threads.push_back(std::thread(std::bind(&ConsoleUi::pollingUserInput, this)));
}

void ConsoleUi::join()
{
    for (auto &thread : m_Threads)
    {
        thread.join();
    }
}

