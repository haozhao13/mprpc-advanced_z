#include <iostream>
#include <string>
#include "user.pb.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"
#include <thread>

// 继承自 Protobuf 生成的 RPC 服务基类
class UserService : public fixbug::UserServiceRpc 
{
public:
    // 1. 本地真实的业务逻辑
    bool Login(std::string name, std::string pwd) 
    {
        std::cout << "Doing local service: Login" << std::endl;

        // 模拟真实 RPC 调用耗时，让当前工作线程稍微挂起
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::cout << "name: " << name << " pwd: " << pwd << std::endl;
        return true;
    }

    // 2. 重写基类的虚函数，此函数由 RpcProvider 在收到网络请求时动态反射调用
    void Login(::google::protobuf::RpcController* controller,
               const ::fixbug::LoginRequest* request,
               ::fixbug::LoginResponse* response,
               ::google::protobuf::Closure* done) override 
    {
        // 框架已将网络字节流反序列化为 request，直接读取参数
        std::string name = request->name();
        std::string pwd = request->pwd();

        // 执行本地业务
        bool local_result = Login(name, pwd);

        // 将业务执行结果写入 response
        fixbug::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_success(local_result);

        // 执行回调函数 (由框架绑定，负责将 response 序列化后通过 Epoll 发送回客户端)
        done->Run();
    }
};

int main(int argc, char **argv) 
{
    // 初始化 RPC 框架（解析命令行参数并加载配置文件）
    MprpcApplication::Init(argc, argv);

    // 把 UserService 对象发布到 RPC 节点上
    RpcProvider provider;
    provider.NotifyService(new UserService());

    // 启动一个 RPC 服务发布节点，阻塞等待客户端连接
    provider.Run();

    return 0;
}