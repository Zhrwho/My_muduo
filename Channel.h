/**
 * @brief Channel类封装了socket(fd)及其感兴趣的event(loop)，如EPOLLIN、EPOLLOUT事件
 * @brief 还绑定了poller返回的具体事件
 * @date 2026.03.24
 * using EventCallback = std::function<void()>
 * EventCallback 是一种“函数类型”，“可以存任何 无参数、无返回值的函数”
 */
#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
#include <functional>
#include <memory>

class EventLoop;

/**
 * @brief 封装了socket及其感兴趣的event
 */
class Channel : public mymuduo::noncopyable
{
public:
    /*定义了可以存任何无参数、无返回值的函数的 EventCallback 函数类型*/
    /*事件回调*/
    using EventCallback = std::function<void()>;
    /*只读事件回调*/
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    /*fd得到poller通知以后，处理事件（调用相应的回调方法）*/
    void handleEvent(Timestamp reveiveTime);

    /*把cb的参数直接转过去，左值转给右值*/
    /* 设置读回调 */
    void setReadCallback(ReadEventCallback cb)  { readCallback_ = std::move(cb); }
    /* 设置写回调 */
    void setWriteCallback(EventCallback cb)     { writeCallback_ = std::move(cb); }
    /* 设置关闭回调 */
    void setCloseCallback(EventCallback cb)     { closeCallback_ = std::move(cb); }
    /* 设置错误回调 */
    void setErrorCallback(EventCallback cb)     { errorCallback_ = std::move(cb); }

    /*防止当channel被手动remove掉，channel还在执行回调操作*/
    void tie(const std::shared_ptr<void>&);

    /* 返回channel对应fd */
    int fd() const { return fd_; }
    /* 返回channel感兴趣的事件 */
    int events() const { return events_; }  
    /* 设置channel对应fd发生的事件, epoll监听后设置的*/
    void set_revents(int revt) { revents_ = revt; }
    bool isNoneEvent() const { return revents_ == KNoneEvent; }

    /* 让channel对读感兴趣 */
    void enableReading()  { events_ |=  KReadEvent ; update(); }
    /* 让channel对写感兴趣 */
    void enablewriting()  { events_ |=  KWriteEvent; update(); }
    /* 让channel对读不感兴趣 */
    void disableReading() { events_ &= ~KReadEvent ; update(); }
    /* 让channel对写不感兴趣 */
    void disableWriting() { events_ &= ~KWriteEvent; update(); }
    /* 让channel对所有事件不感兴趣 */
    void disableAll()     { events_  =  KNoneEvent ; update(); }

    /* 返回fd当前的事件状态 */
    /* 返回channel是否对所有事件不感兴趣 */
    bool isNoneEvent() const { return events_ == KNoneEvent; }
    /* 返回channel是否对写事件感兴趣 */
    bool isWriting() const { return events_ & KWriteEvent; }
    /* 返回channel是否对读事件感兴趣 */
    bool isReading() const { return events_ & KReadEvent; }

    /* 返回channel的index：加入、删除、修改 */
    int index() { return index_; }
    /* 设置channel的index，标记channel状态：加入、删除、修改 */
    void set_index (int index) { index_ = index; }

    /* one loop per thread */
    /* 返回channel对应的EventLoop */
    EventLoop* ownerLoop() { return loop_; }

    /*删除channel*/
    void remove();

private:
    /* 把fd感兴趣的事件添加到Epoll上*/
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    /* 表示对感兴趣事件信息的状态描述*/
    static const int KNoneEvent;
    static const int KReadEvent;  /*读事件*/
    static const int KWriteEvent; /*写事件*/

    EventLoop* loop_;  // 事件循环
    const int fd_;     // Poller监听对象，一个poller监听多个channel(fd)
    int events_;       // 注册fd感兴趣的事情
    int revents_;      // Poller返回的具体发生的事件
    int index_;

    /* 跨线程的生存状态的监听-tie*/
    /* 弱智能指针可以提升为强智能指针*/
    std::weak_ptr<void> tie_;
    bool tied_;

    /* 四个函数对象-具体事件回调，到时候可以绑定外部传进来的相关操作*/
    /* channel根据发生的具体事件，来调用相应的回调*/
    /* 因为channel通道里面能够获知fd最终发生的具体的事件revents，所以它负责调用具体事件的回调操作*/
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

};