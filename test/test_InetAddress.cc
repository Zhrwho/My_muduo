#include "../InetAddress.h"
#include <iostream>

int main()
{
    InetAddress addr(8080);
    std::cout << addr.toIpPort() << std::endl;
    return 0;
}