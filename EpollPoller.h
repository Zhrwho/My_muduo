/**
 * override 是 C++11 关键字，用于显式声明函数重写父类虚函数，编译器会检查函数签名是否匹配，从而避免因拼写错误或参数不一致导致的多态失效问题。
 */
#pragma once

#include "Poller.h"

class Channel;

class EpollPoller : public Poller
{
public:
    EpollPoller(/* args */);
    ~EpollPoller();

    /*override:重写函数标记作用*/
    void updateChannel(Channel* channel) override;


};


