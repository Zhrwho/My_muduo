/**
 * @brief Socket封装类
 * @date 2026.03.26
 */

  #pragma once

#include "noncopyable.h"

class InetAddress;

// 封装socket fd
class Socket : public mymuduo::noncopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {}

    ~Socket();

    /* const 只读接口 */
    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress *peeraddr);

    void shutdownWrite();

    /* 更改TCP选项 */
    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);
    
private:
    const int sockfd_;
};