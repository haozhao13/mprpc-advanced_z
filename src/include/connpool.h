#pragma once
#include <string>
#include <queue>
#include <unordered_map>
#include <mutex>

// 客户端 TCP 连接池（单例模式）
class ConnectionPool 
{
public:
    static ConnectionPool& GetInstance();

    // 获取连接：若池中为空则新建，否则复用已有连接
    int GetConnection(const std::string& ip, uint16_t port);

    // 释放连接：将存活的 Socket 归还至连接池
    void ReleaseConnection(const std::string& ip, uint16_t port, int fd);

private:
    ConnectionPool() = default;
    ~ConnectionPool() = default;
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    // 映射关系： "ip:port" -> 空闲的 socket fd 队列
    std::unordered_map<std::string, std::queue<int>> m_connMap;
    // 互斥锁，保障多线程并发获取连接时的线程安全
    std::mutex m_mutex;
};