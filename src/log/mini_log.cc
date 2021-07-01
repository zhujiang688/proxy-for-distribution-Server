#include "mini_log.h"
#include <stdio.h>

Log::Log() { Init(); }

Log::~Log() {
    if (m_write_thread_ && m_write_thread_->joinable()) {
        if (!m_log_queue_->IsEmpty()) {
            m_log_queue_->Flush();
        }
        m_log_queue_.reset();
        m_write_thread_->join();
    }
    if (m_fp_) {
        std::lock_guard<std::mutex> locker(m_mutex_);
        Flush();
        fclose(m_fp_);
    }
}

Log& Log::GetInstance() {
    static Log instance;
    return instance;
}

void Log::FlushLogThread() { Log::GetInstance().AsyncWrite(); }

void Log::Init(const char* log_path, const char* suffix, int max_queue_capacity) {
    if (max_queue_capacity > 0) {
        m_is_async_ = true;
        if (!m_log_queue_) {
            std::unique_ptr<BlockingQueue> temp_bq(new BlockingQueue(max_queue_capacity));  // no delete
            m_log_queue_ = std::move(temp_bq);

            // create thread, write log asynchronously
            std::unique_ptr<std::thread> temp_thd(new std::thread(FlushLogThread));
            m_write_thread_ = std::move(temp_thd);
        }
    } else {
        m_is_async_ = false;
    }

    m_line_count_ = 0;
    m_path_ = log_path;
    m_suffix_ = suffix;

    time_t curr_time = time(nullptr);
    struct tm* fmt_time = localtime(&curr_time);

    char log_name[kLogNameLen];
    // global log
    snprintf(log_name, kLogNameLen - 1, "%s/%04d-%02d-%02d%s", m_path_, fmt_time->tm_year + 1900, fmt_time->tm_mon + 1,
             fmt_time->tm_mday, m_suffix_);
    m_today_ = fmt_time->tm_mday;

    std::lock_guard<std::mutex> locker(m_mutex_);
    m_buffer_.RetrieveAll();  // move read and write ptrs to initial pos
    if (m_fp_) {
        Flush();
        fclose(m_fp_);
    }

    m_fp_ = fopen(log_name, "a");
    if (!m_fp_) {
        mkdir(m_path_, 0755);
        m_fp_ = fopen(log_name, "a");
    }
    assert(m_fp_ != nullptr);
}

// [yyyy-mm-dd HH:MM:SS.miusec][level] Log
void Log::Write(int level, const char* format, ...) {
    struct timeval now = {0, 0};  // second, microsecond
    gettimeofday(&now, nullptr);
    struct tm* fmt_time = localtime(&now.tv_sec);
    va_list valist;

    // [level]
    char log_head[9] = {0};
    switch (level) {
        case 0:
            strncpy(log_head, "[Debug] ", 9);
            break;
        case 1:
            strncpy(log_head, "[ Info] ", 9);
            break;
        case 2:
            strncpy(log_head, "[ Warn] ", 9);
            break;
        case 3:
            strncpy(log_head, "[Error] ", 9);
            break;

        default:
            break;
    }

    {  // write log
        std::lock_guard<std::mutex> locker(m_mutex_);
        ++m_line_count_;

        // tomorrow or new file
        if (m_today_ != fmt_time->tm_mday || (m_line_count_ && (m_line_count_ % kMaxLines == 0))) {
            char new_log_file[kLogNameLen];
            char log_name_time[32] = {0};
            snprintf(log_name_time, 32, "%04d-%02d-%02d", fmt_time->tm_year + 1900, fmt_time->tm_mon + 1,
                     fmt_time->tm_mday);

            if (fmt_time->tm_mday != m_today_) {
                snprintf(new_log_file, kLogNameLen - 1, "%s/%s%s", m_path_, log_name_time, m_suffix_);
            } else {
                snprintf(new_log_file, kLogNameLen - 1, "%s/%s-%d%s", m_path_, log_name_time,
                         (m_line_count_ / kMaxLines), m_suffix_);
            }

            Flush();
            fclose(m_fp_);
            m_fp_ = fopen(new_log_file, "a");
            assert(m_fp_ != nullptr);
        }

        // [yyyy-mm-dd HH:MM:SS.miusec][level] : 36 chars
        m_buffer_.EnsureWritable(40);
        int real_bytes = snprintf(m_buffer_.BeginWrite(), 40, "[%04d-%02d-%02d %02d:%02d:%02d.%06ld]%s",
                                  fmt_time->tm_year + 1900, fmt_time->tm_mon + 1, fmt_time->tm_mday, fmt_time->tm_hour,
                                  fmt_time->tm_min, fmt_time->tm_sec, now.tv_usec, log_head);
        m_buffer_.HasWritten(real_bytes);

        // Log
        m_buffer_.EnsureWritable(kMaxCharsInOneLine);
        va_start(valist, format);
        real_bytes = vsnprintf(m_buffer_.BeginWrite(), m_buffer_.WritableBytes() - 2, format, valist);
        va_end(valist);

        m_buffer_.HasWritten(real_bytes);
        m_buffer_.Append("\n\0", 2);

        if (m_is_async_ && m_log_queue_ && !m_log_queue_->IsFull()) {
            m_log_queue_->Emplace(m_buffer_.RetrieveAsString());
        } else {
            fputs(m_buffer_.Peek(), m_fp_);
        }
        m_buffer_.RetrieveAll();
    }
}

// obligate to write into FILE*
void Log::Flush() {
    // TODO: necessary to flush BlockingQueue?
    if (m_is_async_ && m_log_queue_) {
        m_log_queue_->Flush();
    }
    fflush(m_fp_);
}

bool Log::IsOpen() { return m_fp_ != nullptr; }

void Log::AsyncWrite() {
    std::string str;
    while (m_log_queue_->Pop(str)) {
        std::lock_guard<std::mutex> locker(m_mutex_);
        fputs(str.c_str(), m_fp_);
    }
}