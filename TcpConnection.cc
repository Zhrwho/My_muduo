/**
 * @brief 
 * @date 2026.03.22
 * 连接客户端和客户端,一条已建立连接的链路(在subloop管理)
 */
#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <string>

/**
 * @brief 检查eventloop是否为空
 * @param loop 待检查的eventloop
 */
/* static避免名字冲突 */
static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s%s%d TcpLoop is null !\n", __FILE__, __FUNCTION__,__LINE__); /*显示在哪个文件哪个函数哪一行*/
    }

    return loop;
}


TcpConnection::TcpConnection(EventLoop *loop,
                const std::string &nameArg,
                int sockfd, 
                const InetAddress& localAddr,
                const InetAddress& peerAddr)
 : loop_(CheckLoopNotNull(loop))
 , name_(nameArg)
 , state_(kConnecting)
 , reading_(true)
 , socket_(new Socket(sockfd)) /*直接创建一个socket*//*socket_ = std::unique_ptr<Socket>(new Socket(sockfd));*/
 , channel_(new Channel(loop, sockfd))
 , localAddr_(localAddr)
 , peerAddr_(peerAddr)
 , highWaterMark_(64*1024*1024) /*初始化设置为64M*/
{
    /*下面给channel设置相应的回调函数，poller监听到给channel通知感兴趣的事件发生了，channel会回调相应的操作函数*/
    /*设置了一堆的回调*/
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1)
        /*是否需要占位参数，具体看所绑定的函数*/
    );
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this)
    );
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this)
    );
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this)
    );
    /*c_str()作用:把 std::string 转换成 C 风格字符串（const char*）*/
    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd); /*格式化字符串*/
    socket_->setKeepAlive(true);

}
TcpConnection::~TcpConnection()
{
    /*因为没有自己开辟资源，所以不需要释放*/
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n",
        name_.c_str(), channel_->fd(), (int)state_);
}

/**
 * @brief: 发送数据到outputbffer
 * @param loop 待检查的eventloop
 */
 void TcpConnection::send(const std::string &buf) 
 {
    /* 已连接状态 */
    if(state_ == kConnected)
    {
        /*判断loop在不在当前线程*/
        if(loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else{
            loop_->runInLoop(std::bind(
                                        &TcpConnection::sendInLoop,
                                        this,
                                        buf.c_str(),
                                        buf.size()));                          
        }
    }
 }


 /**
 * @brief 向connection对应channel的fd发送数据，一次没写完则使能channel写事件(enablewriting)
 * @param data 数据内容
 * @param len 数据长度
 * 
 * 发送数据 应用写的快，而内核发送数据慢，
 * 需要把待发送数据写入缓冲区 而且设置水位回调防止发送太快
 */
 void TcpConnection::sendInLoop(const void* data, size_t len)
 {
    ssize_t nwrote = 0;
    ssize_t remaining = 0;/*没发送完的数据*/
    bool faultError = false;
    /*之前调用过shutdown，不能再调用send了*/
    if(state_ == kDisconncted) 
    {
        LOG_ERROR("DisConnectied, give up writing!");
        return;
    }
    /*channel会先对读时间感兴趣，对写不感兴趣，所以取非*/
    /*channel第一次开始写数据，而且缓冲区没有待发送数据（readable为0）*/
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        /*发送*/
        nwrote = ::write(channel_->fd(), data, len);
        if(nwrote >= 0) /*发送成功*/
        {
            remaining = len - nwrote; /*还剩多少数据没有发送完*/
            if(remaining == 0 && writeCompleteCallback_) /*全部发送完了，且写入完成回调用户也注册过了*/
            {
                /*既然在这里数据全部发送完成，那也就不用再给channel设置epollout事件了，不会再执行下面的handlewrite事件*/
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                /*执行回调方法*/
            }
        }
        else //nwrote < 0 ，出错
        {
            nwrote = 0; /*重置*/
            if(errno != EWOULDBLOCK)
            {
                LOG_FATAL("TcpConnection::senInLoop");
                if(errno == EPIPE || errno == ECONNRESET) /*接收到对端的socket重置*/
                {
                    faultError = true;
                }
            }
        }
    }

    /*说明连接正常，当前这一次write并没有把数据全部发送出去，所以剩余的数据需要保存到outputbuffer*/
    /*然后给channel注册epollout事件， poller 发现tcp的发送缓冲区有空间，会通知相应的sock(channel)，调用writeCallback_(handlewrite)回调方法*/
    /*对于channel来说的writecallback, 是tcpconnection给的handlewrite*/
    /*最终也就是调用TcpConnection::handlewrite方法，把发送缓冲区的数据全部发送完成*/
    if(!faultError && remaining > 0)
    {
        size_t oldLen = outputBuffer_.readableBytes(); /*目前发送缓冲区剩余的待发送数据的长度*/
        /*发送缓冲区第一次超过阈值*/
        if(oldLen + remaining > highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_)
        {
            /*把一个任务（cb）扔进 EventLoop 所在线程，稍后执行*/
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen+remaining));
        }
        /*把待发送数据添加到缓冲区中*/
        outputBuffer_.append((char*)data + nwrote, remaining);
        if( !channel_->isWriting())
        {
            /*一定要注册channel的写事件，否则poller不会给channel通知epollout*/
            channel_->enablewriting();
        }
    }

 }

