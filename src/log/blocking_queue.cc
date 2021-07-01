#include "blocking_queue.h"

// Before constructing one object, ensure that max_capacity is valid,
// i.e. max_capacity > 0
BlockingQueue::BlockingQueue(size_t max_capacity) : m_capacity_(max_capacity), m_is_close_(false) {}

BlockingQueue::~BlockingQueue() {
    Clear(true);
    m_cond_producer_.notify_all();
    m_cond_consumer_.notify_all();
}

size_t BlockingQueue::Size() {
    std::lock_guard<std::mutex> locker(m_mutex_);
    return m_deq_.size();
}

void BlockingQueue::Clear(bool is_close) {
    std::lock_guard<std::mutex> locker(m_mutex_);
    m_deq_.clear();
    m_is_close_ = is_close;
}

bool BlockingQueue::IsEmpty() {
    std::lock_guard<std::mutex> locker(m_mutex_);
    return m_deq_.empty();
}

bool BlockingQueue::IsFull() {
    std::lock_guard<std::mutex> locker(m_mutex_);
    return m_deq_.size() >= m_capacity_;
}

std::string BlockingQueue::Front() {
    std::lock_guard<std::mutex> locker(m_mutex_);
    return m_deq_.front();
}

void BlockingQueue::Emplace(std::string&& item) {
    std::unique_lock<std::mutex> locker(m_mutex_);
    m_cond_producer_.wait(locker, [this] { return m_deq_.size() < m_capacity_; });

    m_deq_.emplace_back(item);
    m_cond_consumer_.notify_one();
}

bool BlockingQueue::Pop(std::string& item) {
    std::unique_lock<std::mutex> locker(m_mutex_);
    while (m_deq_.empty()) {
        m_cond_consumer_.wait(locker);
        if (m_is_close_) {
            return false;
        }
    }
    item = m_deq_.front();
    m_deq_.pop_front();
    m_cond_producer_.notify_one();

    return true;
}

void BlockingQueue::Flush() { m_cond_consumer_.notify_one(); }