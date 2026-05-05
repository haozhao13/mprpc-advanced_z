#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>

using namespace std;

// 模板类：泛型阻塞队列
template<class T>
class BlockQueue {
public:
    // 初始化队列最大容量
    BlockQueue(int max_size = 1000) {
        if(max_size <= 0) {
            exit(-1);
        }
        m_max_size = max_size;
        m_is_close = false;
    }

    ~BlockQueue() {
        close();
    }

    // 清空队列
    void clear() {
        lock_guard<mutex> locker(m_mutex);
        m_queue.clear();
    }

    // 判断队列是否满
    bool full() {
        lock_guard<mutex> locker(m_mutex);
        if(m_queue.size() >= m_max_size) {
            return true;
        }
        return false;
    }

    // 判断队列是否为空
    bool empty() {
        lock_guard<mutex> locker(m_mutex);
        return m_queue.empty();
    }

    // 返回队首元素
    bool front(T &value) {
        lock_guard<mutex> locker(m_mutex);
        if(m_queue.size() == 0) {
            return false;
        }
        value = m_queue.front();
        return true;
    }

    // 返回队尾元素
    bool back(T &value) {
        lock_guard<mutex> locker(m_mutex);
        if(m_queue.size() == 0) {
            return false;
        }
        value = m_queue.back();
        return true;
    }

    // 【核心】生产者：往队列里塞数据
    bool push(const T &item) {
        unique_lock<mutex> locker(m_mutex);
        
        // 如果队列满了，就通知消费者快点取，不仅如此，为了保险这里返回false（或者可以选择等待）
        if(m_queue.size() >= m_max_size) {
            m_cond.notify_all();
            return false;
        }

        m_queue.push_back(item);
        m_cond.notify_one(); // 唤醒一个正在等待的消费者（写线程）
        return true;
    }

    // 【核心】消费者：从队列里取数据
    // 如果队列为空，线程会卡在这里休眠(wait)，直到有数据进来
    bool pop(T &item) {
        unique_lock<mutex> locker(m_mutex);
        
        // 循环等待，防止虚假唤醒
        while(m_queue.size() <= 0) {
            if(m_is_close) return false;
            m_cond.wait(locker); // 阻塞等待，直到被 notify
        }

        item = m_queue.front();
        m_queue.pop_front();
        return true;
    }

    // 增加超时处理的 pop
    bool pop(T &item, int ms_timeout) {
        unique_lock<mutex> locker(m_mutex);
        if(m_queue.size() <= 0) {
            if(m_is_close) return false;
            // 等待指定时间
            if(m_cond.wait_for(locker, chrono::milliseconds(ms_timeout)) == cv_status::timeout) { 
                return false; 
            }
        }
        if(m_is_close) return false;
        if(m_queue.size() <= 0) return false; // 再次检查

        item = m_queue.front();
        m_queue.pop_front();
        return true;
    }

    void close() {
        {   
            lock_guard<mutex> locker(m_mutex);
            m_queue.clear();
            m_is_close = true;
        }
        m_cond.notify_all();
    }

private:
    deque<T> m_queue; // 底层容器
    size_t m_max_size; // 最大容量
    mutex m_mutex;     // 互斥锁
    condition_variable m_cond; // 条件变量
    bool m_is_close;
};

#endif