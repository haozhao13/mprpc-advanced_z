#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "mprpcapplication.h"
#include "user.pb.h"
#include "mprpcchannel.h"
#include "mprpccontroller.h"
#include <functional>

#include "ThreadPool.h" // 引入线程池


// --- 此处填补 MprpcClosure 的定义 ---
class MprpcClosure : public google::protobuf::Closure {
public:
    MprpcClosure(std::function<void()> cb) : m_cb(cb) {}
    void Run() override { m_cb(); } // 压测时由主逻辑统一控制生命周期
private:
    std::function<void()> m_cb;
};

int main(int argc, char **argv) {
    MprpcApplication::Init(argc, argv); // 初始化配置与日志

    // 初始化一个拥有 100 个工作线程的线程池
    ThreadPool pool(100);
    
    // 应在外部创建一个 Stub 实例，其内部共享一个 MprpcChannel
    fixbug::UserServiceRpc_Stub stub(new MprpcChannel());
    
    int total_requests = 1500;
    std::atomic<int> success_count{0};
    std::atomic<int> completed_count{0};
    std::mutex mtx;
    std::condition_variable cv;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < total_requests; ++i) {
        // 将整个发起请求的过程打包成一个任务投递给线程池_【lambda表达式】
        pool.enqueue([i, &stub, &success_count, &completed_count, &cv, total_requests]() {
            // 1. 为每次异步调用准备独立的上下文对象
            auto* request = new fixbug::LoginRequest();
            auto* response = new fixbug::LoginResponse();
            auto* controller = new MprpcController();

            // 重要：设置变化的用户名，触发 ConsistentHash 的分流
            request->set_name("user_" + std::to_string(i)); 
            request->set_pwd("123456");

            // 2. 构造回调逻辑_【lambda表达式】
            MprpcClosure* closure = new MprpcClosure([request, response, controller, &success_count, &completed_count, &cv, total_requests]() {
                if (!controller->Failed() && response->result().errcode() == 0) {
                    success_count++;
                }
                // 记录已完成的请求数
                int current_completed = ++completed_count;
                // 资源清理
                delete request;
                delete response;
                delete controller;

                // 检查是否全部压测任务结束
                if (current_completed == total_requests) {
                    cv.notify_one();
                }
            });

            // 此时调用 Login，它会在线程池的 Worker 线程中执行
            // 3. 发起异步 RPC 调用
            // 由于 done 不为空，CallMethod 内部会启动新线程执行任务
            stub.Login(controller, request, response, closure);
        });
    }

    /* 剩下的逻辑与线程池无关了 */
    // 4. 等待所有并发请求返回
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&]() { return completed_count == total_requests; });

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // 5. 打印压测数据结果
    std::cout << "========== 压力测试结果 ==========" << std::endl;
    std::cout << "并发请求总数: " << total_requests << std::endl;
    std::cout << "成功响应总数: " << success_count.load() << std::endl;
    std::cout << "总计耗时: " << duration.count() << " ms" << std::endl;
    std::cout << "吞吐率 (QPS): " << (total_requests * 1000.0 / duration.count()) << std::endl;
    std::cout << "==================================" << std::endl;

    return 0;
}