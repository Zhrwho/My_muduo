/**
 * @brief 使用Epoll构建的Poller
 * @date 2026.03.25
 * 一个 epollfd_：可以管理 成百上千个 fd，是 高并发的核心，而一个 fd 只能属于某一个 epoll 实实例
 * Thread 1  → EventLoop  → EpollPoller → epollfd_1
 */


#include "EpollPoller.h"
#include "Logger.h"
#include <unistd.h>
#include <strings.h>

/* channel的成员index_初始化也是-1 */
const int kNew = -1;    /* 一个channel还没有添加到poller中 */
const int kAdded = 1;   /* 一个channel已经被添加到poller中 */
const int kDeleted = 2; /* 一个channel已经被删除掉 */


/**
 * @brief 构造函数，会创建一个epoll
 * @param loop 对应的Eventloop
 */
EpollPoller::EpollPoller(EventLoop* loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(KInitEventListSize)
    {
        if(epollfd_ < 0)
        {
            LOG_INFO("epoll_create error:%d \n", errno);
        }
    }

/**
 * @brief 析构函数，关闭epoll
 */
EpollPoller:: ~EpollPoller()
{
    ::close(epollfd_);
}

/**
 * @brief epoll进行监听
 * @param timeoutMs 监听超时时间
 * @param activeChannels 监听到的有事件的channel，传出参数
 */
Timestamp EpollPoller::poll(int timeoutMS, ChannelList* activeChannels)
{
    LOG_DEBUG("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

    /* 用vector方便扩容，取底层数组的起始地址：&*events_.begin()*/
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMS);
    int saveError = errno; /* errno是全局的，所以这里先用个局部变量存储*/
    /* 定义一个变量 now，类型是 Timestamp，用 Timestamp::now() 的返回值来初始化它 */
    Timestamp now(Timestamp::now());

    if(numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        /* 把 epoll_wait 返回的“就绪事件”，转换成 muduo 的 Channel 列表 */
        fillActiveChannels(numEvents, activeChannels);
        if( numEvents == events_.size())
        {
            /* 二倍扩容 */
            events_.resize(events_.size() * 2);
        }
    }
    else if(numEvents == 0) 
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else {
        if(saveError != EINTR) {
            errno = saveError;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}


/**
 * @brief 更新channel状态：加入、删除、修改感兴趣事件
 * @param channel 要更新的channel
 * channel的update--调用eventloop的update，然后调用下面这个（实际就是poller执行）
 */
void EpollPoller::updateChannel(Channel* channel)
{   
    /* 对应的就是EPOLL 的ADD, DEL, MOD */

    /* index表示的就是channel在poller中的状态*/
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);
    if( index == kNew || index ==  kDeleted)
    /* kdeleted 状态表示原来监听过，现在是从epoll中删除了，但还在channels中，
    所以不需要加入到map里面，现在需要重新监听，所以需要重新使用add加入到epoll中*/
    {
        if(index == kNew)
        {
            int fd = channel->fd();
            /* 添加到channelMap里面 */
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else /* channel已经在poller中注册过了 */
    {
        int fd = channel->fd();/* 貌似无用 */
        /* 如果channel对任何事件都不感兴趣了，不需要poller监听了*/
        if(channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel); /* 只从epoll内核监听表中删除 */
            channel->set_index(kDeleted);
        }
        else update(EPOLL_CTL_MOD, channel);
    }
}

/**
 * @brief 从poller监听表中删除channel
 * @param channel 要删除的channel
 */
void EpollPoller::removeChannel(Channel* channel)
{
    int fd = channel->fd();
    channels_.erase(fd); /* 从poller的map表中删除 */

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channel -> index();
    if(index == kAdded) /* 只有 kAdded 才需要删，因为 kDeleted 本来就不在 epoll 里  */
    {
        update(EPOLL_CTL_DEL, channel); /* 从 epoll 内核监听表中删除 */
    }
    channel->set_index(kNew);

}

/**
 * @brief 设置有事件发生的channel的事件类型
 * @param numEvents 发生事件的channel的数量
 * @param activeChannels 存储发生事件的channel，传出参数
 * 把 epoll 返回的就绪事件（events_），转换成 Channel 列表 activeChannels
 */
void EpollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
    for(int i = 0; i<numEvents; i++)
    {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

/**
 * @brief 在epoll中对channel进行加入、删除、修改感兴趣事件
 * @param operation 对应操作：加入、删除、修改
 * @param channel 对应channel
 */
void EpollPoller::update(int operation, Channel* channel)
{
    epoll_event event;
    bzero(&event, sizeof event);

    int fd = channel->fd();

    event.events = channel->events();  //fd所感兴趣的事件
    event.data.fd = fd; //联合体？
    event.data.ptr = channel;


    /* epollfd_ 决定了“你在操作哪一个事件集合, 相当于一个epoll实例
    epollfd_可管理很多个fd，一个fd智能属于某一个epoll实例
    int epollfd_ = epoll_create1(EPOLL_CLOEXEC);*/
    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if(operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else 
        {
            LOG_FATAL("epoll_ctl add/mod errror:%d\n", errno);
        }           
    }
}