#include "mprpcchannel.h"
#include "rpcheader.pb.h"
#include "mprpcapplication.h"
#include "log.h"
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include "zookeeperutil.h"
#include "connpool.h"
#include "mprpccontroller.h"
#include <thread>
#include "ConsistentHash.h"


#include <mutex>
#include <unordered_map>
#include <memory>


// 1. 全局静态缓存与锁
// 缓存结构：method_path -> ConsistentHash 对象
static std::unordered_map<std::string, ConsistentHash> g_route_cache;
static std::mutex g_cache_mutex;

// 2. 全局唯一的 ZkClient 单例（保证客户端进程生命周期内只连一次 ZK）
static ZkClient* g_zkCli = nullptr;
static std::once_flag g_zk_init_flag;

// 3. 全局 Watcher 回调函数（由 Zookeeper C 客户端后台线程触发）
void ZkChildWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx) 
{
    // ZOO_CHILD_EVENT 表示其子节点发生了增减（即服务提供者上下线）
    if (type == ZOO_CHILD_EVENT) 
    {
        std::string method_path = path;
        String_vector children;
        
        // 重新拉取最新的节点列表，并【再次注册 Watcher】
        // zoo_wget_children 是 ZK 提供的 C API，可以在获取的同时绑定 Watcher
        int flag = zoo_wget_children(zh, path, ZkChildWatcher, nullptr, &children);
        
        if (flag == ZOK) 
        {
            std::vector<std::string> new_host_list;
            for (int i = 0; i < children.count; ++i) 
            {
                new_host_list.push_back(children.data[i]);
            }
            deallocate_String_vector(&children); // 防止内存泄漏
            
            // 构建新的哈希环
            ConsistentHash new_ch;
            new_ch.AddNodes(new_host_list);
            
            // 加锁，更新全局缓存
            std::lock_guard<std::mutex> lock(g_cache_mutex);
            g_route_cache[method_path] = new_ch;
            LOG_INFO("ZK nodes updated for %s, rebuilt Consistent Hash Ring.", path);
        }
    }
}

