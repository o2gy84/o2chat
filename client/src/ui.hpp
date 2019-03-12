#pragma once

#include <mutex>
#include <thread>
#include <string>
#include <vector>


class EventsWorker;

namespace ui
{

struct Message
{
    std::string from;
    std::string to;
    std::string message;
};

}   // namespace

class AbstractUi
{
public:
    AbstractUi(EventsWorker &ew) : m_EventWorker(ew) {}
    virtual ~AbstractUi() {}
    virtual void showMsg(const ui::Message &msg) = 0;
    virtual void showMsg(const std::string &msg) = 0;
    virtual void run() = 0;
    virtual void join() = 0;

protected:
    EventsWorker &m_EventWorker;
};

class ConsoleUi : public AbstractUi
{
public:
    ConsoleUi(EventsWorker &w) : AbstractUi(w) {}
    void showMsg(const ui::Message &msg) override;
    void showMsg(const std::string &msg) override;
    void run() override;
    void join() override;

private:
    void pollingUserInput();

private:
    std::mutex m_Mutex;
    std::vector<std::thread> m_Threads;
};
