#pragma once

/**
 * @brief:用户使用muduo编写服务器程序的接口类
 * 对外的服务器编程使用的类
 */
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "Callbacks.h"
#include "EventLoopThreadPool.h"

#include <string>
#include <unordered_map>
#include <atomic>
#include <functional>
#include <memory>

class TcpServer : noncopyable
{
public:

    using ThreadInitCallback = std::function<void(EventLoop*)>; /*定义线程初始化回调*/

    enum Option
    {
        /*预置两个选项，表示是否对端口可重用*/
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop* loop
             , const InetAddress& listenAddr
             , const std::string& nameArg
            , Option option = kNoReusePort); /*默认不重用端口*/
    ~TcpServer();

    /*对外实现的端口*/
    const std::string& ipPort() const { return ipPort_; }
    const std::string& name() const { return name_; }
    EventLoop* getloop() const { return loop_; }



    void setThreadInitCallback(const ThreadInitCallback& cb) /*线程初始化回调*/{ threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

    void setThreadNum(int numThreads); /*设置底层subloop的个数，触发thread,eventloopthread,eventloopthreadpool*/

    std::shared_ptr<EventLoopThreadPool> threadpool() /*返回底层的pool*/
    { return threadPool_; }

    void start(); /*开启服务器监听*/

private:
    /*跟connection相关*/
    void newConnection(int sockfd, const InetAddress& peerAdr); /*新连接来*/
    void removeConnection(const TcpConnectionPtr& coon); /*断开这条连接，从connectionmap中移除*/
    void removeConnectionInLoop(const TcpConnectionPtr& coon);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop* loop_;          /*用户定义的那个baseloop（mainloop）*/

    const std::string ipPort_; /*ip和port一起打包，保存服务器相关的IP端口号名称*/
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;               /*运行在mainLoop，监听新连接事件*/
    std::shared_ptr<EventLoopThreadPool> threadPool_;  /*one loop per thread*/

    /*用户的回调*/
    ConnectionCallback connectionCallback_;   /*新用户连接的回调*/
    MessageCallback messageCallback_;         /*已连接用户的有读写消息的回调*/
    WriteCompleteCallback writeCompleteCallback_; /*消息发送完成以后的回调*/

    ThreadInitCallback threadInitCallback_;       /*loop线程初始化的回调*/
    
    std::atomic_int started_;

    int nextConnId_;           /*conncetion相关*/
    ConnectionMap connections_;

};


