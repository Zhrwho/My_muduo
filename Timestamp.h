/**
 * @brief 时间戳类
 * @date 2026.03.24
 * 知识点：explicit-禁止隐式类型转换，一般用于单参数的构造函数，在函数定义时不加
 */

#pragma once

#include <string>
#include <iostream>

class Timestamp
{
public:
    /*默认构造和带参数的构造*/
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    /*获取当前时间*/
    /*说明这个函数返回一个 Timestamp 对象（通常表示“时间戳”）*/
    static Timestamp now();
    /*获取年月日时分秒的输出*/
    std::string toString() const; /*只读方法*/

private:
    /*一个表示时间的长整型变量*/
    int64_t microSecondsSinceEpoch_;
};

/*Timestame::now().tostring()*/