#pragma once

#include "mprpcconfig.h"

// mprpc 框架的基础类，负责框架的一些初始化操作
class MprpcApplication
{
public:
    // 初始化框架，解析命令行启动参数
    static void Init(int argc, char **argv);
    // 获取单例实例
    static MprpcApplication& GetInstance();
    // 获取全局配置对象
    static MprpcConfig& GetConfig();

private:
    static MprpcConfig m_config;

    // 单例模式：构造函数私有化，删除拷贝构造和移动构造
    MprpcApplication() = default;
    MprpcApplication(const MprpcApplication&) = delete;
    MprpcApplication(MprpcApplication&&) = delete;

    static void ShowArgsHelp();
};