/* Stub在调用任何业务方法（如 Login）时，底层都会统一调用 channel_->CallMethod(...)
因此负载均衡是在该程序（mprpcchannel.cc）中实现的，因为该程序仍属于“客户端的范畴”。 */
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                              google::protobuf::RpcController* controller,
                              const google::protobuf::Message* request,
                              google::protobuf::Message* response,
                              google::protobuf::Closure* done)
{
    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string service_name = sd->name();
    std::string method_name = method->name();

    // 1. 获取参数的序列化字符串长度
    uint32_t args_size = 0;
    std::string args_str;
    if (request->SerializeToString(&args_str))
    {
        args_size = args_str.size();
    }
    else
    {
        controller->SetFailed("serialize request error!");
        LOG_ERROR("serialize request error!");
        return;
    }

    // 2. 定义 RPC 的请求头
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if (rpcHeader.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();
    }
    else
    {
        controller->SetFailed("serialize rpc header error!");
        LOG_ERROR("serialize rpc header error!");
        return;
    }

    // 3. 组织待发送的完整 RPC 报文: [header_size (4 bytes)] + [RpcHeader] + [Args]
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char*)&header_size, 4));
    send_rpc_str += rpc_header_str;
    send_rpc_str += args_str;

    // 4. 查询 zk 上服务所在的 host 信息
    // ZkClient zkCli;
    // zkCli.Start();

    // ----------------- 【重构开始：服务发现与哈希环缓存机制】 ----------------- //
    std::string method_path = "/" + service_name + "/" + method_name;

    // 4.1 确保整个进程生命周期内，只初始化连接一次 Zookeeper
    std::call_once(g_zk_init_flag, [](){
        g_zkCli = new ZkClient();
        g_zkCli->Start();
    });

    ConsistentHash ch;
    bool cache_hit = false;

    // 4.2 优先从本地缓存中查找该服务对应的哈希环
    {
        std::lock_guard<std::mutex> lock(g_cache_mutex);
        auto it = g_route_cache.find(method_path);
        if (it != g_route_cache.end()) 
        {
            ch = it->second;
            cache_hit = true;
        }
    }

    // 4.3 如果缓存未命中（说明是进程启动后第一次调用该方法）
    if (!cache_hit) 
    {
        String_vector children;
        // 首次拉取子节点，并绑定 ZkChildWatcher 监听器
        int flag = zoo_wget_children(g_zkCli->GetZhandle(), method_path.c_str(), ZkChildWatcher, nullptr, &children);
        
        if (flag == ZOK) 
        {
            std::vector<std::string> host_list;
            for (int i = 0; i < children.count; ++i) 
            {
                host_list.push_back(children.data[i]);
            }
            deallocate_String_vector(&children);
            
            if (host_list.empty()) 
            {
                controller->SetFailed(method_path + " is not exist or no provider!");
                LOG_ERROR("%s is not exist or no provider!", method_path.c_str());
                return;
            }

            // 初始化哈希环并写入缓存
            ch.AddNodes(host_list);
            
            std::lock_guard<std::mutex> lock(g_cache_mutex);
            g_route_cache[method_path] = ch;
        } 
        else 
        {
            controller->SetFailed(method_path + " get children error!");
            LOG_ERROR("get znode children error... path:%s", method_path.c_str());
            return;
        }
    }

    // 4.4 从哈希环中执行一致性哈希寻址 (复用缓存的环结构，极大提升性能)
    // 使用method_name这一静态数据进行哈希计算！这是不对的！应改为动态的数据去进行哈希计算
    // std::string host_data = ch.GetTargetHost(method_name);

    // ---------------------- 【重构结束】 ---------------------- //

    // ================= 【从静态哈希_改进为动态哈希】 ================= //
    // 1. 默认使用方法名，保证在没有特定标识时也能正常路由
    std::string hash_key = method_name; 

    // 2. 针对特定业务场景提取动态参数作为哈希 Key
    // 假设服务名是 fixbug.UserServiceRpc，方法名是 Login
    if (service_name == "UserServiceRpc" && method_name == "Login") 
    {
        // 使用 Protobuf 的反射(Reflection)接口动态获取 "name" 字段的值
        // 这样不需要在框架代码里强制包含具体的 user.pb.h
        const google::protobuf::Descriptor* descriptor = request->GetDescriptor();
        const google::protobuf::FieldDescriptor* field = descriptor->FindFieldByName("name");
    
        if (field != nullptr) 
        {
            // 提取请求体里的用户名（如 "paris_async"）
            hash_key = request->GetReflection()->GetString(*request, field);
        }
    }
    // 如果有其他服务，可以继续 else if ...

    // 3. 将动态提取的 hash_key 传入一致性哈希环
    std::string host_data = ch.GetTargetHost(hash_key);


    // ======================== 【改进逻辑结束】 ======================== //




    // 解析出 ip 和 port
    int idx = host_data.find(":");
    if (idx == -1)
    {
        controller->SetFailed(method_path + " address is invalid!");
        LOG_ERROR("%s address is invalid!", method_path.c_str());
        return;
    }
    std::string ip = host_data.substr(0, idx);
    uint16_t port = atoi(host_data.substr(idx + 1, host_data.size() - idx).c_str());

    // 5. 将网络传输和解析逻辑封装为 lambda 任务
    // 注意：ip 和 send_rpc_str 等字符串必须按值捕获以防局部变量销毁，指针对象按值捕获其地址
    auto rpc_task = [ip, port, send_rpc_str, response, controller, done]() {
        int clientfd = ConnectionPool::GetInstance().GetConnection(ip, port);
        if (-1 == clientfd)
        {
            controller->SetFailed("get connection from pool error!");
            if (done) done->Run();
            return;
        }

        if (-1 == send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0))
        {
            controller->SetFailed("send error! errno:" + std::to_string(errno));
            close(clientfd);
            if (done) done->Run();
            return;
        }

        char recv_buf[1024] = {0};
        int recv_size = 0;
        if (-1 == (recv_size = recv(clientfd, recv_buf, 1024, 0)))
        {
            controller->SetFailed("recv error! errno:" + std::to_string(errno));
            close(clientfd);
            if (done) done->Run();
            return;
        }

        if (!response->ParseFromArray(recv_buf, recv_size))
        {
            controller->SetFailed("parse response error!");
            ConnectionPool::GetInstance().ReleaseConnection(ip, port, clientfd);
            if (done) done->Run();
            return;
        }

        ConnectionPool::GetInstance().ReleaseConnection(ip, port, clientfd);
        
        // 关键：若包含回调函数，则执行回调通知调用方 RPC 已完成
        if (done)
        {
            done->Run();
        }
    };

    // 6. 根据 done 是否为空，选择同步阻塞或异步分离线程执行
    if (done == nullptr)
    {
        rpc_task(); // 同步
    }
    else
    {
        std::thread(rpc_task).detach(); // 异步
    }
}



