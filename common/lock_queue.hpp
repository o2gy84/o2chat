#pragma once

#include <queue>
#include <mutex>
#include <vector>

template<typename T>
class Queue
{
public:
    void push(const T &t);
    // void push(const std::vector<T> &vec);
    void pop();
    T &front();
    size_t size();

    bool getTask(T &t);
    bool getTask(std::vector<T> &t, size_t n);

private:
    std::queue<T> m_Queue;
    std::mutex m_Mtx;
};


// implementation

template<typename T>
void Queue<T>::push(const T &t)
{
    std::lock_guard<std::mutex> lock(m_Mtx);
    m_Queue.push(t);
}
/*
template<typename T>
void Queue<T>::push(const std::vector<T> &vec)
{
    std::lock_guard<std::mutex> lock(m_Mtx);
    for (const auto &v: vec)
    {
        m_Queue.push(v);
    }
}
*/

template<typename T>
void Queue<T>::pop()
{
    std::lock_guard<std::mutex> lock(m_Mtx);
    m_Queue.pop();
}

template<typename T>
T &Queue<T>::front()
{
    std::lock_guard<std::mutex> lock(m_Mtx);
    return m_Queue.front();
}

template<typename T>
size_t Queue<T>::size()
{
    std::lock_guard<std::mutex> lock(m_Mtx);
    return m_Queue.size();
}

template<typename T>
bool Queue<T>::getTask(T &t)
{
    std::lock_guard<std::mutex> lock(m_Mtx);
    if (m_Queue.empty())
    {
        return false;
    }
    t = m_Queue.front();
    m_Queue.pop();
    return true;
}

template<typename T>
bool Queue<T>::getTask(std::vector<T> &t, size_t n)
{
    std::lock_guard<std::mutex> lock(m_Mtx);
    if (m_Queue.empty())
    {
        return false;
    }

    while (n--)
    {
        t.push_back(m_Queue.front());
        m_Queue.pop();
        if (m_Queue.empty())
        {
            return true;
        }
    }

    return true;
}

