#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <deque>
#include <netinet/in.h>
#include <time.h>
#include "log.h"

using namespace std;

#define BUFFER_SIZE 64

class util_timer; // 前向声明

// 用户数据结构
struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer *timer;
};

// 定时器类
class util_timer
{
public:
    util_timer() : cb_func(NULL), user_data(NULL) {}

public:
    time_t expire; // 超时时间
    
    // 回调函数
    void (*cb_func)(client_data *);
    client_data *user_data;
    
    // 注意：堆实现不需要 prev/next 指针，因为我们用数组存
};

// 最小堆定时器管理类
class time_heap
{
public:
    // 构造函数：初始化堆大小
    // capacity: 预估的最大连接数，比如 10000
    time_heap(int capacity) : capacity(capacity), cur_size(0)
    {
        array = new util_timer *[capacity]; // 创建指针数组
        if (!array)
        {
            throw std::exception();
        }
        for (int i = 0; i < capacity; ++i)
        {
            array[i] = NULL;
        }
    }
    
    // 析构函数
    ~time_heap()
    {
        for (int i = 0; i < cur_size; ++i)
        {
            delete array[i];
        }
        delete[] array;
    }

    // 添加定时器：O(log N)
    void add_timer(util_timer *timer)
    {
        if (!timer) return;
        
        if (cur_size >= capacity)
        {
            resize(); // 容量不够时扩容
        }

        // 1. 先把新节点放到数组最后
        int hole = cur_size++;
        int parent = 0;
        
        // 2. 上滤 (Percolate Up)：如果比父节点小，就交换
        for (; hole > 0; hole = parent)
        {
            parent = (hole - 1) / 2;
            if (array[parent]->expire <= timer->expire)
            {
                break; 
            }
            array[hole] = array[parent];
        }
        array[hole] = timer;
    }

    // 删除定时器：惰性删除 O(1)
    // 我们不真正删除它，只是把它的回调函数设为 NULL
    // 等到 tick 到它的时候，发现是 NULL 再删
    void del_timer(util_timer *timer)
    {
        if (!timer) return;
        // 只是把回调置空，并未释放内存，也没有从数组移除
        timer->cb_func = NULL;
    }

    // 调整定时器：O(log N)
    // 通常是因为连接有活动，过期时间延长了，需要往下沉
    // 调整定时器：策略是“惰性刷新”
    // 我们找不到旧定时器在堆里的位置，所以直接把它废弃，然后加个新的
    void adjust_timer(util_timer *timer)
    {
        if (!timer) return;
        
        // 1. 保存旧定时器的关键信息
        // 因为我们要把旧的废弃掉，得先把回调函数和用户数据存下来
        void (*cb_func)(client_data *) = timer->cb_func;
        client_data *user_data = timer->user_data;
        
        // 2. 【核心步骤】废弃旧定时器
        // 将回调置空，tick() 遍历到它时会直接跳过并删除
        // 注意：这里不要 delete timer，因为它的指针还在堆数组里，delete了会导致堆数组里有悬空指针！
        // 内存释放交给 tick() 的 pop_timer() 去做。
        timer->cb_func = NULL;
        
        // 3. 创建新定时器
        util_timer *new_timer = new util_timer;
        new_timer->user_data = user_data;
        new_timer->cb_func = cb_func;
        
        // 4. 设置新的超时时间 (当前时间 + 3倍的时间槽)
        // TIMESLOT 需要你在头部定义，比如 #define TIMESLOT 5
        new_timer->expire = time(NULL) + 3 * 5; 

        // 5. 【关键】更新用户数据中的指针
        // 这一点非常重要！否则下次有数据来，找到的还是旧的 timer
        user_data->timer = new_timer;

        // 6. 加入堆
        add_timer(new_timer);
    }

    // 获取堆顶
    util_timer *top() const
    {
        if (cur_size == 0) return NULL;
        return array[0];
    }

    // 删除堆顶
    void pop_timer()
    {
        if (cur_size == 0) return;
        if (array[0])
        {
            delete array[0];
            array[0] = NULL;
        }
        
        // 1. 把最后一个元素移到堆顶
        array[0] = array[--cur_size];
        
        // 2. 下滤 (Percolate Down)
        int hole = 0;
        int child = 0;
        util_timer *temp = array[0];
        
        for (; hole * 2 + 1 < cur_size; hole = child)
        {
            child = hole * 2 + 1;
            // 找左右孩子里更小的那个
            if ((child < cur_size - 1) && (array[child + 1]->expire < array[child]->expire))
            {
                child++;
            }
            if (array[child]->expire < temp->expire)
            {
                array[hole] = array[child];
            }
            else
            {
                break;
            }
        }
        array[hole] = temp;
    }

    // 心跳函数：处理过期连接
    void tick()
    {
        util_timer *tmp = array[0];
        time_t cur = time(NULL);
        
        while (cur_size > 0)
        {
            if (!tmp) break;
            
            // 如果堆顶的 expire 大于当前时间，说明堆里剩下的都没过期（最小堆性质）
            if (tmp->expire > cur)
            {
                break;
            }
            
            // 【核心处理】
            // 如果 cb_func 是 NULL，说明这个定时器之前被 adjust_timer 废弃了
            // 我们什么都不做，直接把它 pop 掉（惰性删除在这里真正生效）
            if (array[0]->cb_func)
            {
                // 只有活着的定时器才执行回调
                array[0]->cb_func(array[0]->user_data);
            }
            
            // 弹出堆顶（这里会 delete 掉旧的 timer 内存）
            pop_timer();
            
            // 重置 tmp 指向新的堆顶
            tmp = array[0];
        }
    }
    
    bool empty() const { return cur_size == 0; }

private:
    // 数组扩容：容量翻倍
    void resize()
    {
        util_timer **temp = new util_timer *[2 * capacity];
        for (int i = 0; i < 2 * capacity; ++i)
        {
            temp[i] = NULL;
        }
        if (!temp) throw std::exception();
        
        capacity = 2 * capacity;
        for (int i = 0; i < cur_size; ++i)
        {
            temp[i] = array[i];
        }
        delete[] array;
        array = temp;
    }

    util_timer **array; // 堆数组
    int capacity;       // 堆容量
    int cur_size;       // 当前元素个数
};

#endif