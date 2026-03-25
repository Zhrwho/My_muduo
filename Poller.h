#pragma once

#include "noncopyable.h"
#include "Channel.h"

class Poller : public mymuduo :: noncopyable
{
public:

    /*纯虚函数？*/
    virtual void updateChannel(Channel* channel) = 0;

};