#include "connpool.h"
#include "log.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

ConnectionPool& ConnectionPool::GetInstance() 
{
    static ConnectionPool pool;
    return pool;
}

int ConnectionPool::GetConnection(const std::string& ip, uint16_t port) 
{
    std::string key = ip + ":" + std::to_string(port);
    std::unique_lock<std::mutex> lock(m_mutex);
    
    // 1. 若队列中有空闲连接，直接弹出复用
    if (!m_connMap[key].empty()) 
    {
        int fd = m_connMap[key].front();
        m_connMap[key].pop();
        return fd;
    }
    
    // 2. 若队列为空，释放锁后进行耗时的网络连接操作
    lock.unlock(); 
    
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

    if (-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr))) 
    {
        LOG_ERROR("connect error! errno:%d", errno);
        close(clientfd);
        return -1;
    }

    return clientfd;
}

void ConnectionPool::ReleaseConnection(const std::string& ip, uint16_t port, int fd) 
{
    if (fd == -1) return;
    std::string key = ip + ":" + std::to_string(port);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connMap[key].push(fd); // 归还队列尾部
}