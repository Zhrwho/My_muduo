/**
 * @brief 禁止拷贝、赋值操作的基类
 * @date 2026.03.24
 * @brief 禁止拷贝和赋值是为了防止多个对象共享独占资源，避免潜在的崩溃或资源冲突
 */

 #pragma once

namespace mymuduo {

class noncopyable{
public:
    noncopyable(const noncopyable&) = delete; /*拷贝构造*/
    noncopyable& operator=(const noncopyable&) = delete; /*赋值*/

protected:
    /*默认构造和析构*/
    noncopyable() = default;
    ~noncopyable() = default;

};
}