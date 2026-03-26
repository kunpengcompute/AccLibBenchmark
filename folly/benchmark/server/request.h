#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <cstdint>
#include <iostream>

#include <folly/io/async/AsyncSocket.h>
#include <folly/portability/Sockets.h>
#include <folly/io/async/EventBase.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <folly/experimental/io/AsyncIoUringSocket.h>
#include <folly/experimental/io/IoUringBackend.h>

using namespace folly;
using std::unique_ptr;
using std::shared_ptr;
using std::cout;
using std::endl;

typedef unique_ptr<char, void(*)(void*)> ManagedBuffer;
typedef std::function<void(void *opaque, std::shared_ptr<folly::AsyncIoUringSocket>, ManagedBuffer, size_t)> readCallback;
typedef std::function<void(std::shared_ptr<folly::AsyncIoUringSocket>, ManagedBuffer, size_t)> writeCallback;

class EventBaseProvider {
 public:
  enum Type {
    DEFAULT,
    IO_URING,
  };

  static std::unique_ptr<EventBase> getEventBase(
      Type type, size_t capacity = 32 * 1024) {
    switch (type) {
      case DEFAULT: {
        return std::make_unique<EventBase>();
      }
      case IO_URING: {
        try {
          IoUringBackend::Options opts;
	  		opts.setCapacity(capacity).setMaxSubmit(256).setInitialProvidedBuffers(200000000, 1);

          auto factory = [opts] {
            return std::make_unique<IoUringBackend>(opts);
          };
          return std::make_unique<EventBase>(
              EventBase::Options().setBackendFactory(
                  std::move(factory)));
        } catch (const IoUringBackend::NotAvailable&) {
          return nullptr;
        }
      }
    }

    CHECK(false);
    return nullptr;
  }
};

struct request {
        uint32_t seq;
        uint32_t total_len;
        uint64_t requestid;
        char     buf[1024-sizeof(uint64_t)*2];
        request() : seq(-1), total_len(sizeof (request)), requestid(-2) { }
};
static_assert(sizeof(request) == 1024, "struct request too large.");

class Buffer {
private:
	size_t length;
public:
	Buffer(size_t l) : length(l) {
		assert(length == sizeof(struct request));
	}

	void allocate(char **bufpp, size_t *sizep) {
		char *bufp = static_cast<char *>(malloc(length));
		assert(bufp);
		*bufpp = bufp;
		*sizep = length;
	}

	void free(char *bufp) {
		::free(bufp);
	}
};

class ReadCallback : public AsyncTransportWrapper::ReadCallback {
private:
	EventBase               *basep_;
	std::shared_ptr<folly::AsyncIoUringSocket>               sockp_;
	int                     fd_;
private:
	Buffer                  buffer_;
	size_t                  req_size_;
	char                    *raw_bufp_;
	size_t                  bytes_read_;

private:
	readCallback            cbp_;
	void                    *cbdatap_;

public:
	ReadCallback(EventBase *basep, std::shared_ptr<folly::AsyncIoUringSocket> sockp, int fd, size_t req_size) :
			basep_(basep), sockp_(sockp), fd_(fd), req_size_(req_size), buffer_(req_size) {
		// cout << "ReadCallback instantiated.\n";
		this->cbp_        = nullptr;
		this->cbdatap_    = nullptr;
		this->bytes_read_ = 0;
		this->raw_bufp_   = nullptr;
	}

	void getReadBuffer(void **bufpp, size_t *sizep) override {
		if (bytes_read_ >= req_size_ || !this->raw_bufp_) {
			size_t size;
			char   *bufp;

			buffer_.allocate(&bufp, &size);
			assert(size == req_size_);
			this->raw_bufp_   = bufp;
			this->bytes_read_ = 0;
		}
		assert(this->raw_bufp_);

		*bufpp = this->raw_bufp_ + bytes_read_;
		*sizep = this->req_size_ - bytes_read_;
	}

	void readDataAvailable(size_t len) noexcept override {
		assert(cbp_);

		bytes_read_ += len;
		assert(bytes_read_ <= req_size_);

		if (bytes_read_ >= req_size_) {
			assert(bytes_read_ == req_size_);

			char *bufp = raw_bufp_;
			this->raw_bufp_   = nullptr;
			this->bytes_read_ = 0;
			ManagedBuffer mbufp(reinterpret_cast<char*>(bufp), ::free);
			if (cbp_) {
				cbp_(cbdatap_, sockp_, std::move(mbufp), req_size_);
			}
		}
	}

	void registerReadCallback(readCallback cb, void *data) {
		// cout << "Reagistering ReadCallback instantiated.\n";
		this->cbp_ = cb;
		this->cbdatap_ = data;
	}

	void readEOF() noexcept override {
		// cout << "client closed connection.\n";
		basep_->terminateLoopSoon();
	}

	void readErr(const folly::AsyncSocketException& ex) noexcept override {
		// cout << "Connect Error: " << ex.what() << endl; 
		// cout << "Read Error closing connection.\n";
		basep_->terminateLoopSoon();
	}
};
#endif
