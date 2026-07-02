#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <folly/io/IOBuf.h>
#include <folly/io/IOBufQueue.h>
#include <google/protobuf/io/zero_copy_stream.h>

namespace press_test {

class IOBufAsZeroCopyOutputStream
    : public google::protobuf::io::ZeroCopyOutputStream {
 public:
  explicit IOBufAsZeroCopyOutputStream(folly::IOBufQueue* queue)
      : IOBufAsZeroCopyOutputStream(queue, 0) {}

  IOBufAsZeroCopyOutputStream(folly::IOBufQueue* queue, size_t size_hint)
      : queue_(queue),
        size_hint_(size_hint),
        byte_count_(0),
        last_next_size_(0) {}

  bool Next(void** data, int* size) override {
    size_t remaining_hint =
        size_hint_ > byte_count_ ? size_hint_ - byte_count_ : 0;
    size_t new_alloc_size = remaining_hint > 0 ? remaining_hint : kGrowth;

    auto range = queue_->preallocate(1, new_alloc_size);
    *data = range.first;
    *size = static_cast<int>(range.second);
    last_next_size_ = range.second;
    queue_->postallocate(range.second);
    byte_count_ += range.second;
    return true;
  }

  void BackUp(int count) override {
    if (count <= 0) {
      return;
    }
    size_t to_trim = static_cast<size_t>(count);
    if (to_trim > static_cast<size_t>(byte_count_)) {
      return;
    }
    queue_->trimEnd(to_trim);
    byte_count_ -= to_trim;
  }

  int64_t ByteCount() const override { return byte_count_; }

 private:
  static constexpr size_t kGrowth = 4096;

  folly::IOBufQueue* queue_;
  size_t size_hint_;
  int64_t byte_count_;
  size_t last_next_size_;
};

class IOBufAsZeroCopyInputStream
    : public google::protobuf::io::ZeroCopyInputStream {
 public:
  explicit IOBufAsZeroCopyInputStream(const folly::IOBuf* buf)
      : head_(buf),
        cur_(buf),
        offset_(0),
        byte_count_(0),
        started_(false) {}

  bool Next(const void** data, int* size) override {
    if (!head_ || !cur_) {
      return false;
    }

    if (!started_) {
      started_ = true;
      cur_ = head_;
      offset_ = 0;
    }

    if (offset_ >= cur_->length()) {
      if (cur_->isChained()) {
        cur_ = cur_->next();
        if (cur_ == head_) {
          return false;
        }
        offset_ = 0;
      } else {
        return false;
      }
    }

    *data = cur_->data() + offset_;
    *size = static_cast<int>(cur_->length() - offset_);
    byte_count_ += (cur_->length() - offset_);
    offset_ = cur_->length();
    return true;
  }

  void BackUp(int count) override {
    if (count <= 0 || byte_count_ == 0) {
      return;
    }
    size_t to_backup = static_cast<size_t>(count);

    while (to_backup > 0 && cur_ != nullptr) {
      size_t available = offset_;
      if (to_backup <= available) {
        offset_ -= to_backup;
        byte_count_ -= to_backup;
        return;
      }
      to_backup -= available;
      byte_count_ -= available;
      offset_ = 0;

      if (cur_->isChained()) {
        cur_ = cur_->prev();
        offset_ = cur_->length();
      } else {
        cur_ = head_;
        offset_ = 0;
        break;
      }
    }
  }

  bool Skip(int count) override {
    if (count <= 0) {
      return true;
    }
    size_t to_skip = static_cast<size_t>(count);

    while (to_skip > 0 && cur_ != nullptr) {
      size_t available = cur_->length() - offset_;
      if (to_skip < available) {
        offset_ += to_skip;
        byte_count_ += to_skip;
        return true;
      }
      to_skip -= available;
      byte_count_ += available;

      if (cur_->isChained()) {
        cur_ = cur_->next();
        if (cur_ == head_) {
          return false;
        }
        offset_ = 0;
      } else {
        return to_skip == 0;
      }
    }
    return to_skip == 0;
  }

  int64_t ByteCount() const override { return byte_count_; }

 private:
  const folly::IOBuf* head_;
  const folly::IOBuf* cur_;
  size_t offset_;
  int64_t byte_count_;
  bool started_;
};

} // namespace press_test