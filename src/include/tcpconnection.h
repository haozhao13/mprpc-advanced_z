#pragma once
#include "buffer.h"
#include <memory>
#include <string>
#include <functional>
#include <unistd.h>

class TcpConnection;

// 定义智能指针，交由系统自动管理连接生命周期
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
// 连接建立/断开的回调
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
// 收到消息的回调
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*)>;

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(int fd);
    ~TcpConnection();

    int fd() const { return m_fd; }
    bool connected() const { return m_state == kConnected; }

    // 提供给上层的发送数据接口
    void Send(const std::string& msg);
    // 提供给上层的关闭连接接口
    void Shutdown();

    // 绑定上层 (RpcProvider) 的回调函数
    void SetConnectionCallback(const ConnectionCallback& cb) { m_connectionCallback = cb; }
    void SetMessageCallback(const MessageCallback& cb) { m_messageCallback = cb; }

    // 供底层 WebServer Epoll 调用的读写处理函数
    void HandleRead();
    void HandleWrite();
    void HandleClose();

private:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
    void SetState(StateE state) { m_state = state; }

    int m_fd;
    StateE m_state;

    Buffer m_inputBuffer;  // 接收缓冲区
    Buffer m_outputBuffer; // 发送缓冲区

    ConnectionCallback m_connectionCallback;
    MessageCallback m_messageCallback;
};