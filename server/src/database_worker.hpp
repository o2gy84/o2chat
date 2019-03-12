#pragma once

#include <thread>

#include "database.hpp"
#include "common/lock_queue.hpp"


class DatabaseWorker
{
public:
    DatabaseWorker(db::type_t type,  size_t workers);
    void putTask(db::Task &&task);
    void run();
    void join();

private:
    void processQueue();

private:
    size_t m_Workers;
    Queue<db::Task> m_Queue;
    std::unique_ptr<AbstractDatabase> m_Db;
    std::vector<std::thread> m_Threads;
};
