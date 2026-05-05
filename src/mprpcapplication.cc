#include "mprpcapplication.h"
#include <iostream>
#include <unistd.h>
#include <string>
#include "log.h" // 引入日志头文件

MprpcConfig MprpcApplication::m_config;

void MprpcApplication::ShowArgsHelp()
{
    std::cout << "format: command -i <configfile>" << std::endl;
}

void MprpcApplication::Init(int argc, char **argv)
{
    if (argc < 2)
    {
        ShowArgsHelp();
        exit(EXIT_FAILURE);
    }

    int c = 0;
    std::string config_file;
    // 使用 getopt 解析命令行参数 -i
    while ((c = getopt(argc, argv, "i:")) != -1)
    {
        switch (c)
        {
        case 'i':
            config_file = optarg;
            break;
        case '?':
        case ':':
            ShowArgsHelp();
            exit(EXIT_FAILURE);
        default:
            break;
        }
    }

    // 开始加载配置文件
    m_config.LoadConfigFile(config_file.c_str());
    // 全局初始化日志模块，供服务端和客户端通用
    Log::Instance()->init("./log/RpcServerLog", 0, 2000, 800000, 800);
}

MprpcApplication& MprpcApplication::GetInstance()
{
    static MprpcApplication app;
    return app;
}

MprpcConfig& MprpcApplication::GetConfig()
{
    return m_config;
}