#pragma once
#include <queue>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <thread>

// 定义连接结构体，记录 fd 和放入池中的时间戳
struct ConnectionItem {
    int fd;
    std::chrono::steady_clock::time_point last_active_time;
};

class ConnectionPool {
public:
    static ConnectionPool& GetInstance();
    int GetConnection(const std::string& ip, uint16_t port);
    int CreateRealSocket(const std::string& ip, uint16_t port); // 封装原有的连接逻辑
    void ReleaseConnection(const std::string& ip, uint16_t port, int fd);

private:
    ConnectionPool();
    ~ConnectionPool();
    void CleanIdleConnections(); // 后台清理任务

    // 参数设置
    const int m_maxConnections = 5000;    // 最大连接总数
    const int m_maxIdleTime = 60;       // 连接最大空闲时间（秒）
    
    std::atomic<int> m_curConnCount{0}; // 当前已创建的总连接数
    std::mutex m_mutex;
    std::condition_variable m_cv;       // 用于处理连接数满时的等待
    
    // Key: ip:port, Value: 存放带时间戳的连接项
    std::unordered_map<std::string, std::queue<ConnectionItem>> m_connMap; 
    
    bool m_stop; // 线程池停止标志
    std::thread m_cleanThread; // 清理线程
};