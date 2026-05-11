// latency_stats.h
#pragma once

#include <cstdint>
#include <folly/stats/Histogram.h>

class LatencyStats {
public:
    // 构造：桶宽(us)，最小值，最大值。例如 1000us=1ms 一个桶，范围 0~100000us (100ms)
    LatencyStats(int64_t bucketSize = 1000, int64_t min = 0, int64_t max = 100000)
        : hist_(bucketSize, min, max), sum_(0), count_(0), min_(0), max_(0) {}

    // 记录一个延迟值（微秒）
    void addValue(int64_t latency_us) {
        hist_.addValue(latency_us);
        sum_ += latency_us;
        count_++;
        if (count_ == 1) {
            min_ = latency_us;
            max_ = latency_us;
        } else {
            if (latency_us < min_) min_ = latency_us;
            if (latency_us > max_) max_ = latency_us;
        }
    }

    // 合并另一个 LatencyStats 对象
    void merge(const LatencyStats& other) {
        if (other.count_ == 0) return;
        hist_.merge(other.hist_);
        sum_ += other.sum_;
        count_ += other.count_;
        if (count_ == other.count_) { // 当前为空，则直接取对方
            min_ = other.min_;
            max_ = other.max_;
        } else {
            if (other.min_ < min_) min_ = other.min_;
            if (other.max_ > max_) max_ = other.max_;
        }
    }

    // 获取总样本数
    uint64_t getCount() const { return count_; }

    // 获取总延迟和
    int64_t getSum() const { return sum_; }

    // 获取平均延迟（微秒）
    double getAvg() const {
        return count_ ? static_cast<double>(sum_) / count_ : 0.0;
    }

    // 获取最小值
    int64_t getMin() const { return min_; }

    // 获取最大值
    int64_t getMax() const { return max_; }

    // 获取百分位数（例如 p=0.99 表示 P99）
    int64_t getPercentile(double p) const {
        if (count_ == 0) return 0;
        if (p < 0.0) p = 0.0;
        if (p > 1.0) p = 1.0;
        return hist_.getPercentileEstimate(p);
    }

    // 清空所有统计
    void clear() {
        hist_.clear();
        sum_ = 0;
        count_ = 0;
        min_ = 0;
        max_ = 0;
    }

private:
    folly::Histogram<int64_t> hist_;
    int64_t sum_;
    uint64_t count_;
    int64_t min_;
    int64_t max_;
};