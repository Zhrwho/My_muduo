/**
 * @brief 线程封装类
 * @date 2026.03
 */

 #include "Thread.h"
 #include "CurrentThread.h"

 #include <semaphore.h>

Thread::Thread(ThreadFunc func, const std::string& name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    /* 线程分离 */
    if(started_ && !joined_) {
        thread_->detach();
    }
}

/**
 * @brief 线程启动，启动完成后退出
 * 一个Thread对象，记录的就是一个新线程的详细信息
 */
void Thread::start()
{
    started_ = true;
    sem_t sem;
    /* 一开始所有 sem_wait() 都设置为阻塞 */
    sem_init(&sem, false, 0);

    /* 开启线程 */ /* [&]() { ... } */
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        // 获取线程的tid值
        tid_ = CurrentThread::tid();
        /* 开启一个新线程，专门执行该线程函数*/
        sem_post(&sem);

        /* func_() 就是执行 threadFunc,会创建一个独立的eventloop */ 
        func_(); 
        /* Thread::Thread(ThreadFunc func, const std::string& name)
            : func_(func),
            name_(name)
        {}*/
        /* threadFunc?*/
        /* thread_(std::bind(&EventLoopThread::threadFunc, this), name) */ 
    }));
    /* 这里必须等待获取上面新创建的线程的tid值  */
    sem_wait(&sem);
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    /* 全局静态计数器，每创建一个thread，就+1 */
    int num = numCreated_;
    if(name_.empty())
    {
        /* 给线程起名字 */
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}