/**
 * @brief 获取线程id
 * @date 2026.03.25
 */

#include "CurrentThread.h"

namespace CurrentThread {
    __thread int t_cacheTid = 0;
    /*获取当前线程*/
    void cacheTid()
    {
        if(t_cachedTid == 0)
        {
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}

