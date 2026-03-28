#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/**
 * @brief:TcpServer通过Acceptor
 * 有一个新用户连接，通过Acceptor函数拿到connfd
 * 然后就可以打包TcpConnection 设置相应回调
 * 再把回调给相应的Channel,
 * channel再注册到poller上，poller监听到事件之后，就会调用channel的回调操作
 */

class TcpConnection : public mymuduo::noncopyable, public std::enable_shared_from_this<TcpConnection> /*支持通过智能指针安全获取自身*/
{
public:
    TcpConnection(EventLoop *loop,
                 const std::string &name,
                 int sockfd, 
                 const InetAddress& localAddr,
                 const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getloop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; } /*返回状态是否成功*/
    bool disconnected() const { return state_ == kDisconncted; }

    void send(const std::string &buf); /*发送数据给客户端*/
    void shutdown(); /*关闭半连接，只关闭服务端的写连接，用户要调用的shutdown*/

    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
    void setHigeWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) 
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_ = cb; }

    /*连接建立*/
    void connectEstablished();
    /*连接销毁*/
    void connectDestroyed();


private:
    void handleRead(Timestamp reveiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* data, size_t len);
    
    void shutdownInLoop();

    enum StateE {kDisconncted/*已经断开*/, kConnecting/*正在连接*/, kConnected/*已经连接成功*/, kDisconnecting/*正在断开连接*/ };
    
   void setState(StateE state) { state_ = state; }

    EventLoop *loop_; /*绝对不是baseloop，因为TcpConnection 都是在subloop里面管理的*/
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    /*socket和channel*/ /*和Acceptor相似*/
    /*Acceptor在mainLoop里面， TcpConnection在subLoop里面，都需要将底层socket封装成channel，然后再相应poller里面监听事件*/
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_; /*记录当前主机的IP地址*/
    const InetAddress peerAddr_;  /*记录对端的端口号*/

    /*回调*/
    /*TcpServer---TcpConnection---Channel,然后poller监听channel，当有事件发生则执行相应的回调*/
    ConnectionCallback connectionCallback_;
    MessageCallback  messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;

    size_t highWaterMark_; /*水位线标志*/

    /*数据缓冲区*/
    Buffer inputBuffer_;  //*接收数据的缓冲区*/
    Buffer outputBuffer_; ///*发送数据的缓冲区*/ /*send发到output里面/
};

