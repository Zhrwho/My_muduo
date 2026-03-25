/**
 * @brief 多路事件分发器中的核心IO复用模块基类
 * @date 2026.03.25
 * 抽象类：包含纯虚函数的类，不能实例化
 */
#pragma once

#include "noncopyable.h"
#include "Channel.h"
#include "Timestamp.h"
#include <vector>
#include <unordered_map>


/**
 * @brief 多路IO复用基类
 */
class Poller : public mymuduo :: noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller() = default; /*虚析构函数*/

    /* 给所有IO复用保留统一的接口 */
    virtual void updateChannel(Channel* channel) = 0;
    /* 激活的channel, 相当于启动epollwait */
    virtual Timestamp poll(int timeoutMS, ChannelList* activeChannels) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    /* 判断一个poller里是否有某个channel*/
    bool hasChannel(Channel* channel) const;

    /* Eventloop 可以通过该接口获取默认的IO复用的具体实现 */
    /* 根据当前系统，创建一个合适的 Poller 实现（epoll / poll） */
    static Poller* newDefaultPoller(EventLoop* loop);

protected: /* 派生类可以访问 */
    /*保存所有已经注册到 Poller 的 Channel*/
    /* key: socckfd  value: sockfd所属的通道类型*/
    /* fd  ---> Channel 对象 */
    /* 在Epoll的updatechannel 里面存入了一一对应的关系 */
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private: /* 派生类无法访问 */
    EventLoop* ownerLoop_;
};