#ifndef BLOCKING_QUEUE_H_
#define BLOCKING_QUEUE_H_

#include <mutex>
#include <deque>
#include <condition_variable>
#include <string>

// namespace mini_log {
class BlockingQueue {
   public:
    explicit BlockingQueue(size_t max_capacity = 1000);
    virtual ~BlockingQueue();

   public:
    size_t Size();
    void Clear(bool is_close = false);
    bool IsEmpty();
    bool IsFull();
    std::string Front();
    void Emplace(std::string&& item);
    bool Pop(std::string& item);

   public:
    void Flush();

   private:
    std::deque<std::string> m_deq_;
    size_t m_capacity_;
    std::mutex m_mutex_;
    bool m_is_close_;
    std::condition_variable m_cond_consumer_;
    std::condition_variable m_cond_producer_;
};


// }  // namespace mini_log
#endif  // BLOCKING_QUEUE_H_