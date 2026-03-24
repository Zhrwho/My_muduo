/**
 * @brief 简单日志类
 * @date 2026.03.24
 * 知识点：单例模式，唯一实例（instance) 、 宏定义
 */
//宏里写注释一定要写完整 /* 注释 */，否则宏展开会语法错误

#pragma once

#include "noncopyable.h"
#include "string"
#include <iostream>

/*定义四种宏，让用户直接调用，不需要定义单例等，直接调用LOG_INFO这些*/
#define LOG_INFO(logmsgFormat, ...) \
    do \
    {  \
        /*定义一个logger来接收单例对象*/ \
        Logger& logger = Logger::Instance(); \
        logger.setLogLevel(INFO); \
        char buf[1024] = { 0 }; \
        /*把格式化后的字符串写进缓冲区 buf 里，获取可变参列表*/ \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        /*写日志*/ \
        logger.log(buf); \
    }while (0)

#define LOG_ERROR(logmsgFormat, ...) do{                    \
        Logger& logger = Logger::Instance();                \
        logger.setLogLevel(ERROR);                          \
        char buf[1024] = { 0 };                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);   \
        logger.log(buf);                                    \
    }while(0)

#define LOG_FATAL(logmsgFormat, ...) do{                    \
        Logger& logger = Logger::Instance();                \
        logger.setLogLevel(FATAL);                          \
        char buf[1024] = { 0 };                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);   \
        logger.log(buf);                                    \
        exit(-1);                                           \
    }while(0)

/*debug信息比较多，所以默认设置关闭，定义了MUDEBUG这个宏才输出*/
#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...) do{                    \
        Logger& logger = Logger::Instance();                \
        logger.setLogLevel(DEBUG);                          \
        char buf[1024] = { 0 };                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);   \
        logger.log(buf);                                    \
    }while(0)
#else /*否则啥也不输出*/
#define LOG_DEBUG(logmsgFormat, ...)
#endif

/*定义日志级别，INFO, ERROR, FATAL, DEBUG*/
enum LogLevel {
    INFO,   // 普通信息
    ERROR,  // 错误信息
    FATAL,  // core信息
    DEBUG,  // 调试信息   
};

class Logger : public mymuduo::noncopyable
{
public:
    /*获取日志唯一的实例对象，提供一个全局唯一的 Logger 对象的访问入口*/
    static Logger& Instance();
    /*设置日志级别*/
    void setLogLevel(int level);
    /*写日志*/
    void log(std::string msg);

private:
    int logLevel_;/*枚举就是整数*/
    Logger(){}    /*构造函数私有*/

};

