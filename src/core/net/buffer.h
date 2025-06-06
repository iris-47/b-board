#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <assert.h>

namespace core{

// [0, reader_index_)：已释放的预留空间（可被覆盖）。
// [reader_index_, writer_index_)：待读取的有效数据。
// [writer_index_, buffer_.size())：可写入的空闲空间。

// 用于读写fd的缓冲区
class Buffer{
private:
    std::vector<char> buffer_;
    size_t reader_index_;
    size_t writer_index_;

    static const char kCRLF[];

    // 返回数据起始位置
    char* begin() {return &*buffer_.begin();}
    const char* begin() const {return &*buffer_.begin();}

    // 返回可写位置
    char* beginWrite() { return begin() + writer_index_; }
    const char* beginWrite() const { return begin() + writer_index_; }

    // 创建写空间
    void makeSpace(size_t len);
public:
    static const size_t kCheapPrepend = 8;   // 预留前置空间
    static const size_t kInitialSize = 1024;

    Buffer():buffer_(kCheapPrepend+kInitialSize),
             reader_index_(kCheapPrepend),
             writer_index_(kCheapPrepend){}

    size_t readableBytes() const {return writer_index_ - reader_index_;}
    size_t writableBytes() const { return buffer_.size() - writer_index_;}
    size_t prependableBytes() const { return reader_index_; }

    // 返回可读数据的指针
    const char* peek() const { return begin() + reader_index_; } 

    const char* findCRLF() const;
    const char* findCRLF(const char* start) const;
    const char* findEOL() const;
    const char* findEOL(const char* start) const;

    void retrieve(size_t len);
    void retrieveUntil(const char* end);
    void retrieveAll();
    
    std::string retrieveAllAsString();
    
    std::string retrieveAsString(size_t len);
    void ensureWritableBytes(size_t len);

    void append(const char* data, size_t len);
    void append(const std::string& str);
    void append(const Buffer& buf);

    void hasWritten(size_t len);

    void prepend(const void* data, size_t len);
    void shrink(size_t reserve);

    // 从fd读取数据
    ssize_t readFd(int fd, int* savedErrno);
};

}