/**
 * @brief Socket地址封装类
 * @date 2026.03.24
 * bzero:清零函数,把一段内存全部置为 0
 * c_str():C++ 的 std::string 转成 C 风格字符串（const char*）
 */
#include <strings.h>
#include <string.h>

#include "InetAddress.h"


InetAddress::InetAddress(uint64_t port, std::string ip)
{
    /*bzero:清零函数,把一段内存全部置为 0*/
    bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;
    /*htons：host to network short，把主机字节序（Host）转换成网络字节序（Network）的 short（16 位）整数*/
    addr_.sin_port = htons(port);
    /*c_str():C++ 的 std::string 转成 C 风格字符串（const char*）*/
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}

/**
 * @brief 输出socket地址的ip
 * @return 点分十进制ip
 */
std::string InetAddress::toIp() const
{
    /*上面已经转成了网络字节序了，这要转成本地字节序才能读出来*/
    char buf[64] = { 0 };
    /*读出来整数的表示后，还要转成本地字节序*/
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    return buf;
}

/**
 * @brief 输出socket地址的ip以及port
 * @return 点分十进制ip:port
 */
std::string InetAddress::toIpPort() const
{
    /*ip:port*/
    char buf[64] = { 0 };
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    /*算ip的有效长度，再算port*/
    size_t end = strlen(buf);
    /*本地转网络*/
    uint16_t port = ntohs(addr_.sin_port);
    /*buf 本质是 指向数组首元素的指针（char）* */
    /*从字符串末尾开始写入的位置*/
    sprintf(buf + end, ":%u", port);
    return buf;
}

/**
 * @brief 输出socket地址的port
 * @return port uint16_t
 */
uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}