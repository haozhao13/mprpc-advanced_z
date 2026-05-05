#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           
#include <assert.h>
#include <sys/stat.h>         
#include "block_queue.h"

using namespace std;

class Log
{
public:
    static Log *Instance()
    {
        static Log instance;
        return &instance;
    }

    static void *flush_log_thread(void *args)
    {
        Log::Instance()->async_write_log();
        return nullptr;
    }

    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    void write_log(int level, const char *format, ...);

    void flush(void);

    // 【新增】公开获取关闭状态的接口，给宏使用
    int get_close_log() { return m_close_log; }

private:
    Log();
    virtual ~Log();
    
    void *async_write_log()
    {
        string single_log;
        while (m_log_queue->pop(single_log))
        {
            lock_guard<mutex> locker(m_mutex);
            fputs(single_log.c_str(), m_fp);
        }
        return nullptr;
    }

private:
    char dir_name[128]; 
    char log_name[128]; 
    int m_split_lines;  
    int m_log_buf_size; 
    long long m_count;  
    int m_today;        
    FILE *m_fp;         
    char *m_buf;        
    BlockQueue<string> *m_log_queue; 
    bool m_is_async;    
    mutex m_mutex;
    int m_close_log; 
};

// 【修复】宏定义：现在通过 get_close_log() 获取状态，而不是直接访问私有变量
#define LOG_DEBUG(format, ...) if(0 == Log::Instance()->get_close_log()) {Log::Instance()->write_log(0, format, ##__VA_ARGS__); Log::Instance()->flush();}
#define LOG_INFO(format, ...) if(0 == Log::Instance()->get_close_log()) {Log::Instance()->write_log(1, format, ##__VA_ARGS__); Log::Instance()->flush();}
#define LOG_WARN(format, ...) if(0 == Log::Instance()->get_close_log()) {Log::Instance()->write_log(2, format, ##__VA_ARGS__); Log::Instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == Log::Instance()->get_close_log()) {Log::Instance()->write_log(3, format, ##__VA_ARGS__); Log::Instance()->flush();}

#endif