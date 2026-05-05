#include "rpcprovider.h"
#include "tcpserver.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "log.h"
#include <google/protobuf/descriptor.h>
#include "zookeeperutil.h"

void RpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info;

    // 获取服务对象的描述信息
    const google::protobuf::ServiceDescriptor *pServiceDesc = service->GetDescriptor();
    std::string service_name = pServiceDesc->name();
    int methodCnt = pServiceDesc->method_count();

    // 将服务对象的所有方法及其描述信息记录在哈希表中
    for (int i = 0; i < methodCnt; ++i)
    {
        const google::protobuf::MethodDescriptor* pMethodDesc = pServiceDesc->method(i);
        std::string method_name = pMethodDesc->name();
        service_info.m_methodMap.insert({method_name, pMethodDesc});
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}

void RpcProvider::Run()
{
    // 从全局配置对象中获取服务器 IP 和端口
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserver_ip");
    std::string port_str = MprpcApplication::GetInstance().GetConfig().Load("rpcserver_port");
    uint16_t port = atoi(port_str.c_str());

    // 实例化底层的网络服务器
    TcpServer server(ip, port);

    // 绑定连接回调和消息读写回调
    server.SetConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.SetMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2));

    // 把当前 rpc 节点上要发布的服务全部注册到 zk 上面，让 rpc client 可以从 zk 上发现服务
    ZkClient zkCli;
    zkCli.Start();
    
    // service_name 为永久性节点，method_name 为永久性节点，host 为临时性节点
    for (auto &sp : m_serviceMap) 
    {
        // 1. 创建服务名目录: /UserServiceRpc
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0);
        
        for (auto &mp : sp.second.m_methodMap)
        {
            // 2. 创建方法名目录: /UserServiceRpc/Login
            std::string method_path = service_path + "/" + mp.first;
            zkCli.Create(method_path.c_str(), nullptr, 0);

            // 3. 创建提供者实例节点: /UserServiceRpc/Login/127.0.0.1:8080
            char host_path[128] = {0};
            sprintf(host_path, "%s/%s:%d", method_path.c_str(), ip.c_str(), port);
            
            // ZOO_EPHEMERAL 临时节点。数据传 nullptr 即可，因为 IP 和端口已在路径名中
            zkCli.Create(host_path, nullptr, 0, ZOO_EPHEMERAL);
        }
    }

    LOG_INFO("RpcProvider start service at ip:%s port:%d", ip.c_str(), port);
    
    // 启动网络事件循环，阻塞等待客户端连接
    server.Start();
}

void RpcProvider::OnConnection(const TcpConnectionPtr& conn)
{
    // RPC 请求是短连接行为，客户端断开时服务端直接关闭底层 Socket 即可
    if (!conn->connected())
    {
        // 资源清理在 TcpServer 层已通过 removefd 和哈希表 erase 处理
    }
}

void RpcProvider::OnMessage(const TcpConnectionPtr& conn, Buffer* buffer)
{
    // 提取网络中接收到的全部字符
    std::string recv_buf = buffer->RetrieveAllAsString();

    // 1. 拆包：读取前 4 个字节，获取 RpcHeader 的长度
    uint32_t header_size = 0;  // 长度前缀，4字节，用于解决 TCP 粘包问题，告诉程序接下来的Header有多长
    recv_buf.copy((char*)&header_size, 4, 0);

    // 2. 反序列化：根据 header_size 【提取数据头，解析出服务名、方法名和参数长度】
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;  // 服务名
    std::string method_name;  // 方法名
    uint32_t args_size;  // 参数长度，用于计算 Body 的长度

    if (rpcHeader.ParseFromString(rpc_header_str))
    {
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else
    {
        LOG_ERROR("rpc_header_str:%s parse error!", rpc_header_str.c_str());
        return;
    }

    // 3. 提取 RPC 方法的实际参数内容_Body (长度由 Header 决定)
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    // 4. 路由验证：在已注册的服务表中查找对应的服务与方法
    auto it = m_serviceMap.find(service_name);
    if (it == m_serviceMap.end())
    {
        LOG_ERROR("%s is not exist!", service_name.c_str());
        return;
    }

    auto mit = it->second.m_methodMap.find(method_name);
    if (mit == it->second.m_methodMap.end())
    {
        LOG_ERROR("%s:%s is not exist!", service_name.c_str(), method_name.c_str());
        return;
    }

    // 提取服务对象和方法描述对象
    google::protobuf::Service *service = it->second.m_service;
    const google::protobuf::MethodDescriptor *method = mit->second;

    // 5. 动态生成方法的 Request 和 Response 对象
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(args_str))
    {
        LOG_ERROR("request parse error, content:%s", args_str.c_str());
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    // 6. 绑定闭包 (Closure) 回调：业务层处理完后，自动触发 SendRpcResponse 将数据序列化并通过网络发回
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcProvider, const TcpConnectionPtr&, google::protobuf::Message*>
                                      (this, &RpcProvider::SendRpcResponse, conn, response);

    // 7. 在框架层面发起调用。实际会跳转至用户代码中重写的业务函数
    service->CallMethod(method, nullptr, request, response, done);
}

void RpcProvider::SendRpcResponse(const TcpConnectionPtr& conn, google::protobuf::Message* response)
{
    std::string response_str;
    // 序列化响应结果
    if (response->SerializeToString(&response_str))
    {
        // 投递到 TcpConnection 的发送缓冲区，触发 EPOLLOUT 事件发出
        conn->Send(response_str);
    }
    else
    {
        LOG_ERROR("Serialize response_str error!");
    }
}