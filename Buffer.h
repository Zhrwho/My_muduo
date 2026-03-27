/**
 * @brief 缓冲区, 防止粘包问题
 * @date 2026.03.22
 */

/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size


 #pragma once

 #include <vector>
 #include <unistd.h>
 #include <string>
 #include <assert.h>

/** 
 * @brief 一个自适应缓冲区，用于对收发数据的缓存
 */

class Buffer
{
public:
	/* 前面数据包的长度 */
    static const size_t KCheapPrepend = 8;
	/* 初始化缓冲区的大小 */
    static const size_t KInitialSize = 1024;

    explicit Buffer(size_t initialSize = KInitialSize)
            : buffer_(KCheapPrepend + initialSize)
            , readerIndex_(KCheapPrepend)
            , writerIndex_(KCheapPrepend) {}

    /* 返回可读数据数量（长度） */
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    /* 返回可写数据数量 */
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    /* 返回读指针位置 */
    size_t prependableBtes() const { return readerIndex_; }
    /* 返回首个可读数据指针，返回缓冲区中可读数据的起始地址 */
    const char* peek() const { return begin() + readerIndex_; }
    /* 设置buffer为初始状态, 可读区都没有了，相当于复位了 */
    void retrieveAll() { readerIndex_ = writerIndex_ = KCheapPrepend; }

    /**
     * @brief 读数据后重置读指针
     * @param len 被读取的字节数
     */
    void retrieve(size_t len) {
        if(len < readableBytes()) {
            readerIndex_ += len;
        }
        else { // len == readableBytes()
            retrieveAll();
        }
    }

    /**
     * @brief 以字符串形式读，把onMessage函数上报的Buffer数据，转成string类型的数据返回
     * @param len 读的长度
     */
    std::string retrieveAsString(size_t len) {
    	assert(len <= readableBytes()); 
        std::string result(peek(), len); /* 从 buffer 当前可读位置，拷贝 len 字节，生成一个 string */
        retrieve(len);
        return result;
    }

    /* 以字符串形式读取所有数据 */
    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }

    /**
     * @brief 写入时检查可写缓冲区是否足够
     * @param len 准备写的长度
     */
    void ensureWritableBytes(size_t len) {
        if(writableBytes() < len) {
			/* 扩容 */
            makeSpace(len); 
        }
    }

    /* 返回写指针地址, copy都需要用指针 */
    char* beginWrite() { return begin() + writerIndex_; }
    /* 返回写指针地址、常量指针 */
    const char* beginWrite() const { return begin() + writerIndex_; }

    /**
     * @brief 在buffer写指针后，添加数据
     * @param data 追加数据
     * @param len 追加长度
     * 把[data, data+len]内存上的数据，添加到writable缓冲区当中
     */
    void append(const char* data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ += len;
    }
    
    /* 从fd上读取数据 */
    ssize_t readFd(int fd, int* saveErrno);
    /* 通过fd 发送数据 */
    ssize_t writeFd(int fd, int* saveErrno);

private:
    /* 获取buffer存储区的首地址，数组首元素的地址 */
    char* begin() { return &*buffer_.begin(); }
    /* 获取buffer存储区的首地址，常量指针重载 */
    const char* begin() const { return &*buffer_.begin(); }

    /**
     * @brief 重新构建缓冲区
     * @param len 需要的空间
     */
    void makeSpace(size_t len) {
        if(writableBytes() + prependableBtes() < len + KCheapPrepend) {
            buffer_.resize(writerIndex_ + len);
			/* buffer重新写为前面的所有的长度+要写入的len的长度 */
        }
        else {
            size_t readable = readableBytes();
			/* 把“可读数据”整体往前挪，腾出后面的空间用于写新数据，加begin() 是指针，copy要指向迭代器 */
			/* | prepend |  readable data	|  writable  |
			|---------|-----------------|------------|
			0	kCheapPrepend	   readerIndex_   writerIndex_ */

            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + KCheapPrepend);
            readerIndex_ = KCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};






