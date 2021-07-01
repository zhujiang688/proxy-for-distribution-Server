#ifndef MINI_LOG_H_
#define MINI_LOG_H_

#include <string.h>
#include <stdarg.h>  // va_list
#include <sys/time.h>
#include <sys/stat.h>  // mkdir
#include <assert.h>

#include <mutex>
#include <string>
#include <thread>
#include <iostream>

#include "blocking_queue.h"
#include "mini_buffer.h"

// namespace mini_log {
// singleton pattern
class Log {
   public:
    static Log& GetInstance();
    static void FlushLogThread();

   public:
    void Init(const char* log_path = "../log", const char* suffix = ".log", int max_queue_capacity = 0);
    void Write(int level, const char* format, ...);
    void Flush();
    bool IsOpen();

   protected:
    Log();
    ~Log();
    Log(const Log&) = delete;
    Log(Log&&) = delete;
    Log& operator=(const Log&) = delete;
    Log& operator=(Log&&) = delete;

   private:
    void AsyncWrite();

   private:
    const int kLogPathLen = 256;
    const int kLogNameLen = 256;
    const int kMaxLines = 50000;
    const int kMaxCharsInOneLine = 256;

    const char* m_path_;
    const char* m_suffix_;

    int m_max_lines_;
    int m_line_count_;
    int m_today_;  // change log when it is tomorrow
    bool m_is_async_;

    Buffer m_buffer_;
    FILE* m_fp_;
    std::unique_ptr<BlockingQueue> m_log_queue_;
    std::unique_ptr<std::thread> m_write_thread_;
    std::mutex m_mutex_;
};

#define LogBase(level, format, ...)                                                                                   \
    do {                                                                                                              \
        if (Log::GetInstance().IsOpen()) {                                                                            \
            Log::GetInstance().Write(level, format " [%s] in [%s:%ld]", ##__VA_ARGS__, __func__, __FILE__, __LINE__); \
            Log::GetInstance().Flush();                                                                               \
        }                                                                                                             \
    } while (0);

#define LogDebug(format, ...)              \
    do {                                   \
        LogBase(0, format, ##__VA_ARGS__); \
    } while (0);
#define LogInfo(format, ...)               \
    do {                                   \
        LogBase(1, format, ##__VA_ARGS__); \
    } while (0);
#define LogWarn(format, ...)               \
    do {                                   \
        LogBase(2, format, ##__VA_ARGS__); \
    } while (0);
#define LogError(format, ...)              \
    do {                                   \
        LogBase(3, format, ##__VA_ARGS__); \
    } while (0);
#endif  // LOG_H_
