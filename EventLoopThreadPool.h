/**
 * @brief EventLoop线程池模式
 * @date 2026.03.26
 */

 #pragma once

 #include "EventLoop.h"
 #include <string>
 #include <functional>
 #include <vector>
 #include <memory>

 class EventLoop;
 class EventLoopThread;

 #include "noncopyable.h"

class EventLoopThreadPool : public mymuduo::noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>; 

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    // 如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }
    const std::string name() const { return name_; }


private:
    /* 用户创建的那个 Loop*/
    EventLoop *baseLoop_; // EventLoop loop;  
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    /* 所有创建的线程 */
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};


