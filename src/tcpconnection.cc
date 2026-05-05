#include "tcpconnection.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>

TcpConnection::TcpConnection(int fd)
    : m_fd(fd)
    , m_state(kConnected)
{
}

TcpConnection::~TcpConnection()
{
    close(m_fd);
}

void TcpConnection::Send(const std::string& msg)
{
    if (m_state == kConnected)
    {
        m_outputBuffer.Append(msg.data(), msg.size());
        // 将数据写入缓冲区后，立刻尝试发送
        HandleWrite(); 
    }
}

void TcpConnection::Shutdown()
{
    if (m_state == kConnected)
    {
        SetState(kDisconnecting);
        shutdown(m_fd, SHUT_WR);
    }
}

void TcpConnection::HandleRead()
{
    char buf[65536];
    while (true)
    {
        ssize_t n = recv(m_fd, buf, sizeof(buf), MSG_DONTWAIT);
        if (n > 0)
        {
            m_inputBuffer.Append(buf, n);
        }
        else if (n == 0) // 客户端断开连接
        {
            HandleClose();
            break;
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) // 读空了
            {
                break;
            }
            else if (errno == EINTR) // 被中断
            {
                continue;
            }
            else // 真正发生错误
            {
                HandleClose();
                break;
            }
        }
    }

    // 数据读完后，若连接正常且绑定了消息回调，则抛给上层 RpcProvider 反序列化
    if (m_state == kConnected && m_messageCallback && m_inputBuffer.ReadableBytes() > 0)
    {
        m_messageCallback(shared_from_this(), &m_inputBuffer);
    }
}

void TcpConnection::HandleWrite()
{
    if (m_state == kConnected && m_outputBuffer.ReadableBytes() > 0)
    {
        ssize_t n = send(m_fd, m_outputBuffer.Peek(), m_outputBuffer.ReadableBytes(), MSG_DONTWAIT);
        if (n > 0)
        {
            m_outputBuffer.Retrieve(n);
            if (m_outputBuffer.ReadableBytes() == 0 && m_state == kDisconnecting)
            {
                HandleClose();
            }
        }
    }
}

void TcpConnection::HandleClose()
{
    m_state = kDisconnected;
    if (m_connectionCallback)
    {
        m_connectionCallback(shared_from_this());
    }
}