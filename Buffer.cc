/**
 * @brief 网络库缓冲类
 * @date 2026.03.27
 * 用户态的内存缓冲区，主要用来解决 TCP socket 的半包、粘包问题 和 非阻塞 IO 的数据管理。
 * InputBuffer：缓存 fd 数据 → 给用户
 * OutputBuffer：缓存用户数据 → 写给 fd
 */

#include "Buffer.h"
#include <unistd.h>
#include <sys/uio.h>


/**
 * @brief 从fd，读数据 (Poller工作在LT模式,会不断读取)
 * @param fd 目标文件描述符
 * @param saveErrno errno存储
 * Buffer缓冲区是有大小的！ 但是从fd上读数据的时候，却不知道tcp数据最终的大小
 */
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    /* 开一个栈上的空间64K, 能自动回收, 效率快 buffer是vector上的堆上的内存,会浪费*/
    char extrabuf[65536] = {0}; 
    
    struct iovec vec[2];
    
    /* 这是Buffer底层缓冲区剩余的可写空间大小 */
    const size_t writable = writableBytes(); 

    /* 存堆上的 */
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    /* 存超出长度的 */
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;
    
    /* 如果还有数据, epoll会再次触发(LT模式),下次再来readv, writable >= extrabuf, 说明空间已经很大了 */
    /* 够用就行,不追求一次读完 */
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1; /*一次最少读64K,提高读取效率*/
    /* readv会自动合并两段的内容 ,redav:一次系统调用，把数据读到多个缓冲区 */
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable) // Buffer的可写缓冲区已经够存储读出来的数据了
    {
        writerIndex_ += n;
    }
    else // extrabuf里面也写入了数据 
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);  // writerIndex_开始写 n - writable大小的数据
    }

    return n;
}


/**
 * @brief 向fd，写数据
 * @param fd 目标文件描述符
 * @param saveErrno errno存储
 */
ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    /**
     * @param:readableBytes()可写字节数
     * @param: peek()开始读的位置 位置
     * @return: 实际写了多少字节
     * 从 buffer 当前可读位置（peek()）开始，把所有可读字节（readableBytes()）写到 fd 对应的 socket 上，返回实际写了多少字节（n）
     * */
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}
