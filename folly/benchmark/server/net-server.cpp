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

#include "request.h"

using namespace folly;
using namespace std::chrono_literals;

struct FreeWriteCb : public AsyncWriter::WriteCallback {
    FreeWriteCb() { }
    void writeSuccess() noexcept override
    {
	delete this;
    }
    void writeErr(size_t, const AsyncSocketException &ex) noexcept override
    {
	writeSuccess();
        LOG(ERROR) << "writeErr: " << ex.what();
    }
};

// Server-side read callback: echoes back whole message and appends timestamps
class EchoReadCb : public AsyncReader::ReadCallback {
public:
    EchoReadCb(AsyncSocket *sock, size_t expectLen, std::shared_ptr<AsyncServerSocket> server) : sock_{sock}, expectLen_{expectLen}, server_(server)
    {}

    void getReadBuffer(void **bufReturn, size_t *lenReturn) override
    {
        if (buf_.size() != sizeof (struct request)) {
            buf_.resize(sizeof (struct request));
        }
        if (bytes_read_ >= sizeof (struct request)) {
            bytes_read_ = 0;
        }
        *bufReturn = buf_.data() + bytes_read_;
        *lenReturn = buf_.size() - bytes_read_;
    }

    void readEOF() noexcept override
    {
        sock_->closeNow();
        std::cout << "resume on readEOF for server_: " << server_ << std::endl;
        server_->startAccepting();
    }
    void readErr(const AsyncSocketException &ex) noexcept override
    {
        LOG(ERROR) << "server readErr " << ex.what();
        sock_->closeNow();
        std::cout << "resume on readErr for server_: " << server_ << std::endl;
        server_->startAccepting();
    }

    void readDataAvailable(size_t len) noexcept override
    {
        bytes_read_ += len;
        if (bytes_read_ < sizeof (struct request)) {
            return;
        }

        auto resp = IOBuf::copyBuffer(buf_.data(), sizeof (struct request));
        sock_->writeChain(new FreeWriteCb, std::move(resp));

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

        //    auto resp = IOBuf::copyBuffer(msg->data(), msg->size());

        //    auto req = reinterpret_cast<const request *>(msg->c_str());

        //    sock_->writeChain(new FreeWriteCb(msg), std::move(resp));
        //}
    }

    //bool isBufferMovable() noexcept override
    //{
    //    return true;
    //}

private:
    AsyncSocket *sock_{nullptr};
    std::shared_ptr<AsyncServerSocket> server_;
    size_t expectLen_{0};
    std::vector<char> buf_;
    size_t bytes_read_{0};
};

// Server acceptor
class EchoServer : public AsyncServerSocket::AcceptCallback {
public:
    EchoServer(EventBase *base, uint16_t port, size_t maxPacket, std::shared_ptr<AsyncServerSocket> server) : base_(base), port_(port), maxPacket_(maxPacket), server_(server)
    {
        // ! main() 函数 for 循环已经做了
        // AsyncServerSocket::newSocket returns a shared_ptr in this folly version
        // server_ = AsyncServerSocket::newSocket(base_);
        // server_->setTFOEnabled(true, 1);
        // server_->bind(port_);
        // server_->listen(1024);
        // server_->addAcceptCallback(this, nullptr);
        // server_->startAccepting();
        // server_->getAddress(&addr_);
    }

    void connectionAccepted(NetworkSocket ns, const SocketAddress &, AcceptInfo) noexcept override
    {
        // create AsyncSocket::UniquePtr for this connection
        auto sock = AsyncSocket::newSocket(base_, ns);
        AsyncSocket *raw = sock.get();
        auto cb = std::make_unique<EchoReadCb>(raw, maxPacket_ + 64, server_);
        raw->setReadCB(cb.get());
        conns_.push_back({std::move(sock), std::move(cb)});
	    // std::cout << "accept" << std::endl;
        server_->pauseAccepting();
        std::cout << "accept and paused server_: " << server_ << std::endl;
    }

    void acceptError(const std::exception &ex) noexcept override
    {
        LOG(ERROR) << "acceptError: " << ex.what();
    }

private:
    // std::vector<EventBase *> &base_;
    EventBase *base_{nullptr};
    // size_t thread_index_{0};
    uint16_t port_{0};
    size_t maxPacket_{0};
    // server_ stored as shared_ptr because folly v2022 uses shared ownership
    std::shared_ptr<AsyncServerSocket> server_;
    SocketAddress addr_;

    struct Conn {
        AsyncSocket::UniquePtr sock;
        std::unique_ptr<EchoReadCb> cb;
    };
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
    uint16_t port = 10016;
    uint16_t io_threads = 12; // "Number of IO threads"
    size_t packetSize = 200000;
    uint64_t requests = 1000;
    size_t threads = 1;
    bool backendIoUring = false;
    bool iouringRegisterFd = false;
    bool iouringSendZC = false;
    size_t maxPacket = 2000000;
};

