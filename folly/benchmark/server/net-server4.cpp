// net_bench_single.cpp
// Single-file benchmark tool for comparing AsyncSocket (epoll) client
// vs AsyncIoUringSocket (io_uring) client with a single AsyncSocket server.
//
// Usage (examples printed below in the companion chat message):
//  - Build with folly and optionally liburing installed.
//  - Run server on machine A:
//      ./net_bench --mode server --port 9000 --max-packet 2000000
//  - Run client(s) on machine B (AsyncSocket):
//      ./net_bench --mode client --server 10.0.0.1 --port 9000 --backend async --threads 20 --requests 1000
//      --packet-size 200000
//  - Run client(s) on machine B (IoUring client):
//      ./net_bench --mode client --server 10.0.0.1 --port 9000 --backend iouring --threads 20 --requests 1000
//      --packet-size 200000 --iouring_register_fd 1 --iouring_sendzc 0
#ifndef IORING_CQE_F_MORE
#define IORING_CQE_F_MORE 0
#endif
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <folly/Conv.h>
#include <folly/FileUtil.h>
#include <folly/Format.h>
#include <folly/init/Init.h>
#include <folly/io/IOBuf.h>
#include <future>
#include <folly/io/async/AsyncServerSocket.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/EventBase.h>
#include <folly/SocketAddress.h>
#include <folly/portability/Unistd.h>
#include <folly/executors/IOThreadPoolExecutor.h>
#include <folly/synchronization/MicroSpinLock.h>

#include "request.h"

using namespace folly;
using namespace std::chrono_literals;

static uint64_t now_ns()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string toString(const std::unique_ptr<IOBuf> &buf)
{
    if (!buf)
        return std::string();
    auto c = buf->coalesce();
    return std::string(reinterpret_cast<const char *>(c.data()), c.size());
}

// Simple null write callback
struct NullWriteCb : public AsyncWriter::WriteCallback {
    void writeSuccess() noexcept override
    {}
    void writeErr(size_t, const AsyncSocketException &ex) noexcept override
    {
        LOG(ERROR) << "writeErr: " << ex.what();
    }
};
static NullWriteCb nullWriteCb;

struct FreeWriteCb : public AsyncWriter::WriteCallback {
    FreeWriteCb(std::string *msg) : msg(msg) { }
    void writeSuccess() noexcept override
    {
        delete msg;
        delete this;
    }
    void writeErr(size_t, const AsyncSocketException &ex) noexcept override
    {
        writeSuccess();
        LOG(ERROR) << "writeErr: " << ex.what();
    }
    std::string *msg;
};

//pthread_mutex_t xmutex = PTHREAD_MUTEX_INITIALIZER;

// Server-side read callback: echoes back whole message and appends timestamps
class EchoReadCb : public AsyncReader::ReadCallback {
public:
    EchoReadCb(AsyncSocket *sock, size_t expectLen, AsyncServerSocket *server) : sock_{sock}, expectLen_{expectLen}, server_{server}
    {}

    void getReadBuffer(void **bufReturn, size_t *lenReturn) override
    {
        if (!buf_.size())
            buf_.resize(expectLen_ + 64);
        *bufReturn = buf_.data();
        *lenReturn = buf_.size();
    }

    void readEOF() noexcept override
    {
        //LOG(ERROR) << "server readEOF";
        sock_->closeNow();
        //std::cout << "resume on readEOF" << std::endl;
        //server_->startAccepting();
    }
    void readErr(const AsyncSocketException &ex) noexcept override
    {
        LOG(ERROR) << "server readErr " << ex.what();
        sock_->closeNow();
        //std::cout << "resume on readErr" << std::endl;
        //server_->startAccepting();
    }

    //size_t forwards = 0;

    void readDataAvailable(size_t len) noexcept override
    {
        // append to internal buffer
        recvBuf_.append(buf_.data(), len);

        // try to extract full messages; our protocol expects a 16-byte header
        while (recvBuf_.size() >= sizeof(uint32_t) * 2 + sizeof(uint64_t)) {
            // header: seq(uint32), total_len(uint32), client_send_ns(uint64)
            uint32_t total_len;
            // read total_len at offset 4
            std::memcpy(&total_len, recvBuf_.data() + 4, sizeof(total_len));
            if (total_len != 1024) {
                LOG(ERROR) << "server bad msgLen=" << total_len;
                sock_->closeNow();
                return;
            }
            if (recvBuf_.size() < total_len)
                break;
            //pthread_mutex_lock(&xmutex);
            //forwards++;
            //std::cout << this << " forwards " << forwards << std::endl;;
            //pthread_mutex_unlock(&xmutex);
            //if ((forwards % 10000) == 0) {
            //    printf("%p forwards %d\n", this, forwards);
            //}

            // extract full message
            auto msg = new std::string(recvBuf_.substr(0, total_len));
            recvBuf_.erase(0, total_len);

            uint64_t server_recv = now_ns();
            // prepare response chain: echo original msg and append two timestamps
            auto resp = IOBuf::copyBuffer(msg->data(), msg->size());
            //uint64_t server_recv_le = server_recv;  // host order
            //uint64_t server_send = now_ns();

            auto req = reinterpret_cast<const request *>(msg->c_str());
            //reinterpret_cast<uint64_t *>(req->buf)[0] = server_recv;
            //reinterpret_cast<uint64_t *>(req->buf)[1] = server_send;
            //std::cout << "request seq=" << req->seq << " total=" << req->total_len << " msgsize=" << msg->size() << " reqid=" << req->requestid << std::endl;
            //std::array<uint64_t, 2> times = {server_recv_le, server_send};
            //auto times_buf = IOBuf::copyBuffer(reinterpret_cast<const void *>(times.data()), sizeof(times));
            //resp->appendToChain(std::move(times_buf));

            // send back (AsyncSocket writeChain with 2 args)
            //sock_->writeChain(&nullWriteCb, std::move(resp));
            sock_->writeChain(new FreeWriteCb(msg), std::move(resp));
        }
    }

