/**
 * @brief 对socket执行流的封装，似乎是监听fd的封装类
 * @date 2026.03.26
 */

#pragma once

#include "noncopyable.h"
#include "Channel.h"
#include "Socket.h"
#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : public mymuduo::noncopyable 
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    /* 设置新连接产生回调 */
    void setNewConnectionCallback(const NewConnectionCallback& cb) {
        newConnectionCallback_ = cb;
    }

    bool listenning() const { return listenning_; }
    void listen();

private:
    void handleRead();

    /* Acceptor用的就是用户定义的那个baseLoop，也称作mainLoop */
    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};