DEFINE_string(server_ip, "0.0.0.0", "Server IP Address");
DEFINE_int32(server_port, 12345, "Server Port");
DEFINE_int32(conn_num, 80, "Number of connections");
DEFINE_int32(conn_per_thread, 80, "Number of io_threads");
DEFINE_int32(qdepth, 64, "Per-connection queue depth");

void print_args(int argc, char** argv, const std::string& label) {
    std::cout << "--- " << label << " ---" << std::endl;
    std::cout << "argc: " << argc << std::endl;
    std::cout << "argv content:" << std::endl;
    for (int i = 0; i < argc; ++i) {
        std::cout << "  argv[" << i << "]: \"" << (argv[i] ? argv[i] : "NULL") << "\"" << std::endl;
    }
    // 注意：在 argc 减少的情况下，argv[argc] 之后的内容是未定义的，通常为 NULL，
    // 但在 C/C++ 标准中，argv[argc] 总是 NULL。
}

int main(int argc, char **argv)
{
    // print_args(argc, argv, "Before folly::init");

    folly::init(&argc, &argv);

    // print_args(argc, argv, "After folly::init");

    CmdOpts opts;

    opts.io_threads = FLAGS_conn_per_thread;
    opts.server = FLAGS_server_ip;
    opts.port = FLAGS_server_port;

    // for (int i = 1; i < argc; ++i) {
    //     std::string a = argv[i];
    //     if (a == "--mode")
    //         opts.mode = argv[++i];
    //     else if (a == "--server"){
    //         cout << "find server!" << endl;
    //         opts.server = argv[++i];
    //     }
    //     else if (a == "--port"){
    //         cout << "find port!" << endl;
    //         opts.port = static_cast<uint16_t>(std::stoi(argv[++i]));
    //     }
    //     else if (a == "--io_threads"){
    //         cout << "find io_threads!" << endl;
    //         opts.io_threads = static_cast<uint16_t>(std::stoi(argv[++i]));
    //     }
    //     else if (a == "--packet-size")
    //         opts.packetSize = std::stoul(argv[++i]);
    //     else if (a == "--requests")
    //         opts.requests = std::stoull(argv[++i]);
    //     else if (a == "--threads")
    //         opts.threads = std::stoul(argv[++i]);
    //     else if (a == "--backend") {
    //         std::string v = argv[++i];
    //         opts.backendIoUring = (v == "iouring");
    //     } else if (a == "--iouring_register_fd") {
    //         opts.iouringRegisterFd = std::stoi(argv[++i]) != 0;
    //     } else if (a == "--iouring_sendzc") {
    //         opts.iouringSendZC = std::stoi(argv[++i]) != 0;
    //     } else if (a == "--max-packet") {
    //         opts.maxPacket = std::stoul(argv[++i]);
    //     }
    // }

    if (opts.mode == "server") {
        std::vector<EventBase *> eventBases;
        std::vector<std::thread> threads;
        std::vector<std::shared_ptr<AsyncServerSocket>> serverSockets;

        eventBases.reserve(opts.io_threads);
        serverSockets.reserve(opts.io_threads);

        for (int i = 0; i < opts.io_threads; ++i) {
            EventBase* eventBase = new EventBase();
            eventBases.push_back(eventBase);

            auto serverSocket = AsyncServerSocket::newSocket(eventBase);
            serverSocket->setReusePortEnabled(true);
            serverSocket->bind(opts.port);

            // 优化的配置参数
            serverSocket->setQueueTimeout(std::chrono::milliseconds(1000));
            serverSocket->setMaxNumMessagesInQueue(8192);
            serverSocket->setMaxAcceptAtOnce(128);  // 减少批量处理数量
            serverSocket->listen(1024);

            auto acceptCb = new EchoServer(eventBases[i], opts.port, opts.maxPacket, serverSocket);

            serverSocket->addAcceptCallback(acceptCb, eventBase);
            serverSockets.push_back(serverSocket);

            threads.emplace_back([eventBase, serverSocket, i] {
                LOG(INFO) << "Thread " << i << " starting";
                serverSocket->startAccepting();
                eventBase->loopForever();
            });
        }

        LOG(INFO) << "Server started on port " << opts.port
                << " with " << opts.io_threads << " threads";

        std::cout << "Server started on port " << opts.port << " with " << opts.io_threads << " threads" << std::endl;

        for (auto &t : threads) {
            t.join();
        }

        for (auto ev : eventBases) delete ev;
        return 0;

        // EventBase evb;
        // EchoServer server(&evb, opts.port, opts.maxPacket);
        // std::cout << "Server listening on port " << opts.port << std::endl;
        // evb.loopForever();
        // return 0;
    }

}
