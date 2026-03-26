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
#include <folly/experimental/io/AsyncIoUringSocket.h>
#include <folly/io/async/EventBase.h>
#include <folly/SocketAddress.h>
#include <folly/portability/Unistd.h>
#include <folly/executors/IOThreadPoolExecutor.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
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
    FreeWriteCb(uint8_t *buf = nullptr) : buf(buf) { }
    void writeSuccess() noexcept override
    {
    delete [] buf;
	delete this;
    }
    void writeErr(size_t, const AsyncSocketException &ex) noexcept override
    {
	writeSuccess();
        LOG(ERROR) << "writeErr: " << ex.what();
    }
    uint8_t *buf;
};

// Server-side read callback: echoes back whole message and appends timestamps
class EchoReadCb : public AsyncReader::ReadCallback {
public:
    EchoReadCb(AsyncIoUringSocket *sock, size_t expectLen, AsyncServerSocket *server, CPUThreadPoolExecutor *cpuPool) : sock_{sock}, expectLen_{expectLen}, server_{server}, cpuPool_{cpuPool}
    {}

    void getReadBuffer(void **bufReturn, size_t *lenReturn) override
    {
#ifdef USING_WRITE
        if (buf_.size() != sizeof (struct request)) {
            buf_.resize(sizeof (struct request));
        }
        if (bytes_read_ >= sizeof (struct request)) {
            bytes_read_ = 0;
        }
        *bufReturn = buf_.data() + bytes_read_;
        *lenReturn = buf_.size() - bytes_read_;
#else
        *bufReturn = nullptr;
        *lenReturn = 0;
#endif
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

#ifndef USING_WRITE
    bool isBufferMovable() noexcept { return true; }

    void readBufferAvailable(std::unique_ptr<IOBuf> readBuf) noexcept {
        sock_->writeChain(new FreeWriteCb, std::move(readBuf), WriteFlags::NONE);
    }
#endif

    void readDataAvailable(size_t len) noexcept override
    {
#ifdef USING_WRITE
        // std::cout << "readDataAvailable here!" << std::endl;
        bytes_read_ += len;
        if (bytes_read_ < sizeof (struct request)) {
            return;
        }

        //auto resp = IOBuf::copyBuffer(buf_.data(), sizeof (struct request));
        //sock_->writeChain(new FreeWriteCb, std::move(resp), WriteFlags::NONE);
        uint8_t *xbuf = new uint8_t[sizeof (struct request)];
        memcpy(xbuf, buf_.data(), 16);
        sock_->write(new FreeWriteCb(xbuf), xbuf, sizeof (struct request), WriteFlags::NONE);
#endif

        //// append to internal buffer
        //recvBuf_.append(buf_.data(), len);

        //// try to extract full messages; our protocol expects a 16-byte header
        //while (recvBuf_.size() >= sizeof(uint32_t) * 2 + sizeof(uint64_t)) {
        //    // header: seq(uint32), total_len(uint32), client_send_ns(uint64)
        //    uint32_t total_len;
        //    // read total_len at offset 4
        //    std::memcpy(&total_len, recvBuf_.data() + 4, sizeof(total_len));
        //    if (total_len == 0 || total_len > 10 * 1024 * 1024) {
        //        LOG(ERROR) << "server bad msgLen=" << total_len;
        //        sock_->closeNow();
        //        return;
        //    }
        //    if (recvBuf_.size() < total_len)
        //        break;

        //    // extract full message
        //    auto msg = new std::string(recvBuf_.substr(0, total_len));
        //    recvBuf_.erase(0, total_len);

        //    uint64_t server_recv = now_ns();
        //    // prepare response chain: echo original msg and append two timestamps
        //    auto resp = IOBuf::copyBuffer(msg->data(), msg->size());
        //    //uint64_t server_recv_le = server_recv;  // host order
        //    //uint64_t server_send = now_ns();

        //    // auto req = reinterpret_cast<const request *>(msg->c_str());
        //    folly::Executor* ioExecutor = sock_->getEventBase();

        //    // 2. 创建一个 Future 来封装整个异步流程
        //    makeFuture()
        //        .via(cpuPool_) // 切换到 CPU 线程池
        //        .thenValue([this](folly::Unit) {
        //                // 🚨 警告: 这是一个阻塞调用! 它会使 cpuPool_ 中的一个线程空闲 500ms
        //                std::this_thread::sleep_for(std::chrono::milliseconds(500));
        //                // 模拟计算任务结束后，返回一个 Future
        //                return folly::Unit();
        //            })

        //        // 🌟 切换到 I/O 线程池并执行 I/O (写操作)
        //        .via(ioExecutor)
        //        .thenValue([
        //            // 🌟 使用初始化捕获，安全地移动 IOBufUniquePtr 和 msg_ptr
        //            resp_to_write = std::move(resp),
        //            msg_ptr_to_free = std::move(msg),
        //            sock_ = sock_
        //        ](folly::Unit) mutable {
        //            // 现在已经在 ioExecutor 线程上，执行非阻塞的 I/O 操作
        //            sock_->writeChain(new FreeWriteCb(msg_ptr_to_free), std::move(resp_to_write));
        //        })

        //        // 错误处理
        //        .thenError(
        //            tag_t<std::exception>{},
        //            [](const std::exception& e) {
        //                LOG(ERROR) << "Async task failed: " << e.what();
        //            }
        //        );
        //    }
    }

    //bool isBufferMovable() noexcept override
    //{
    //    return true;
    //}

private:
    AsyncIoUringSocket *sock_{nullptr};
    size_t expectLen_{0};
    std::vector<char> buf_;
    CPUThreadPoolExecutor *cpuPool_;
    //std::string recvBuf_;
    AsyncServerSocket *server_{nullptr};
    size_t bytes_read_{0};

};

// std::shared_ptr<folly::CPUThreadPoolExecutor>
// Server acceptor
class EchoServer : public AsyncServerSocket::AcceptCallback {
public:
    EchoServer(EventBase *base, uint16_t port, size_t maxPacket, IOThreadPoolExecutor *ioPool, CPUThreadPoolExecutor *cpuPool) : base_(base), port_(port), maxPacket_(maxPacket), ioPool_(ioPool), cpuPool_(cpuPool)
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
                // auto sock = AsyncSocket::newSocket(evb, ns);
                auto ioSock = AsyncIoUringSocket::UniquePtr{new AsyncIoUringSocket(evb)};
                ioSock->setFd(ns);
                AsyncIoUringSocket *raw = ioSock.get();
                auto cb = std::make_unique<EchoReadCb>(raw, maxPacket_ + 64, server_.get(), cpuPool_);
                raw->setReadCB(cb.get());
                std::cout << "accept connection on evb " << evb << std::endl;
                {
                    folly::MSLGuard g(connsLock);
                    conns_.push_back(Conn{std::move(ioSock), std::move(cb)});
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
    CPUThreadPoolExecutor *cpuPool_;

    struct Conn {
        AsyncIoUringSocket::UniquePtr sock;
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
    uint16_t port = 10085;
    size_t packetSize = 200000;
    uint64_t requests = 1000;
    size_t accept_threads = 1;
    size_t io_threads = 400;
    size_t cpu_threads = 80;
    bool backendIoUring = false;
    bool iouringRegisterFd = false;
    bool iouringSendZC = false;
    size_t maxPacket = 2000000;
};

int main(int argc, char **argv)
{
    folly::init(&argc, &argv);

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
        // auto ioPool = std::make_shared<folly::IOThreadPoolExecutor>(
        //         opts.io_threads, std::make_unique<folly::NamedThreadFactory>("server-io-"));
        // std::cout << "io threads: " << opts.io_threads << std::endl;

        auto ebm = std::make_unique<EventBaseManager>([]() {
                IoUringBackend::Options ioopts;
                ioopts.setCapacity(65536)
                .setMaxSubmit(4096)
                .setUseRegisteredFds(1024)
                .setInitialProvidedBuffers(4096, 1024);
                // 如果想绑定 SQPOLL CPU，可以设置：
                // .setFlags(IoUringBackend::Options::Flags::POLL_SQ)
                // .setSQGroupName("thread1")
                // .setSQGroupNumThreads(1)
                // .setSQCpus({3});
                auto backend = std::make_unique<IoUringBackend>(ioopts);
                return std::move(backend);
        });

        auto ioPool = std::make_shared<folly::IOThreadPoolExecutor>(
                    opts.io_threads, std::make_unique<folly::NamedThreadFactory>("server-io-"), ebm.get());
        std::cout << "io threads: " << opts.io_threads << std::endl;

        auto cpuPool = std::make_shared<folly::CPUThreadPoolExecutor>(
                opts.cpu_threads, std::make_unique<folly::NamedThreadFactory>("server-cpu-"));
        std::cout << "cpu threads: " << opts.cpu_threads << std::endl;

        std::vector<std::thread> acceptThreads;
        for (int i = 0; i < opts.accept_threads; i++){
            acceptThreads.push_back(std::thread([&opts, ioPool, cpuPool, i] {
                    EventBase evb;
                    EchoServer server(&evb, opts.port, opts.maxPacket, ioPool.get(), cpuPool.get());
                    std::cout << "Server " << i << " listening on port " << opts.port << std::endl;
                    evb.loopForever();
                    }));
        }
        for (auto& t : acceptThreads) {
            t.join();
        }
        ioPool->join();
        cpuPool->join();
        return 0;
    }

}


