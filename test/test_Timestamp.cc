/**
 * 编译运行：g++ test/test_Timestamp.cc Timestamp.cc -o test/test_Timestamp
 */

#include "../Timestamp.h"

int main()
{
    Timestamp test = Timestamp::now();
    std::cout << test.toString() << std::endl;

    return 0;
}