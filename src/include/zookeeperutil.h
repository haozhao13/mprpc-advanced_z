#pragma once
#include <semaphore.h>
#include <zookeeper/zookeeper.h>
#include <string>
#include <vector>

// 封装 Zookeeper 客户端类
class ZkClient
{
public:
    ZkClient();
    ~ZkClient();
    
    // 启动连接 zkserver
    void Start();
    
    // 在 zkserver 上根据指定的 path 创建 znode 节点
    // state=0 为永久性节点，state=ZOO_EPHEMERAL 为临时性节点
    void Create(const char *path, const char *data, int datalen, int state = 0);

    // 获取指定路径下的所有子节点
    std::vector<std::string> GetChildren(const char *path);
    
    // 根据参数指定的 znode 节点路径，获取 znode 节点的值
    std::string GetData(const char *path);

    // 暴露底层句柄，用于原生 Watcher 注册_2026.05.04
    zhandle_t* GetZhandle() const { return m_zhandle; }

private:
    // zk的客户端句柄
    zhandle_t *m_zhandle;
};