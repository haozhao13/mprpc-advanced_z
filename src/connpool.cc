#include "connpool.h"
#include "log.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <thread>

ConnectionPool& ConnectionPool::GetInstance() 
{
    static ConnectionPool pool;
    return pool;
}

// 构造函数实现：初始化成员并启动后台清理线程
ConnectionPool::ConnectionPool() : m_curConnCount(0), m_stop(false) 
{
    // 启动后台线程执行消亡机制
    m_cleanThread = std::thread(&ConnectionPool::CleanIdleConnections, this);
}

// 析构函数实现
ConnectionPool::~ConnectionPool() 
{
    m_stop = true;
    if (m_cleanThread.joinable()) {
        m_cleanThread.join();
    }

    // 释放池中所有残留的 socket fd 资源
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto &pair : m_connMap) {
        while (!pair.second.empty()) {
            close(pair.second.front().fd);
            pair.second.pop();
        }
    }
}

// 原实现逻辑的封装（第一次构建长连接时使用）
int ConnectionPool::CreateRealSocket(const std::string& ip, uint16_t port) 
{
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd) 
    {
        LOG_ERROR("create socket error! errno:%d", errno);
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    // 耗时的三次握手操作
    if (-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr))) 
    {
        LOG_ERROR("connect error! errno:%d", errno);
        close(clientfd);
        return -1;
    }
    return clientfd;
}

int ConnectionPool::GetConnection(const std::string& ip, uint16_t port) {
    std::string key = ip + ":" + std::to_string(port);
    std::unique_lock<std::mutex> lock(m_mutex);

    // 1. 优先检查空闲队列
    if (!m_connMap[key].empty()) {
        int fd = m_connMap[key].front().fd;
        m_connMap[key].pop();
        return fd;
    }

    // 2. 最大连接数限制与等待逻辑
    m_cv.wait(lock, [this, &key] {
        return m_curConnCount < m_maxConnections || !m_connMap[key].empty();
    });

    // 再次确认队列是否因 ReleaseConnection 唤醒而有了新连接
    if (!m_connMap[key].empty()) {
        int fd = m_connMap[key].front().fd;
        m_connMap[key].pop();
        return fd;
    }

    // 3. 确定需要新建连接，先占坑
    m_curConnCount++; 
    lock.unlock(); // 释放锁，允许其他线程访问 map

    int clientfd = CreateRealSocket(ip, port); // 调用刚才封装的原逻辑
    if (clientfd == -1) {
        std::lock_guard<std::mutex> lock_re(m_mutex);
        m_curConnCount--; 
        m_cv.notify_one(); 
    }
    return clientfd;
}

void ConnectionPool::ReleaseConnection(const std::string& ip, uint16_t port, int fd) {
    if (fd == -1) return;
    
    std::string key = ip + ":" + std::to_string(port);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // 存入当前时间戳，供清理线程比对
        m_connMap[key].push({fd, std::chrono::steady_clock::now()});
    }
    m_cv.notify_one(); 
}

void ConnectionPool::CleanIdleConnections() {
    while (!m_stop) {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 缩短巡检周期
        
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_connMap.begin(); it != m_connMap.end(); ++it) {
            auto& queue = it->second;
            while (!queue.empty()) {
                auto& item = queue.front();
                auto now = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - item.last_active_time);
                
                // 自动消亡逻辑：超过 m_maxIdleTime (60s) 则关闭
                if (duration.count() > m_maxIdleTime) {
                    close(item.fd);
                    queue.pop();
                    m_curConnCount--; 
                    LOG_INFO("Connection idle timeout, closed fd:%d", item.fd);
                } else {
                    break; 
                }
            }
        }
    }
}