#pragma once
#include "tcpconnection.h"
#include <string>
#include <functional>

// 封装底层 Epoll 网络的服务器类
class TcpServer
{
public:
    using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
    using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*)>;

    TcpServer(const std::string &ip, uint16_t port);
    
    // 供上层 RpcProvider 注入业务逻辑的回调绑定接口
    void SetConnectionCallback(const ConnectionCallback &cb);
    void SetMessageCallback(const MessageCallback &cb);
    
    // 启动网络事件循环
    void Start();

private:
    std::string m_ip;
    uint16_t m_port;
    ConnectionCallback m_connectionCallback;
    MessageCallback m_messageCallback;
};