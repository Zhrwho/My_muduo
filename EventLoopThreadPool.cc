/**
 * @brief EventLoop线程池模式
 * @date 2026.03.26
 * 创建对象流程:
① 分配内存（栈）
② 按声明顺序初始化成员（走初始化列表）
③ 执行构造函数函数体
④ 对象创建完成
 */

 #include "EventLoopThreadPool.h"
 #include "EventLoopThread.h"
 #include <memory>

 EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0)
{}

EventLoopThreadPool::~EventLoopThreadPool()
{}

/**
 * @brief:创建新loop和thread,在EventLoopThread中start的
 */
void EventLoopThreadPool::start(const ThreadInitCallback &cb)
/* using ThreadInitCallback = std::function<void(EventLoop*)>; */
{
    started_ = true;

    for (int i = 0; i < numThreads_; ++i)
    {
        /* 底层线程命名 */
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        /* EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, 
        const std::string &name) */
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop()); // 底层创建线程，绑定一个新的EventLoop，并返回该loop的地址
        /* 创建subloop */
    }

    // 整个服务端只有一个线程，运行着baseloop
    if (numThreads_ == 0 && cb)
    {
        cb(baseLoop_);
        /*    using ThreadInitCallback = std::function<void(EventLoop*)>; ,执行cb*/
    }
}

/**
 * @brief:工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop
 */
EventLoop* EventLoopThreadPool::getNextLoop()
{
    /* io 线程: 只做新用户的连接事件 */
    EventLoop *loop = baseLoop_;  /* 指针 先指向mainloop,用户创建的那个*/

    /* worker 线程: 处理已连接用户的读写事件 */
    if (!loops_.empty()) // 通过轮询获取下一个处理事件的loop
    {
        loop = loops_[next_];
        ++next_;
        if (next_ >= loops_.size())
        {
            next_ = 0;
        }
    }

    return loop;
}

/**
 * @brief:返回所有的loop
 */
std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty())
    {
        /* 返回vector形式 */
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    else
    {
        return loops_;
    }
}