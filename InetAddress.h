/**
 * @brief Socket地址封装类(ip+端口号)
 * @date 2026.03.24
 */

#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

/**
 * @brief 对socket_in地址的封装
 */
class InetAddress
{
public:
    explicit InetAddress(uint64_t port = 0, std::string ip = "127.0.0.1");
    /*直接传过来的sockaddr_in*/
    explicit InetAddress(const sockaddr_in& addr) : addr_(addr) {}

    /*获取ip和端口号*/
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    /**
     * @brief 返回socket地址
     * @return socket_in地址指针，常量指针
     */
    const sockaddr_in* getSockAddr() const { return &addr_; }

    /**
     * @brief 设置socket地址
     * @param addr socket地址，常引用
     */
    void setSockAddr(const sockaddr_in& addr) { addr_ = addr; }

private:
    sockaddr_in addr_;
    /*sockaddr_in 是IPV4套接字地址结构体，用于IPV4的地址和端口*/
};
