#include "EventLoop.h"
#include "Poller.h"

void EventLoop::updateChannel(Channel* channel) { poller_->updateChannel(channel); }

