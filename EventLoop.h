/**
 * @brief 事件循环类
 * 主要包含了两大模块 Channel Poller(epoll的抽象)
 */

#pragma once

#include "Timestamp.h"
#include "noncopyable.h"
#include "CurrentThread.h"
#include <functional>
#include <atomic>  //C++11的依赖
#include <memory>
#include <mutex>
#include <vector>

class Channel;
class Poller;

/**
 * @brief 事件循环
 */
class EventLoop : public mymuduo :: noncopyable
{
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();

    void loop(); //开启事件循环
    void quit(); //退出事件循环

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    void runInLoop(Functor cb);
	//把cb放入队列中，唤醒loop所在的线程，执行cb
    void queueInLoop(Functor cb);
	//唤醒loop所在的线程
    void wakeup();

	//Eventloop的方法 -》 Poller 的方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    /* 判断是否在当前loop的线程里面 */
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
    
private:

    void handleRead(); //处理wake up
    void doPendingFunctors(); //执行回调
    
    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;
    std::atomic_bool quit_; //标志退出loop循环

    const pid_t threadId_; //记录当前loop所在thread的ID

    Timestamp pollReturnTime_; //poller返回发生事件的channel的时间点
    std::unique_ptr<Poller> poller_; //动态管理资源

    int wakeupFd_; //当mainloop获取一个新用户的channel,通过轮询算法选择一个subloop,则通过该成员通知，唤醒subloop
    std::unique_ptr<Channel> wakeupChannel_; //封装了wakeupFd和感兴趣的事件，打包成了channel

    ChannelList activeChannels_; //eventloop管理的所有channel

    std::atomic_bool callingPendingFunctors_; //标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_; //返回值不带参的回调，存储loop需要执行的所有回调操作
    std::mutex mutex_; //互斥锁，用来保护上面vector容器的线程安全操作
};