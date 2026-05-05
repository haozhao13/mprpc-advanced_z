#include <iostream>
#include <string>
#include <unistd.h>
#include <future>
#include <functional>
#include "mprpcapplication.h"
#include "user.pb.h"
#include "mprpcchannel.h"
#include "mprpccontroller.h"

// 适配 C++11 lambda 的 Closure 回调类
class MprpcClosure : public google::protobuf::Closure {
public:
    MprpcClosure(std::function<void()> cb) : m_cb(cb) {}
    void Run() override { m_cb(); }
private:
    std::function<void()> m_cb;
};

int main(int argc, char **argv)
{
    MprpcApplication::Init(argc, argv);
    /* Stub负责“假装”在本地调用（服务端的“代理”）
    实际上，stub会通过channel调用CallMethod()方法，即多态式地找到真正需要的那个rpc服务 */ 
    fixbug::UserServiceRpc_Stub stub(new MprpcChannel());
    
    fixbug::LoginRequest request;
    request.set_name("paris_async");
    request.set_pwd("123456");
    
    fixbug::LoginResponse response;
    MprpcController controller;

    // 1. 定义 promise 并在主线程获取对应的 future 对象
    std::promise<void> promise;
    std::future<void> future = promise.get_future();

    // 2. 构造回调：当底层网络线程执行完毕时，触发此 lambda，将 promise 状态置为就绪
    MprpcClosure closure([&promise]() {
        promise.set_value();
    });

    std::cout << "发起异步 RPC 调用，当前主线程不阻塞..." << std::endl;
    
    // 3. 传入 closure，此时 CallMethod 会立即返回
    stub.Login(&controller, &request, &response, &closure);

    // 模拟主线程在等待网络响应的同时，并行处理本地的其他耗时任务
    std::cout << "主线程正在并行执行本地其他任务..." << std::endl;
    sleep(2); 

    // 4. 最终需要 RPC 结果时，阻塞等待底层线程的回调唤醒
    future.wait();
    
    std::cout << "===========================" << std::endl;
    std::cout << "获取到异步 RPC 结果：" << std::endl;
    if (controller.Failed())
    {
        std::cout << "failed! reason: " << controller.ErrorText() << std::endl;
    }
    else
    {
        if (0 == response.result().errcode())
        {
            std::cout << "success:" << response.success() << std::endl;
        }
        else
        {
            std::cout << "error:" << response.result().errmsg() << std::endl;
        }
    }

    return 0;
}


