#ifndef PERCENTILE_CALCULATOR_
#define PERCENTILE_CALCULATOR_

#include <vector>
#include <cstdint>

class PercentileCalculator {
public:
    PercentileCalculator(size_t max_samples = 100000) : max_samples_(max_samples) {}
    
    void addSample(uint64_t latency_us) {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.push_back(latency_us);
        
        // 控制样本数量，避免内存无限增长
        if (samples_.size() > max_samples_) {
            // 保留最新的样本，移除最老的样本
            samples_.erase(samples_.begin(), samples_.begin() + (samples_.size() - max_samples_));
        }
    }
    
    bool calculatePercentiles(double& p50, double& p95, double& p99, double& p999, 
                             double& min_val, double& max_val) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (samples_.empty()) {
            return false;
        }
        
        std::vector<uint64_t> sorted_samples = samples_;
        std::sort(sorted_samples.begin(), sorted_samples.end());
        
        min_val = static_cast<double>(sorted_samples.front());
        max_val = static_cast<double>(sorted_samples.back());
        
        p50 = getPercentile(sorted_samples, 0.5);
        p95 = getPercentile(sorted_samples, 0.95);
        p99 = getPercentile(sorted_samples, 0.99);
        p999 = getPercentile(sorted_samples, 0.999);
        
        return true;
    }
    
    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return samples_.size();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.clear();
    }
    
private:
    double getPercentile(const std::vector<uint64_t>& sorted_samples, double percentile) {
        if (sorted_samples.empty()) return 0.0;
        
        double index = percentile * (sorted_samples.size() - 1);
        size_t lower_index = static_cast<size_t>(std::floor(index));
        size_t upper_index = static_cast<size_t>(std::ceil(index));
        
        if (lower_index == upper_index) {
            return static_cast<double>(sorted_samples[lower_index]);
        }
        
        // 线性插值
        double weight = index - lower_index;
        return (1 - weight) * sorted_samples[lower_index] + weight * sorted_samples[upper_index];
    }
    
    std::vector<uint64_t> samples_;
    mutable std::mutex mutex_;
    size_t max_samples_;
};

#endif