/**
 * @brief 关闭连接，向loop中添加shutdownInLoop
 * 用户调用
 */
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting); /*正在断开连接*/
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
        /*把“关闭连接”的操作，丢到所属的 EventLoop 线程去执行*/
    }
}

/**
 * @brief 在channel无写事件时，关闭socket写端
 */
void TcpConnection::shutdownInLoop()
{
    /*isWriting() = “当前是否在监听 EPOLLOUT（写事件）”*/
    /*channel不在写状态，说明当前outputbuffer中的数据已经全部发送完成*/
    if(!channel_->isWriting()) 
    /*数据发送完，channel不是iswriting才会真的shutdownInloop*/
    /*所以即使上面的shutdown设置了状态为kdisconnecting,也不会真的shutdown，而是会在handlewrite里面一直写*/
    /*301行左右：                if(state_ == kDisconnecting) /*正在关闭*/
    /*直到，handlewrite把所有数据都写完了！又发现这个state变了，知道调用了shutdown，才会去执行这个shutdownInloop!*/
    {
        /*socket.cc文件，关闭写端，会触发EPOLLHUP*/
        socket_->shutdownWrite(); 
        /*就会调用channel的closecallback*/
        /*会回调tcpconnection 的handleclose方法*/
    }
}

/**
 * @brief 建立连接，使能channel读，TcpServer调用,执行连接回调
 */
void TcpConnection::connectEstablished()
{
    setState(kConnected);/*连接成功,记录state*/
    /*tie转化为强智能指针，底层指向的资源不会被释放*/
    channel_->tie(shared_from_this()); 
    /*保证Tcpconnection不会被销毁*/
    channel_->enableReading(); /*向poller注册channel的读事件（epollin事件）*/

    /*新连接建立，执行回调*/
    /*shared_from_this()：当前这个TcpConnection对象的shared_ptr*/
    connectionCallback_(shared_from_this());

}

/**
 * @brief 销毁连接，使能channel所有事件，执行连接回调，在loop中remove channel
 */
/*连接销毁,连接关闭*/
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)
    {
        /*状态设置为未连接*/
        setState(kDisconncted);
        /*把channel的所有感兴趣的事件，从poller中删除掉*/
        channel_->disableAll(); 
        

        connectionCallback_(shared_from_this()); /*断开连接*/
    }
    channel_->remove(); /*把channel从poller中删除掉*/
}

/**
 * @brief 读事件处理，从fd读到inputbuffer，调用消息回调
 */
void TcpConnection::handleRead(Timestamp receiveTime)
/*当 socket 可读时，把内核数据读到用户缓冲区，并交给用户回调处理*/
/*channel上的fd有事件可读时，就读*/
{
    int savedErrno = 0;
    /*把fd上的数据读入到Buffer的底层缓冲区*/
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if(n > 0) /*有数据*/
    {
        /*已建立连接的用户有可读事件发生了，调用用户传入的回调操作onMessage*/
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if(n == 0) /*客户端断开了*/
    {
        handleClose();
    }
    else{ /*出错了*/
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }

}

/**
 * @brief 写事件处理，将outputbuffer中的数据写到fd，写完毕调用回调
 */
void TcpConnection::handleWrite()
{
    if(channel_->isWriting()) /*监听是否可写*/
    {
        int savedErrno = 0;
        /* 写到channel_fd() */
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        /*从用户 Buffer 读取数据 → 写入内核 socket 缓冲区*/

        if( n > 0)
        {
            /*相当于把读的下标指针往后移动一会*//*所以移动读下标*/
            outputBuffer_.retrieve(n);  
            /* 发送完成，完全没有数据了 */
            if(outputBuffer_.readableBytes() == 0) 
            {
                channel_ ->disableWriting();
                if(writeCompleteCallback_) /*如果有这么个回调就执行回调*/
                {
                    /*唤醒这个loop_对应的thread线程，执行回调*/
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())
                    );
                }
                /*正在关闭*/
                if(state_ == kDisconnecting) 
                {
                    shutdownInLoop();
                }
            }
        }
        else{
            LOG_ERROR("TcpConnection::handleWrite");
        }

    }
    else{
        LOG_INFO("Connection fd = %d is down, no more writing \n", channel_->fd());
    }
}

/**
 * @brief 关闭事件处理，失能channel所有事件，调用连接、关闭回调
 */
/*poller  => channel::closeCallback => TcpConnection::handleClose*/
void TcpConnection::handleClose()
{
    LOG_INFO("fd = %d state = %d \n", channel_->fd(), (int)state_);
    assert(state_ == kConnected || state_ == kDisconnecting);
    setState(kDisconncted);
    /*chanel所有的事件都不感兴趣了，从eopllpoller，ctl删掉*/
    channel_ -> disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); /*执行连接关闭的回调*/
    closeCallback_(connPtr);      /*关闭连接的回调*/ /*执行的是Tcpserver：：removeConnectin回调方法*/
}

/**
 * @brief 错误事件处理，获取错误码，打日志
 */
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {

        err = errno;
    }
    else{
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}