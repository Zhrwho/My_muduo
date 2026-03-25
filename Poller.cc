/**
 * @brief 多路事件分发器中的核心IO复用模块基类
 * @date 2026.03.25
 */
#include "Poller.h"

Poller::Poller(EventLoop* loop)
    : ownerLoop_(loop) { }

/**
 * @brief 查找是否存在某个channel
 * @param channel 需要查找的channel
 */

 /* 就是去那个map里面找，channelMap就是存储了所有的channel，并用fd对应查找*/
bool Poller::hasChannel(Channel* channel) const {
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}