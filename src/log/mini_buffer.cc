#include "mini_buffer.h"

Buffer::Buffer()
    : m_buffer_(kBufSize + kCheapPrependable), m_read_idx_(kCheapPrependable), m_write_idx_(kCheapPrependable) {}

size_t Buffer::PrependableBytes() const { return m_read_idx_; }

size_t Buffer::ReadableBytes() const { return m_write_idx_ - m_read_idx_; }

size_t Buffer::WritableBytes() const { return m_buffer_.size() - m_write_idx_; }

const char* Buffer::Peek() const { return Begin() + m_read_idx_; }

// expand buffer size
void Buffer::EnsureWritable(size_t len) {
    if (WritableBytes() < len) {
        MakeSpace(len);
    }
    assert(WritableBytes() >= len);
}

// move write index
void Buffer::HasWritten(size_t len) { m_write_idx_ += len; }

// move read index
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    if (len < ReadableBytes()) {
        m_read_idx_ += len;
    } else {
        RetrieveAll();
    }
}

// move read index to the given pos
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end);
    assert(end <= BeginWrite());
    Retrieve(end - Peek());
}

void Buffer::RetrieveAll() {
    m_read_idx_ = kCheapPrependable;
    m_write_idx_ = kCheapPrependable;
}

std::string Buffer::RetrieveAsString(size_t len) {
    assert(len <= ReadableBytes());
    if (len == 0) {
        len = ReadableBytes();
    }
    std::string str(Peek(), len);
    Retrieve(len);

    return str;
}

char* Buffer::BeginWrite() { return Begin() + m_write_idx_; }

void Buffer::Append(const std::string& str) { Append(str.c_str(), str.length()); }

// put str from m_write_idx
void Buffer::Append(const char* str, size_t len) {
    EnsureWritable(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

// not recommend to use it, since not know what str is
void Buffer::Append(const void* str, size_t len) { Append(static_cast<const char*>(str), len); }

void Buffer::Append(const Buffer& buff_) { Append(buff_.Peek(), buff_.ReadableBytes()); }

ssize_t Buffer::ReadFd(int fd, int* err_no_) {
    char extra_buf[65536];
    struct iovec vec[2];
    const size_t writable = WritableBytes();

    vec[0].iov_base = BeginWrite();
    vec[0].iov_len = writable;
    vec[1].iov_base = extra_buf;
    vec[1].iov_len = sizeof(extra_buf);

    // buffer can be expanded
    // return the number of read bytes
    const ssize_t len = readv(fd, vec, writable < sizeof(extra_buf) ? 2 : 1);
    if (len < 0) {
        *err_no_ = errno;
    } else if (static_cast<size_t>(len) <= writable) {
        m_write_idx_ += len;
    } else {  // expand buffer size & write from original end
        m_write_idx_ = m_buffer_.size();
        Append(extra_buf, len - writable);
    }

    return len;
}

ssize_t Buffer::WriteFd(int fd, int* err_no_) {
    ssize_t len = write(fd, Peek(), ReadableBytes());
    if (len < 0) {
        *err_no_ = errno;
    } else {
        m_read_idx_ += len;
    }

    return len;
}

char* Buffer::Begin() { return &(*m_buffer_.begin()); }

const char* Buffer::Begin() const { return &(*m_buffer_.begin()); }

// Prependable region occupies too much space
// shift the start of Readable region to kCheapPrependable
void Buffer::MakeSpace(size_t len) {
    // move Readable to the front
    assert(kCheapPrependable < m_read_idx_);
    size_t readable = ReadableBytes();
    std::copy(Begin() + m_read_idx_, BeginWrite(), Begin() + kCheapPrependable);
    m_read_idx_ = kCheapPrependable;
    m_write_idx_ = m_read_idx_ + readable;
    assert(readable == ReadableBytes());

    // remainder space is still not enough
    if (WritableBytes() + PrependableBytes() < len + kCheapPrependable) {
        m_buffer_.resize(m_write_idx_ + len);
    }
}