#include "core/net/buffer.h"
#include <string.h>
#include <sys/uio.h>
#include <cassert>

namespace core{
    
const char Buffer::kCRLF[] = "\r\n";
// 查找CRLF（\r\n)
const char* Buffer::findCRLF() const {
    // 笔记：查找子序列第一次出现：auto it = std::search(data.begin(), data.end(), sub.begin(), sub.end());
    const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
    return crlf == beginWrite() ? nullptr : crlf;
}

// 从指定位置开始查找CRLF
const char* Buffer::findCRLF(const char* start) const {
    assert(peek() <= start);
    assert(start <= beginWrite());
    const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF + 2);
    return crlf == beginWrite() ? nullptr : crlf;
}

// 查找换行符
const char* Buffer::findEOL() const {
    const void* eol = memchr(peek(), '\n', readableBytes());
    return static_cast<const char*>(eol);
}

// 从指定位置开始查找换行符
const char* Buffer::findEOL(const char* start) const {
    assert(peek() <= start);
    assert(start <= beginWrite());
    const void* eol = memchr(start, '\n', beginWrite() - start);
    return static_cast<const char*>(eol);
}

// 取出数据，并后移reader_index_
void Buffer::retrieve(size_t len) {
    if (len < readableBytes()) {
        reader_index_ += len;
    } else {
        retrieveAll();
    }
}

// 取出数据到指定位置，即后移reader_index_到end
void Buffer::retrieveUntil(const char* end) {
    assert(peek() <= end);
    assert(end <= beginWrite());
    retrieve(end - peek());
}

// 取出所有数据，或者叫清空Buffer
void Buffer::retrieveAll() {
    reader_index_ = kCheapPrepend;
    writer_index_ = kCheapPrepend;
}

// 取出所有数据并转为字符串
inline std::string Buffer::retrieveAllAsString() {
    return retrieveAsString(readableBytes());
}

// 取出指定长度的数据并转为字符串
inline std::string Buffer::retrieveAsString(size_t len) {
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

// 确保有足够的写空间
inline void Buffer::ensureWritableBytes(size_t len) {
    if (writableBytes() < len) {
        makeSpace(len);
    }
    assert(writableBytes() >= len);
}

// 将数据整体向前移动，或扩容
void Buffer::makeSpace(size_t len){
    if (writableBytes() + prependableBytes() - kCheapPrepend < len) {
        // 空间不够，需要扩容
        buffer_.resize(writer_index_ + len);
    } else {
        // 将数据向前移动
        size_t readable = readableBytes();
        std::copy(begin() + reader_index_,
                    begin() + writer_index_,
                    begin() + kCheapPrepend);
        reader_index_ = kCheapPrepend;
        writer_index_ = reader_index_ + readable;
        assert(readable == readableBytes());
    }
}

// 在数组末尾追加数据，空间不足则扩容
inline void Buffer::append(const char* data, size_t len) {
    ensureWritableBytes(len);
    std::copy(data, data + len, beginWrite());
    hasWritten(len);
}

// 追加字符串
inline void Buffer::append(const std::string& str) {
    append(str.data(), str.size());
}

// 追加其他Buffer
inline void Buffer::append(const Buffer& buf) {
    append(buf.peek(), buf.readableBytes());
}

// 写入数据完成
inline void Buffer::hasWritten(size_t len) {
    assert(len <= writableBytes());
    writer_index_ += len;
}
// 预置数据
void Buffer::prepend(const void* data, size_t len) {
    assert(len <= prependableBytes());
    reader_index_ -= len;
    const char* d = static_cast<const char*>(data);
    std::copy(d, d + len, begin() + reader_index_);
}

// 缩小容量
void Buffer::shrink(size_t reserve) {
    // 笔记：std::copy(InputIt first, InputIt last, OutputIt d_first); 是从前往后一个一个复制，因此覆盖的情况可以酌情而定
    // 将可读数据移到前面
    std::copy(begin() + reader_index_,
                begin() + writer_index_,
                begin() + kCheapPrepend);
    reader_index_ = kCheapPrepend;
    writer_index_ = reader_index_ + readableBytes();
    
    buffer_.resize(writer_index_ + reserve);
    buffer_.shrink_to_fit();
}

// 从fd读取数据
ssize_t Buffer::readFd(int fd, int* savedErrno){
    // 使用栈上的额外空间，避免频繁调整buffer大小
    char extrabuf[65536];
    struct iovec vec[2];
    
    const size_t writable = writableBytes();
    // 第一个缓冲区是Buffer的可写空间
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;
    
    // 第二个缓冲区是栈上的空间
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    // 如果可写空间很小，则使用第二个缓冲区读取数据
    // 如果数据量很大，可能会读到两个缓冲区
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    
    if (n < 0) {
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        // 读取的数据量小于可写空间，直接调整写指针
        writer_index_ += n;
    } else {
        // 读取的数据量大于可写空间，需要将extrabuf中的数据追加到buffer
        writer_index_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    
    return n;
}
}// namespace core