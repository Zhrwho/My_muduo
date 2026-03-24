#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
static int createNonblocking() /*创建非阻塞IO-IPV4*/
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK |SOCK_CLOEXEC, 0);
    if(sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create error:%d \n", __FILE__, __FUNCTION__, __LINE__,errno);
    }
    return sockfd;
}


Acceptor::Acceptor(EventLoop* loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking()) /*需要一个fd*/
    , acceptChannel_(loop, acceptSocket_.fd()) /*channel类型的变量初始化*/
    , listenning_(false) /*channel通过loop和底层的poller通信*/
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddres(listenAddr); /*绑定套接字*/

    /*Tcp_server::start()执行后--Acceptor.listen() 有新用户的连接，要执行一个回调（connfd--channel --subloop)*/
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this)); /*预先注册一个事件回调*/
    /*相当于对listenfd的封装*/

} /*Tcp_server的3个参数*/

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen() /*监听*/
{
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading(); /*把acceptchannel注册到poller里面*/
}


void Acceptor::handleRead()  /*listenfd有事件发送，有新用户连接的时候允许*/
{
    InetAddress peerAddr;  /*客户端连接*/
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0)
    {
        /*执行回调*//*Tcpserver给的*/
        if (newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr); /*socket.cc里面*/
            /*轮询找到subloop,唤醒， 分发当前的新客户端的channel*/
        }
        else{
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__,errno);
        if(errno == EMFILE) /*accept的返回值*/
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit! \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}
