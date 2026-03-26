#ifndef __REQUEST2_H__
#define __REQUEST2_H__

#include <cstdint>
#include <iostream>

#include <folly/io/async/AsyncSocket.h>
#include <folly/portability/Sockets.h>
#include <folly/io/async/EventBase.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <glog/logging.h>

using namespace folly;
using std::unique_ptr;
using std::shared_ptr;
using std::cout;
using std::endl;

typedef unique_ptr<char, void(*)(void*)> ManagedBuffer;
typedef std::function<void(void *opaque, std::unique_ptr<IOBuf> buf)> readCallback;

struct request {
    uint32_t seq;
    uint32_t total_len;
    uint64_t requestid;
    char     buf[1024 - sizeof(uint64_t) * 2];
    request() : seq(-1), total_len(sizeof (request)), requestid(-2) { }
};
static_assert(sizeof(request) <= 4096, "struct request too large.");

class Buffer {
private:
    size_t length;
public:
    Buffer(size_t l) : length(l) {
        assert(length == sizeof(struct request));
    }

    void allocate(char **bufpp, size_t *sizep) {
        char *bufp = new char[length];
        *bufpp = bufp;
        *sizep = length;
    }

    void free(char *bufp) {
        delete[] bufp;
    }
};

class ReadCallback : public AsyncTransportWrapper::ReadCallback {
private:
    EventBase               *basep_;
    std::shared_ptr<folly::AsyncSocket> sockp_;
    std::unique_ptr<char[]> current_buffer_; // RAII 管理
    int                     fd_;
private:
    Buffer                  buffer_;
    size_t                  req_size_;
    size_t                  bytes_read_;

private:
    readCallback            cbp_;
    void                    *cbdatap_;

public:
    ReadCallback(EventBase *basep, std::shared_ptr<folly::AsyncSocket> sockp, int fd, size_t req_size) :
            basep_(basep), sockp_(sockp), fd_(fd), req_size_(req_size), buffer_(req_size) {
        this->cbp_        = nullptr;
        this->cbdatap_    = nullptr;
        this->bytes_read_ = 0;
    }

    void getReadBuffer(void **bufpp, size_t *sizep) override {
        if (!current_buffer_ || bytes_read_ >= req_size_) {
            // 分配新缓冲区
            current_buffer_ = std::make_unique<char[]>(req_size_);
            bytes_read_ = 0;
        }

        *bufpp = current_buffer_.get() + bytes_read_;
        *sizep = req_size_ - bytes_read_;
    }

    void readDataAvailable(size_t len) noexcept override {
        bytes_read_ += len;

        if (bytes_read_ >= req_size_) {
            // 转移缓冲区所有权
            auto buf = std::move(current_buffer_);
            bytes_read_ = 0;

            if (cbp_) {
                cbp_(cbdatap_,
                     IOBuf::takeOwnership(reinterpret_cast<void *>(buf.release()), req_size_, req_size_, [](void* p, void*) {
                         delete[] static_cast<char*>(p);
                     }));
            }
        }
    }

    ~ReadCallback() override {
        VLOG(1) << "ReadCallback destructor called";
    }

    void registerReadCallback(readCallback cb, void *data) {
        this->cbp_ = cb;
        this->cbdatap_ = data;
    }

    void readEOF() noexcept override {
        VLOG(1) << "Client closed connection";
        basep_->terminateLoopSoon();
    }

    void readErr(const folly::AsyncSocketException& ex) noexcept override {
        std::cout << "Read Error: " << ex.what();
        basep_->terminateLoopSoon();
    }
};
#endif
