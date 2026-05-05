#pragma once
#include <vector>
#include <string>
#include <algorithm>

// 网络底层缓冲区
class Buffer
{
public:
    Buffer(size_t initialSize = 1024)
        : m_buffer(initialSize)
        , m_readerIndex(0)
        , m_writerIndex(0)
    {}

    // 可读的数据长度
    size_t ReadableBytes() const { return m_writerIndex - m_readerIndex; }
    
    // 可写的数据长度
    size_t WritableBytes() const { return m_buffer.size() - m_writerIndex; }

    // 返回缓冲区可读数据的起始地址
    const char* Peek() const { return m_buffer.data() + m_readerIndex; }

    // 取出指定长度的数据后，移动读指针
    void Retrieve(size_t len)
    {
        if (len < ReadableBytes())
        {
            m_readerIndex += len;
        }
        else
        {
            RetrieveAll();
        }
    }

    // 复位缓冲区指针
    void RetrieveAll()
    {
        m_readerIndex = 0;
        m_writerIndex = 0;
    }

    // 把缓冲区所有可读数据转成 string 返回，并复位指针
    std::string RetrieveAllAsString()
    {
        std::string str(Peek(), ReadableBytes());
        RetrieveAll();
        return str;
    }

    // 往缓冲区写入数据
    void Append(const char* data, size_t len)
    {
        EnsureWritableBytes(len);
        std::copy(data, data + len, BeginWrite());
        m_writerIndex += len;
    }

    // 返回缓冲区可写位置的起始地址
    char* BeginWrite() { return m_buffer.data() + m_writerIndex; }

private:
    char* Begin() { return m_buffer.data(); }
    const char* Begin() const { return m_buffer.data(); }

    // 确保有足够的可用空间来写入数据
    void EnsureWritableBytes(size_t len)
    {
        if (WritableBytes() < len)
        {
            MakeSpace(len);
        }
    }

    // 内部扩容与数据前移逻辑
    void MakeSpace(size_t len)
    {
        // 如果剩余可写空间 + 读指针前已废弃的空间 < 需要写入的空间，直接扩容
        if (WritableBytes() + m_readerIndex < len)
        {
            m_buffer.resize(m_writerIndex + len);
        }
        else
        {
            // 空间足够，把可读数据往前挪动到 buffer 起始位置
            size_t readable = ReadableBytes();
            std::copy(Begin() + m_readerIndex, Begin() + m_writerIndex, Begin());
            m_readerIndex = 0;
            m_writerIndex = m_readerIndex + readable;
        }
    }

    std::vector<char> m_buffer;
    size_t m_readerIndex; // 读指针
    size_t m_writerIndex; // 写指针
};