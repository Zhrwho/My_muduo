/**
 * @brief 线程封装类
 * @date 2026.03
 */

 #pragma once


 #include "noncopyable.h"

 #include <memory>
 #include <thread>
 #include <functional>
 #include <string>
 #include <atomic>

 class Thread : public mymuduo::noncopyable
 {
 public:
    using ThreadFunc = std::function<void()>;

    /* 线程名字默认为空 */
    explicit Thread(ThreadFunc func, const std::string& name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string& name() { return name_; }

private:
    void setDefaultName();    

    bool started_;
    bool joined_;
    /* 用共享智能指针管理一个线程对象 */
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;
    static std::atomic_int numCreated_;
 };
 

 