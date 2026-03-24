#include "Timestamp.h"

#include <time.h>

/*默认构造啥也没干，初始化为0*/
Timestamp::Timestamp()
: microSecondsSinceEpoch_(0) { }

Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    /*给定参数的把带参数的初始化*/
    : microSecondsSinceEpoch_(microSecondsSinceEpoch)
    { }

/*获取当前时间，本质上是now函数，只是返回的是Timestamp类型的参数*/
/*说明这个函数返回一个 Timestamp 对象（通常表示“时间戳”）*/
Timestamp Timestamp::now()
{
    /*调用系统函数，获取当前时间*/
    return Timestamp(time(NULL));
}

/*获取年月日时分秒的输出*/
std::string Timestamp::toString() const {
    /*用的那个tm结构体*/
    char buf[128] = { 0 };
    /*返回指针类型节省空间*/
    tm* tm_time = localtime(&microSecondsSinceEpoch_);
    snprintf(buf, 128, "%4d-%02d-%02d-%02d:%02d:%02d",
                        tm_time->tm_year + 1900,
                        tm_time->tm_mon + 1,
                        tm_time->tm_mday,
                        tm_time->tm_hour,
                        tm_time->tm_min,
                        tm_time->tm_sec);
    return buf;
}

/*二者连用才是输出当前时间的年月日时分秒表示*/