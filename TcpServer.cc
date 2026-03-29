
/**
 * @brief:用户使用muduo编写服务器程序的接口类
 * 对外的服务器编程使用的类
 */
#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <strings.h>
#include <functional>

/* 检测一下这个loop是否为空 */
static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s%s%d mainLoop is null !\n", __FILE__, __FUNCTION__,__LINE__); /*显示在哪个文件哪个函数哪一行*/
    }

    return loop;
}

    
/*关键就是接收了一个mainloop*/
/**
 * @brief 构造函数，，构造后设置新连接产生回调
 * @param loop 传入的EventLoop指针
 * @param listenAddr 传入的监听socket地址
 * @param nameArg 传入的Server名称
 */
TcpServer::TcpServer(EventLoop* loop    // 外边传进来的
            , const InetAddress& listenAddr
            , const std::string& nameArg
            , Option option) /*默认值不能重复给*/
            : loop_(CheckLoopNotNull(loop)) /*先check一下有这个loop存在*/
            , ipPort_(listenAddr.toIpPort())
            , name_(nameArg) /*服务器名称*/
            // server持有的loop和acceptor持有的loop是同一个,都是外边用户创建后传过来的
            /*处理新用户连接,监听listenfd*/
            , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)) 

            , threadPool_(new EventLoopThreadPool(loop, name_)) /*事件循环线程池*/
            , connectionCallback_()
            , messageCallback_()
            , nextConnId_(1)
{
    /*当有新用户连接时，会执行TcpServer::newConnection回调，绑定回调*/
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                        std::placeholders::_1, std::placeholders::_2)); 
                                        /*传入两个参数:connfd和peerAddr*/
}

/**
 * @brief 析构函数，遍历所有连接并销毁
 */
TcpServer::~TcpServer()
{
    for ( auto& item : connections_)
    {
        /*这个局部的shaerd_ptr对象出右括号可以自动释放new出来的tcpconnection对象资源*/
        TcpConnectionPtr conn(item.second); 
        item.second.reset();
        /*先释放就找不到conn了*/
        conn->getloop()->runInLoop(
            /*销毁连接*/
            std::bind(&TcpConnection::connectDestroyed, conn)
        );
    }
}

/**
 * @brief 设置线程数量
 * @param nunThreads 线程数量
 */
void TcpServer::setThreadNum(int numThreads) /*设置底层subloop的个数*/
{
    threadPool_->setThreadNum(numThreads); /*给底层的threadPool设置numberthreads*/
}

/**
 * @brief 开启事件循环，创建线程，主loop开启监听
 */
void TcpServer::start() /*开启服务器监听*/
{
    if( started_++ == 0 )/*防止一个TcpServer对象被start多次*/
    /*只有第一个线程能进入if语句，后续starrted被++了不再为0*/
    {
        threadPool_->start(threadInitCallback_); /*启动底层线程池,跳转线程池的start*/
        /*启用listen。新用户连接成功返回通信用的connfd*/
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get())); 
        /* listen函数: acceptchannel_.enablereading() :就是注册到pool中*/
        /*get返回内部管理的“原始指针”（裸指针）*/
    }

}

/*有一个新的客户端的连接，acceptor会执行这个回调操作*/
/**
 * @brief 新连接处理函数，为其创建TcpConnection，将连接分配给子loop，并设置其回调
 * @param sockfd 新连接fd
 * @param peerAddr 新连接对端socket地址
 */
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) /* 新连接来,参数在acceptor::handleRead里的coonfd,peerAddr */
{
    /*轮询算法，选择一个loop，来管理channel*/
    EventLoop* ioLoop = threadPool_->getNextLoop();
    /*给connection一个名字*/
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection[%s] - new connection [%s]  from %s \n ",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
    
    /*通过通信的sockfd来获取绑定的本机的的IP地址和端口号*/
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof addrlen;
    if(::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    LOG_ERROR("sockets::getLocalAddr");

    /*根据连接成功的sockfd，创建TcpConnection连接对象 */
    InetAddress localAddr(local);

    /*根据连接成功的sockfd，创建TcpConnection连接对象*/
    TcpConnectionPtr conn(new TcpConnection(
                            ioLoop,
                            connName,
                            sockfd,
                            localAddr,
                            peerAddr));
    /* 建立连接列表 */
    connections_[connName] = conn; /*map键值对插入*/
    /*下面的回调都是用户设置给TcpServer的，Tcpserver设置给connection，设置给channel，再注册到poller上，最后poller通知channel调用回调*/
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    
    /*设置了如何关闭连接的回调*/
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
    );
    /*直接调用TcpConnection::connectEstablished*/
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

/**
 * @brief 删除连接处理函数，将销毁连接函数注册到其queueinloop
 * @param conn 要删除的连接
 */
void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

/**
 * @brief 删除连接处理函数，首先从连接列表中删除，随后将销毁链接函数添加到其回调队列
 * @param conn 要删除的连接
 */
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection [%s] \n", name_.c_str(), conn->name().c_str());

    size_t n = connections_.erase(conn->name()); /*从map表中删除*/
    (void) n;
    assert(n == 1);/*确定要删除一个*/
    EventLoop* ioLoop = conn->getloop(); /*获取了connection所在的loop*/
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn)
    );
}