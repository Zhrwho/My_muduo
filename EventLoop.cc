/**
 * @brief 事件循环
 * @date 2026.03.26
 * eventfd 在 Linux 内核里就是一个 64 位无符号计数器，也就是 uint64_t，占 8 字节。
 */

#include "EventLoop.h"
#include "Poller.h"
#include "Logger.h"
#include "Channel.h"
#include <unistd.h>
#include <sys/eventfd.h>
#include <memory>
#include <mutex>

/* 防止一个线程创建多个EventLoop   thread_local 每个线程都有自己的一份 t_loopInThisThread*/
__thread EventLoop* t_loopInThisThread = nullptr;

/* 定义默认的Poller IO复用接口的超时时间 */
const int KPollTimeMs = 10000;

/**
 * @brief 创建一个事件fd
 * 创建wakeupfd，用来notify唤醒subReactor处理新来的channel
 */
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0)
    {
        LOG_FATAL("eventfd create error:%d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
    , callingPendingFunctors_(false)
{
    LOG_DEBUG("EventLoop create %p in thread %d \n", this, threadId_);
    if(t_loopInThisThread) /* 如果 当前线程已经有一个 EventLoop 了 */
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else{
        t_loopInThisThread = this;
    }

    /* 设置wakeupfd的事件类型以及发生事件后的回调操作 */
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    /* 每一个eventloop都将监听wakeupchannel的EPOLLIN读事件了 */
    wakeupChannel_->enableReading();

}
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

/**
 * @brief 开启监听，开启事件循环
 */
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while (!quit_)
    {
        activeChannels_.clear();
        /* 监听两类fd   一种是client的fd，一种wakeupfd, pollReturnTime_:本次 IO 复用（epoll/poll）返回的时间点*/
        pollReturnTime_ = poller_->poll(KPollTimeMs, &activeChannels_);
        for(Channel* channel : activeChannels_)
        {
            /* Poller监听哪些channel发生事件了，然后上报给EventLoop，通知channel处理相应的事件 */
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        /**
         * IO线程 mainLoop accept fd《=channel subloop
         * mainLoop 事先注册一个回调cb（需要subloop来执行）    wakeup subloop后，执行下面的方法，执行之前mainloop注册的cb操作
         */ 
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping.\n", this);
    looping_ = false;
} 


// 退出事件循环  1.loop在自己的线程中调用quit  2.在非loop的线程中，调用loop的quit
/**
 *              mainLoop
 * 
 *                                             no ==================== 生产者-消费者的线程安全的队列
 * 
 *  subLoop1     subLoop2     subLoop3
 */ 
void EventLoop::quit()
{
    quit_ = true;

    /* 如果是在其它线程中，调用的quit   在一个subloop(woker)中，调用了mainLoop(IO)的quit */
    /* 如果当前线程不是 EventLoop 所在线程，就把 EventLoop 唤醒 */
    if(!isInLoopThread()) wakeup();
}

/**
 * @brief 在EventLoop所属线程中执行cb
 * @param cb 要执行的回调
 */
void EventLoop::runInLoop(Functor cb)
{
    if(isInLoopThread())
    {
        /* 执行回调 */
        cb();
    }
    /* 不在当前loop线程，先存，唤醒后再执行 */
    else queueInLoop(cb);

}

/**
 * @brief 将cb加入到回调队列
 * @param cb 要加入的回调
 */
//把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    // 唤醒相应的，需要执行上面回调操作的loop的线程了
    // || callingPendingFunctors_的意思是：当前loop正在执行回调，但是loop又有了新的回调
    if(!isInLoopThread() || callingPendingFunctors_) wakeup();
    /* wakeup()的主要作用就是快速退出阻塞，让poller监听到事件，然后解除epoll_wait阻塞
    执行Eventloop中loop函数的 pollReturnTime_ = poller_->poll(KPollTimeMs, &activeChannels_);*/
}

/* EventLoop唤醒函数 */
/* 用来唤醒loop所在的线程的  向wakeupfd_写一个数据，wakeupChannel就发生读事件，当前loop线程就会被唤醒*/
void EventLoop::wakeup()
{
    uint64_t one = 1; /* 64位就是八字节 */
    /* 内核counter计数加1，eventfd可读状态， EPOLLIN触发，唤醒epoll_wait */
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if( n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

/* EventLoop被唤醒处理函数 */
void EventLoop::handleRead()
{
    /* read 把 eventfd 的计数清零（消费掉唤醒信号）*/
    uint64_t one = 1; /* one 是初始化无意义，把计数器的值覆盖到 one*/
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if(n != sizeof one) {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}



//Eventloop的方法 -》 Poller 的方法
void EventLoop::updateChannel(Channel* channel) { poller_->updateChannel(channel); }
void EventLoop::removeChannel(Channel* channel) { poller_->removeChannel(channel); }
bool EventLoop::hasChannel(Channel* channel) { return poller_ -> hasChannel(channel); }

/**
 * @brief 执行在其他线程加入的回调函数
 * 回调都在queueInLoop中加入到了pendingvectors中
 */
void EventLoop::doPendingFunctors()
{
    /* 在loop中dopengding? */
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
		//functors.swap(pendingFunctors_) 会 交换两个 vector 的内容。
        functors.swap(pendingFunctors_);        
    }

    for(const Functor& functor : functors)
    {
        functor(); /* 执行回调 */
    }
    callingPendingFunctors_ = false;
}