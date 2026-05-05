#pragma once

#include <unordered_map>
#include <string>

// 框架读取配置文件类
class MprpcConfig
{
public:
    // 解析加载配置文件
    void LoadConfigFile(const char *config_file);
    // 查询配置项信息
    std::string Load(const std::string &key);

private:
    // 存储解析出的键值对
    std::unordered_map<std::string, std::string> m_configMap;
    // 去除字符串前后的空格
    void Trim(std::string &src_buf);
};