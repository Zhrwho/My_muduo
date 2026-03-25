/**
 * @brief 使用Epoll构建的Poller
 * @date 2026.03.25
 * override 是 C++11 关键字，用于显式声明函数重写父类虚函数，编译器会检查函数签名是否匹配，从而避免因拼写错误或参数不一致导致的多态失效问题。
 * epoll_create
 * epoll_ctl   add/modify/delete
 * epoll_wait
 */
#pragma once

#include "Poller.h"
#include <sys/epoll.h>
#include <vector>

class Channel;


/**
 * @brief epoll多路复用Poller
 * epoll_create
 * epoll_ctl   add/modify/delete
 * epoll_wait
 */
class EpollPoller : public Poller
{
public:
    EpollPoller(EventLoop* loop);
    ~EpollPoller() override;

    /*override:重写函数标记作用*/
    void updateChannel(Channel* channel) override;
    /* 激活的channel, 相当于启动epollwait */
    Timestamp poll(int timeoutMS, ChannelList* activeChannels) override;
    void removeChannel(Channel* channel) override;

private:
    /* 给epoll_event 初始化的长度*/
    static const int KInitEventListSize = 16;

    /* 填写活跃的channel */
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    /* 更新活跃的通道，相当于epoll_ctl,更新fd 事件*/
    void update(int operation, Channel* channel);

    /* epoll_event 是底层的Epoll */
    using EventList = std::vector<epoll_event>;

    int epollfd_;
    /* events_:epoll_wait 返回的“已就绪事件集合”，当前这一轮 epoll_wait 返回的“活跃事件”列表 */
    EventList events_;
};