    bool isBufferMovable() noexcept override
    {
        return true;
    }

private:
    AsyncSocket *sock_{nullptr};
    size_t expectLen_{0};
    std::vector<char> buf_;
    std::string recvBuf_;
    AsyncServerSocket *server_{nullptr};
};

// Server acceptor
class EchoServer : public AsyncServerSocket::AcceptCallback {
public:
    EchoServer(EventBase *base, uint16_t port, size_t maxPacket, IOThreadPoolExecutor *ioPool) : base_(base), port_(port), maxPacket_(maxPacket), ioPool_(ioPool)
    {
        // AsyncServerSocket::newSocket returns a shared_ptr in this folly version
        connsLock.init();
        server_ = AsyncServerSocket::newSocket(base_);
        server_->setTFOEnabled(true, 1);
        server_->setReusePortEnabled(true);
        server_->bind(port_);
        server_->listen(1024);
        server_->addAcceptCallback(this, nullptr);
        server_->startAccepting();
        server_->getAddress(&addr_);
    }

    void connectionAccepted(NetworkSocket ns, const SocketAddress &, AcceptInfo) noexcept override
    {
        // create AsyncSocket::UniquePtr for this connection
        auto evb = ioPool_->getEventBase();
        evb->runInEventBaseThread([evb, ns, this] {
                auto sock = AsyncSocket::newSocket(evb, ns);
                AsyncSocket *raw = sock.get();
                auto cb = std::make_unique<EchoReadCb>(raw, maxPacket_ + 64, server_.get());
                raw->setReadCB(cb.get());
                std::cout << "accept connection on evb " << evb << std::endl;
                {
                    folly::MSLGuard g(connsLock);
                    conns_.push_back({std::move(sock), std::move(cb)});
                }
                });
        //server_->pauseAccepting();
        //std::cout << "accept and paused" << std::endl;
    }

    void acceptError(const std::exception &ex) noexcept override
    {
        LOG(ERROR) << "acceptError: " << ex.what();
    }

private:
    EventBase *base_{nullptr};
    uint16_t port_{0};
    size_t maxPacket_{0};
    // server_ stored as shared_ptr because folly v2022 uses shared ownership
    std::shared_ptr<AsyncServerSocket> server_;
    SocketAddress addr_;
    IOThreadPoolExecutor *ioPool_;

    struct Conn {
        AsyncSocket::UniquePtr sock;
        std::unique_ptr<EchoReadCb> cb;
    };
    folly::MicroSpinLock connsLock;
    std::vector<Conn> conns_;
};

// Simple connect callback with promise
struct ConnectPromiseCb : public AsyncSocket::ConnectCallback {
    std::promise<void> prom;
    void connectSuccess() noexcept override
    {
        prom.set_value();
    }
    void connectErr(const AsyncSocketException &ex) noexcept override
    {
        prom.set_exception(std::make_exception_ptr(std::runtime_error(ex.what())));
    }
};


// Minimal CLI parsing
struct CmdOpts {
    std::string mode = "server";
    std::string server = "127.0.0.1";
    uint16_t port = 9091;
    size_t packetSize = 200000;
    uint64_t requests = 1000;
    size_t accept_threads = 1;
    size_t io_threads = 1;
    bool backendIoUring = false;
    bool iouringRegisterFd = false;
    bool iouringSendZC = false;
    size_t maxPacket = 2000000;
};

int main(int argc, char **argv)
{
    //folly::init(&argc, &argv);

    CmdOpts opts;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--mode")
            opts.mode = argv[++i];
        else if (a == "--server")
            opts.server = argv[++i];
        else if (a == "--port")
            opts.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (a == "--packet-size")
            opts.packetSize = std::stoul(argv[++i]);
        else if (a == "--requests")
            opts.requests = std::stoull(argv[++i]);
        else if (a == "--accept-threads")
            opts.accept_threads = std::stoul(argv[++i]);
        else if (a == "--io-threads")
            opts.io_threads = std::stoul(argv[++i]);
        else if (a == "--backend") {
            std::string v = argv[++i];
            opts.backendIoUring = (v == "iouring");
        } else if (a == "--iouring_register_fd") {
            opts.iouringRegisterFd = std::stoi(argv[++i]) != 0;
        } else if (a == "--iouring_sendzc") {
            opts.iouringSendZC = std::stoi(argv[++i]) != 0;
        } else if (a == "--max-packet") {
            opts.maxPacket = std::stoul(argv[++i]);
        }
    }

    if (opts.mode == "server") {
        auto ioPool = std::make_shared<folly::IOThreadPoolExecutor>(
                opts.io_threads, std::make_unique<folly::NamedThreadFactory>("server-io-"));
        std::cout << "io threads: " << opts.io_threads << std::endl;

        std::vector<std::thread> acceptThreads;
        for (int i = 0; i < opts.accept_threads; i++){
            acceptThreads.push_back(std::thread([&opts, ioPool, i] {
                    EventBase evb;
                    EchoServer server(&evb, opts.port, opts.maxPacket, ioPool.get());
                    std::cout << "Server " << i << " listening on port " << opts.port << std::endl;
                    evb.loopForever();
                    }));
        }
        for (auto& t : acceptThreads) {
            t.join();
        }
        ioPool->join();
        return 0;
    }

}
