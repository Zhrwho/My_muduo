/**
 * @brief 获取线程id
 * @date 2026.03.25
 * extern：告诉编译器 这个变量在别的翻译单元定义
 */

#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread {
    /* __thread：告诉编译器 每个线程都有自己的副本 */

    extern __thread int t_cachedTid;

    void cacheTid();
    /* 返回调用线程id */
	/* 内联函数，只在当前文件夹起作用*/
    inline int tid()
    {
        /* 告诉编译器：t_cachedTid==0 的情况很少发生”*/
        if(__builtin_expect(t_cachedTid == 0, 0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }

}