#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <strings.h>
#include <functional>

static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s%s%d mainLoop is null !\n", __FILE__, __FUNCTION__,__LINE__); /*显示在哪个文件哪个函数哪一行*/
    }

    return loop;
}

    
/*关键就是接收了一个mainloop*/

TcpServer::TcpServer(EventLoop* loop
            , const InetAddress& listenAddr
            , const std::string& nameArg
            , Option option) /*默认值不能重复给*/
            : loop_(CheckLoopNotNull(loop)) /*先check一下有这个loop存在*/
            , ipPort_(listenAddr.toIpPort())
            , name_(nameArg) /*服务器名称*/

            , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)) /*处理新用户连接*/

            , threadPool_(new EventLoopThreadPool(loop, name_)) /*事件循环线程池*/
            , connectionCallback_()
            , messageCallback_()
            , nextConnId_(1)
{
    /*当有新用户连接时，会执行TcpServer::newConnection回调，绑定回调*/
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                        std::placeholders::_1, std::placeholders::_2)); /*传入两个参数:connfd和peerAddr*/
}
TcpServer::~TcpServer()
{
    for ( auto& item : connections_)
    {
        /*这个局部的shaerd_ptr对象出右括号可以自动释放 new出来的tcpconnection对象资源*/
        TcpConnectionPtr conn(item.second); 
        item.second.reset();
        /*先释放就找不到conn了*/
        conn->getloop()->runInLoop(
            /*销毁连接*/
            std::bind(&TcpConnection::connectDestroyed, conn)
        );
    }
}

void TcpServer::setThreadNum(int numThreads) /*设置底层subloop的个数*/
{
    threadPool_->setThreadNum(numThreads); /*给底层的threadPool设置numberthreads*/
}


void TcpServer::start() /*开启服务器监听*/
{
    if( started_++ == 0 )/*防止一个TcpServer对象被start多次*/
    /*只有第一个线程能进入if语句，后续starrted被++了不再为0*/
    {
        threadPool_->start(threadInitCallback_); /*启动底层线程池,跳转线程池的start*/
        /*启用listen。新用户连接成功返回通信用的connfd*/
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get())); /*get返回内部管理的“原始指针”（裸指针）*/
    }

}

/*有一个新的客户端的连接，acceptor会执行这个回调操作*/
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) /*新连接来*/
{
    /*轮询算法，选择一个subloop，来管理channel*/
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
    
    connections_[connName] = conn; /*map键值对插入*/
    /*下面的回调都是用户设置给TcpServer的，Tcpserver设置给connection，设置给channel，再注册到poller上，最后poller通知channel调用channel*/
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

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

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