/**
 * @brief 事件循环
 * @date 2026.03.25
 */
#include "Channel.h"
#include "Logger.h"

#include <sys/epoll.h>


const int Channel::KNoneEvent = 0;
const int Channel::KReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::KWriteEvent = EPOLLOUT;

/*构造函数：对成员变量的初始化*/
Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      tied_(false)
    { }

Channel::~Channel() { }

/**
 * @brief 设置tie指针
 * @param obj 对应的EventLoop智能指针
 */
/*防止当channel被手动remove掉，channel还在执行回调操作*/
/*在事件回调时确保这个对象还活着，否则就不执行回调，避免野指针问题*/
/*Channel::tie 用于绑定 TcpConnection 的 shared_ptr，通过 weak_ptr 持有。在事件触发时通过 lock 判断对象是否仍然存在，从而避免 TcpConnection 已析构但 Channel 仍然执行回调导致的野指针问题。*/
void Channel::tie(const std::shared_ptr<void>&  obj)
{
    tie_ = obj; /*相当于把强智能指针变为了弱智能指针*/
    tied_ = true;
}

/**
 * @brief 从channel所属的EventLoop中删除
 */
void Channel::remove()
{
    loop_->removeChannel(this);
}

/**
 * @brief 更新channel状态：加入、删除、修改
 * 当改变channel所表示fd的event事件后，需要update负责在poller里面更改fd相应的事件
 */
void Channel::update()
{
    /* 通过channel所属的Eventloop调用poller的相应方法，注册fd的eventloop事件 */
    loop_->updateChannel(this); // 不完整的类型
    /*对应的：void EventLoop::updateChannel(Channel* channel) { poller_->updateChannel(channel); }*/
}


/*fd得到poller通知以后，处理事件（调用相应的回调方法）*/
/**
 * @brief 处理发生的事件
 * @param receiveTime 发生时间
 */
void Channel::handleEvent(Timestamp reveiveTime)
{
    /*因为前面有tie函数，这里需要判断指针是否还存在，避免出现野指针情况*/
    if(tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();/*弱转强*/
        if(guard) handleEventWithGuard(reveiveTime);
        /*如果引用计数大于0，才执行，否则不会执行*/
    }
    else handleEventWithGuard(reveiveTime);
}

/**
 * @brief 根据发生事件类型调用对应回调
 * @param receiveTime 发生时间
 */
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handlerEvent revents:%d\n", revents_);
    /*连接异常判断， EPOLLHUP对端关闭连接了 EPOLLIN有数据可读 EPOLLPRI有“带外数据（out-of-band data）”可读    EPOLLOUT：这个 fd 可以“写数据”了（*/
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
           closeCallback_();
        }
        if(revents_ & EPOLLERR)
            if(errorCallback_) errorCallback_();

        if(revents_ & (EPOLLIN | EPOLLPRI))
            if(readCallback_) readCallback_(receiveTime);

        if(revents_ & EPOLLOUT)
            if(writeCallback_) writeCallback_();
        }

}
