#pragma once

#include "noncopyable.h"

#include <memory>

class Channel;
class Poller;

class EventLoop : public mymuduo :: noncopyable
{
public:
    EventLoop(/* args */);
    ~EventLoop();

    void updateChannel(Channel* channel);
    void removeChannel(Channel* Channel);

private:


    std::unique_ptr<Poller> poller_;

};

