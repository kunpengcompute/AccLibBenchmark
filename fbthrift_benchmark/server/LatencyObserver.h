#pragma once
#include <thrift/lib/cpp/server/TServerObserver.h>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <cstdint>
class LatencyObserver : public apache::thrift::server::TServerObserver {
  public:
    explicit LatencyObserver(const std::string& filename)
        : TServerObserver(1), fp_(nullptr), cnt_(0), server_time_sum_(0), queue_sum_(0),
          proc_sum_(0), write_delay_sum_(0), write_sum_(0) {
        fp_ = fopen(filename.c_str(), "w");
        if (fp_) {
            fprintf(fp_, "SERVER_TIME,QUEUE,PROC,WRITE_DELAY,WRITE\n");
            fflush(fp_);
        }
    }
    ~LatencyObserver() {
        if (fp_) {
            fclose(fp_);
        }
        fprintf(stderr, "\n============================== Latency Summary ==============================\n");
        fprintf(stderr, "Total requests: %lld\n", (long long)cnt_);
        fprintf(stderr, "SERVER_TIME(us) : avg=%lld (sum=%lld)\n",
                cnt_ > 0 ? (long long)(server_time_sum_ / cnt_) : 0, (long long)server_time_sum_);
        fprintf(stderr, "QUEUE(us)       : avg=%lld\n",
                cnt_ > 0 ? (long long)(queue_sum_ / cnt_) : 0);
        fprintf(stderr, "PROC(us)        : avg=%lld\n",
                cnt_ > 0 ? (long long)(proc_sum_ / cnt_) : 0);
        fprintf(stderr, "WRITE_DELAY(us) : avg=%lld\n",
                cnt_ > 0 ? (long long)(write_delay_sum_ / cnt_) : 0);
        fprintf(stderr, "WRITE(us)       : avg=%lld\n",
                cnt_ > 0 ? (long long)(write_sum_ / cnt_) : 0);
        fprintf(stderr, "===========================================================================\n");
    }
    void callCompleted(const CallTimestamps& ts) override {
        int64_t queue_us = ts.processDelayLatencyUsec().value_or(0);
        int64_t proc_us = ts.processLatencyUsec().value_or(0);
        int64_t write_delay_us = ts.writeDelayLatencyUsec().value_or(0);
        int64_t write_us = ts.writeLatencyUsec().value_or(0);
        int64_t server_time_us = queue_us + proc_us + write_delay_us + write_us;
        std::lock_guard<std::mutex> lg(mutex_);
        cnt_++;
        server_time_sum_ += server_time_us;
        queue_sum_ += queue_us;
        proc_sum_ += proc_us;
        write_delay_sum_ += write_delay_us;
        write_sum_ += write_us;
        if (fp_) {
            fprintf(fp_, "%lld,%lld,%lld,%lld,%lld\n",
                    (long long)server_time_us, (long long)queue_us, (long long)proc_us,
                    (long long)write_delay_us, (long long)write_us);
            fflush(fp_);
        }
    }
  private:
    FILE* fp_;
    std::mutex mutex_;
    int64_t cnt_;
    int64_t server_time_sum_;
    int64_t queue_sum_;
    int64_t proc_sum_;
    int64_t write_delay_sum_;
    int64_t write_sum_;
};