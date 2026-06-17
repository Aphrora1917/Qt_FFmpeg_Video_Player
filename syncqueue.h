#ifndef SYNCQUEUE_H
#define SYNCQUEUE_H

#include <utility>
#include <mutex>
#include <condition_variable>
#include <deque>

template<typename T>
class SyncQueue
{
public:
    SyncQueue(int max_size) : m_max_size(max_size){}
    ~SyncQueue(){};

    // 拷贝入队
    void enqueue(const T& data)
    {
        std::unique_lock<std::mutex> locker(m_mutex);     // std::condition_variable专属的unique_lock，用来搭配条件变量进行抢锁、上锁和解锁

        m_not_full.wait(locker, [this](){       // wait的重载，接受lamda表达式返回值。如果返回值为true，则当前线程重写获得互斥锁所有权，结束阻塞，继续向下执行；如果返回值为false，则当前线程释放互斥锁，同时被阻塞，等待被唤醒。
            return m_queue.size() < m_max_size;
        });
        m_queue.push_back(data);
        m_not_empty.notify_one();
    }

    // 移动入队
    void enqueue(T&& data)
    {
        std::unique_lock<std::mutex> locker(m_mutex);

        m_not_full.wait(locker, [this](){
            return m_queue.size() != m_max_size;
        });
        m_queue.push_back(std::move(data));
        m_not_empty.notify_all();
    }

    // 出队，通用（普通类型、共享智能指针、独占智能指针均可使用）
    T dequeue()
    {
        std::unique_lock<std::mutex> locker(m_mutex);

        m_not_empty.wait(locker, [this](){
            return !m_queue.empty();
        });
        T value = std::move(m_queue.front());       // 对于普通类型是拷贝，对于独占智能指针是移动构造，front()只是读数据，还需后续删除操作
        m_queue.pop_front();        // 删除队列头部数据
        m_not_full.notify_all();
        return value;       // 如果是普通类型则拷贝，如果是独占智能指针则移动（编译器RVO），并且返回值为“T”明确传达独占智能指针已转移所有权
    }

    bool isEmpty()
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_queue.empty();
    }

    bool isFull()
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_queue.size() == m_max_size;
    }

    int size()
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_queue.size();
    }

private:
    int m_max_size = 10;        // 队列最大容量，默认10

    std::deque<T> m_queue;      // 容器，双端队列

    std::mutex m_mutex;     // 互斥锁

    std::condition_variable m_not_empty;    // 条件变量，非空
    std::condition_variable m_not_full;     // 条件变量，非满
};

#endif // SYNCQUEUE_H
