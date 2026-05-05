#include "mprpcconfig.h"
#include <iostream>
#include <string>

void MprpcConfig::LoadConfigFile(const char *config_file)
{
    FILE *pf = fopen(config_file, "r");
    if (nullptr == pf)
    {
        std::cout << config_file << " is not exist!" << std::endl;
        exit(EXIT_FAILURE);
    }

    while(!feof(pf))
    {
        char buf[512] = {0};
        fgets(buf, 512, pf);

        std::string src_buf(buf);
        Trim(src_buf);

        // 忽略注释或空行
        if (src_buf[0] == '#' || src_buf.empty())
        {
            continue;
        }

        // 解析配置项
        int idx = src_buf.find('=');
        if (idx == -1)
        {
            continue; // 配置不合法
        }

        std::string key;
        std::string value;
        key = src_buf.substr(0, idx);
        Trim(key);
        
        // 处理末尾换行符
        int endidx = src_buf.find('\n', idx);
        value = src_buf.substr(idx+1, endidx-idx-1);
        Trim(value);
        
        m_configMap.insert({key, value});
    }

    fclose(pf);
}

std::string MprpcConfig::Load(const std::string &key)
{
    auto it = m_configMap.find(key);
    if (it == m_configMap.end())
    {
        return "";
    }
    return it->second;
}

void MprpcConfig::Trim(std::string &src_buf)
{
    // 去掉字符串前面的空格
    int idx = src_buf.find_first_not_of(' ');
    if (idx != -1)
    {
        src_buf = src_buf.substr(idx, src_buf.size()-idx);
    }
    // 去掉字符串后面多余的空格
    idx = src_buf.find_last_not_of(' ');
    if (idx != -1)
    {
        src_buf = src_buf.substr(0, idx+1);
    }
}