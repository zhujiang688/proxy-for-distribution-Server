#ifndef MINI_BUFFER_H_
#define MINI_BUFFER_H_

#include <sys/uio.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include <atomic>
#include <string>
#include <vector>

// namespace mini_log {
// muduo
class Buffer {
   public:
    Buffer();
    virtual ~Buffer() = default;

   public:
    // | Prependable |      Readable      |      Writable      |
    // 0           r_idx                w_idx               buf_size
    size_t PrependableBytes() const;
    size_t ReadableBytes() const;
    size_t WritableBytes() const;

   public:
    const char* Peek() const;  // return a ptr of first readable char
    void EnsureWritable(size_t len);
    void HasWritten(size_t len);
    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);
    void RetrieveAll();
    std::string RetrieveAsString(size_t len = 0);

    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff_);

   public:
    ssize_t ReadFd(int fd, int* err_no_);
    ssize_t WriteFd(int fd, int* err_no_);

   private:
    char* Begin();
    const char* Begin() const;
    void MakeSpace(size_t len);

   public:
    const size_t kBufSize = 1024;
    const size_t kCheapPrependable = 8;

   private:
    std::vector<char> m_buffer_;
    std::atomic<size_t> m_read_idx_;
    std::atomic<size_t> m_write_idx_;
};


// }  // namespace mini_log
#endif  // BUFFER_H_