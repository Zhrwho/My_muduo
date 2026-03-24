/**
 * @brief 简单日志类
 * @date 2026.03.24
 */

#include "Logger.h"
#include "Timestamp.h"

#include <iostream>

Logger& Logger::Instance()
{
    static Logger logger;
    return logger;
}
/*设置日志级别*/
void Logger::setLogLevel(int level)
{
    logLevel_ = level;
}

/**
 * @brief 日志打印函数
 * @param msg 输出的信息
 * [级别信息] time : msg
 */
void Logger::log(std::string msg)
{
    switch (logLevel_) {
        case INFO: std::cout << "[INFO]"; break;
        case ERROR: std::cout << "[ERROR]";break;
        case FATAL: std::cout << "[FATAL]";break;
        case DEBUG: std::cout << "[DEBUG]";break; 
        default: break;      
    }
   std::cout << Timestamp::now().toString() << " : " << msg << std::endl;
}