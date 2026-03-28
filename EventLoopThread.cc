#include "EventLoopThread.h"
#include "EventLoop.h"


EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, 
        const std::string &name)
        : loop_(nullptr)
        , exiting_(false)
        // using ThreadFunc = std::function<void()>;
        , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
        /* 初始化列表里面直接构造: Thread::Thread(ThreadFunc func, const std::string& name) */
        //std::vector<int> vec(10);
        /* 所以就是把threadFunc传入到了func_()*/
        , mutex_()
        , cond_()
        , callback_(cb)
{   
    //std::vector<int> vec(10);
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

/**
 * @brief 开启循环，等待线程loop创建后退出，返回loop
 * @return 返回线程对应的Loop
 */
EventLoop* EventLoopThread::startLoop()
{
    /* start里的func_() 就是执行下面的 threadFunc,会创建一个独立的eventloop */
    thread_.start(); // 启动底层的 新 线程 ,执行下面的threadFunc


    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while ( loop_ == nullptr )
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

// 下面这个方法，是在单独的新线程里面运行的
/**
 * @brief 线程执行体，会实例化一个eventloop
 */
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建一个独立的eventloop，和上面的线程是一一对应的，one loop per thread
    /* 在当前线程下创建的loop, 创建的时候就会调用构造函数, 进行构造, 将当前线程tid记录*/
    
    if (callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    /* loop开始监听 */
    loop.loop(); // EventLoop loop  => Poller.poll
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}