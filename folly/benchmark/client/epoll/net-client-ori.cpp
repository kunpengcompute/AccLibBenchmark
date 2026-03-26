#include <iostream>
#include <cstdlib>
#include <future>
#include <chrono>
#include <map>
#include <vector>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <memory>
#include <thread>
#include <numeric>

#include <folly/io/async/AsyncSocket.h>
#include <folly/portability/Sockets.h>
#include <folly/io/async/EventBase.h>
#include <folly/init/Init.h>
#include <folly/io/IOBuf.h>
#include <folly/io/IOBufQueue.h>
#include <folly/io/Cursor.h>
#include <glog/logging.h>

#include "request-ori.h"
#include "../common/percentile.h"

using namespace folly;
using std::cout;
using std::endl;
using std::lock_guard;
using std::map;
using std::mutex;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_ptr;
using std::vector;

struct BenchStats {
    std::atomic<int> started {0};
};

#define MILLSEC_TO_SEC(nsec) ((nsec) / 1000.0)
#define BYTES_TO_MB(bytes) ((bytes) / 1024.0 / 1024.0)
static unsigned short active_thread_connection[4096] = {0};
void requestRead(void *opaque, std::unique_ptr<IOBuf> buf);
std::mutex output_mutex;
class Benchmark {
public:
    std::shared_ptr<EventBase> evb_;
    static std::vector<std::vector<std::chrono::time_point<std::chrono::system_clock>>> request_start_time;
    static std::vector<std::vector<std::chrono::time_point<std::chrono::system_clock>>> request_end_time;
private:
    shared_ptr<AsyncSocket> socket_;
    int conn_id_;
    int thread_id_;
    int qdepth_;

    uint32_t req_sent;
    int max_requests;  // 每连接总请求数（保持不变）
    size_t received_bytes;
    static std::atomic<int> active_connections_;
    static int total_connections_;
    static std::atomic<uint64_t> total_bandwidth_;
    unsigned short total_thread_connect;
    int target_qps_per_conn;
#ifdef CHECK_RESPONSE
    request lastreq;
    size_t nextReqid = 1;
    size_t sentreqs = 0;
#endif

    SocketAddress address_;
    std::unique_ptr<ReadCallback> rcb_;
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point end_time;

    std::atomic<uint64_t> request_id;

    uint64_t tot_req_latency;
    uint64_t tot_write_latency;
    uint64_t tot_read_latency;
    uint64_t conn_bandwidth;
    bool PINGPANG_MODE{false};
    static double sum_d;
    static double max_d;

public:
    static void setTotalConnections(int total)
    {
        total_connections_ = total;
        active_connections_ = 0;
        total_bandwidth_ = 0;
    }

    static void initRequestTimer(int num_connects, int num_requests) {
        request_start_time.resize(num_connects);
        for (int i = 0; i < request_start_time.size(); ++i) {
            request_start_time[i].resize(num_requests);
        }

        request_end_time.resize(num_connects);
        for (int i = 0; i < request_end_time.size(); ++i) {
            request_end_time[i].resize(num_requests);
        }
    }

    static void timer_process() {
        int num_connects = request_start_time.size();
        int num_requests = request_start_time[0].size();

        std::vector<std::vector<double>> duration(num_connects, std::vector<double>(num_requests));

        for (int connect_number = 0; connect_number < num_connects; ++connect_number) {
            for (int _request_id = 0; _request_id < num_requests; ++_request_id) {
                duration[connect_number][_request_id] = 
                    std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(
                        request_end_time[connect_number][_request_id] - request_start_time[connect_number][_request_id]
                    ).count();
            }
        }

        std::vector<double> avg_duration(num_connects);
        for (int i = 0; i < num_connects; ++i) {
            avg_duration[i] = std::accumulate(duration[i].begin(), duration[i].end(), (double)0.0) / num_requests;
            PercentileCalculator pc(duration[i].size());
            for (auto it = duration[i].begin(); it != duration[i].end(); ++it) {
                pc.addSample(*it);
            }
            double p50, p95, p99, p999, minv, maxv;
            if (pc.calculatePercentiles(p50, p95, p99, p999, minv, maxv)) {
                std::cout << "p50: " << p50 << std::endl
                    << "p95: " << p95 << std::endl
                    << "p99: " << p99 << std::endl
                    << "min: " << minv << std::endl
                    << "max: " << maxv << std::endl;
            }
        }

        std::cout << "total average:" << std::accumulate(avg_duration.begin(), avg_duration.end(), (double)0.0) / num_connects << std::endl;
    }

    static uint64_t getTotalBandwidth()
    {
        return total_bandwidth_;
    }

    class WriteCallback : public AsyncTransportWrapper::WriteCallback {
    public:
        struct request req_;
        Benchmark *benchp_;

        static WriteCallback* create(Benchmark *bencp, int reqid)
        {
            auto cb = new WriteCallback;
            cb->req_.requestid = reqid;
            cb->benchp_ = bencp;
            return cb;
        }
        void writeSuccess() noexcept override
        {
            benchp_->request_sent(&req_);
        }
        void writeErr(size_t /*bytesWritten*/, const AsyncSocketException &ex) noexcept override
        {
            LOG(ERROR) << "Write error: " << ex.what();
        }
    };

    class ConnectCallback : public AsyncSocket::ConnectCallback {
    private:
        Benchmark *bencp_;

    public:
        ConnectCallback(Benchmark *bencp) : bencp_(bencp)
        {}
        virtual void connectSuccess() noexcept override
        {
            bencp_->rcb_ =
                std::make_unique<ReadCallback>(bencp_->evb_.get(), bencp_->socket_, 0, sizeof(struct request));
            bencp_->rcb_->registerReadCallback(requestRead, bencp_);
            bencp_->socket_->setReadCB(bencp_->rcb_.get());
            bencp_->bs.started++;
        }
        virtual void connectErr(const AsyncSocketException &ex) noexcept override
        {
            LOG(ERROR) << "Connection error: " << ex.what();
            bencp_->bs.started++;
            bencp_->stop_benchmark();
        }
    };

    Benchmark(std::string server, int port, int thread_id, int conn_id, int qdepth, std::shared_ptr<EventBase> evb, int conn_num, int total_requests, BenchStats &bs, bool PINGPANG_MODE, int _target_qps_per_conn)
        : thread_id_(thread_id), conn_id_(conn_id), qdepth_(qdepth), evb_(evb), total_thread_connect(conn_num), max_requests(total_requests), bs(bs), PINGPANG_MODE(PINGPANG_MODE), target_qps_per_conn(_target_qps_per_conn)
    {   address_ = SocketAddress(server, port);
        req_sent = 0;
        received_bytes = 0;
        request_id = 0;
        tot_req_latency = 0;
        tot_write_latency = 0;
        tot_read_latency = 0;
        conn_bandwidth = 0;
    }

    ~Benchmark()
    {
        if (socket_) {
            socket_->close();
        }
    }

    void run()
    {
        socket_ = AsyncSocket::newSocket(evb_.get());
        ccb_ = std::make_shared<ConnectCallback>(this);
        socket_->connect(ccb_.get(), address_);
    }

    void start_benchmark()
    {
        start_time = std::chrono::high_resolution_clock::now();
        if (PINGPANG_MODE) requests_send(1);
    }

    void stop_benchmark()
    {
        end_time = std::chrono::high_resolution_clock::now();
        double d = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        auto seconds = MILLSEC_TO_SEC(d);

        // Calculate bandwidths
        uint64_t total_bytes = max_requests * sizeof(struct request);
        double write_bw = BYTES_TO_MB(total_bytes) / seconds;
        double read_bw = BYTES_TO_MB(total_bytes) / seconds;
        double total_bw = BYTES_TO_MB(total_bytes * 2) / seconds;  // Both write and read

        conn_bandwidth = total_bw;
        total_bandwidth_ += total_bw;
        sum_d += d;
        max_d = std::max(d, max_d);
        // 输出表格格式的结果
        {
            lock_guard<mutex> lock(output_mutex);
            // 表头（只在第一次输出时打印）
            static bool header_printed = false;
            if (!header_printed) {
                cout << std::left << "     "
                     << std::setw(8) << "Thread" 
                     << std::setw(8) << "Conn" 
                     << std::setw(12) << "Requests" 
                     << std::setw(15) << "Time(ms)" 
                     << std::setw(15) << "Avg Lat(ms)" 
                     << std::setw(15) << "Write BW(MB/s)" 
                     << std::setw(15) << "Read BW(MB/s)"
                     << std::setw(15) << "Total BW(MB/s)" 
                     << std::setw(12) << "accept_id" << endl;
                header_printed = true;
            }
            uint32_t req_completed = received_bytes / sizeof (request);
            // 表格数据
            cout << std::left << "data: "
                 << std::setw(8) << thread_id_ 
                 << std::setw(8) << conn_id_ 
                 << std::setw(12) << max_requests 
                 << std::setw(15) << d 
                 << std::setw(15) << sum_d / (++active_connections_)
                 << std::setw(15) << std::fixed
                 << std::setprecision(2) << write_bw 
                 << std::setw(15) << std::fixed 
                 << std::setprecision(2) << read_bw
                 << std::setw(15) << std::fixed 
                 << std::setprecision(2) << total_bw 
                 << std::setw(12) << active_connections_ << endl;
        }

        if (++active_thread_connection[thread_id_] == total_thread_connect) {
            evb_->terminateLoopSoon();
        }

        if (active_connections_ == total_connections_) {
            cout << "================================================================================" << endl;
            cout << "AVERAGE LATENCY: " << sum_d / total_connections_ << " ms" << endl;
            cout << "MAXIMUN LATENCY: " << max_d << " ms" << endl;
            cout << "TARGET  QPS: " << target_qps_per_conn << endl;
            cout << "CURRENT QPS: " << 1000.0 * max_requests / max_d << endl;
            cout << "================================================================================" << endl;
        }
    }

    void requests_send(uint32_t nrequests)
    {
        for (uint32_t i = 0; i < nrequests && req_sent < max_requests; i++) {
            request_id++;
            auto wcb = WriteCallback::create(this, request_id);
            WriteFlags flag = WriteFlags::NONE;
            socket_->write(wcb, reinterpret_cast<char *>(&wcb->req_), sizeof(request), flag);
            req_sent++;
        }
    }

    void request_sent(struct request *reqp) {
#ifdef CHECK_RESPONSE
        sentreqs++;
#endif
    }

#ifdef CHECK_RESPONSE
    bool check(size_t len, IOBuf *buf)
    {
        folly::io::Cursor cur(buf);
        size_t reqSkip = received_bytes % sizeof (request);
        size_t bufSkip;
        size_t needed;
        if (reqSkip < 16) {
            // last part is not fully checked
            bufSkip = 0;
            needed = 16 - reqSkip;
        } else {
            bufSkip = sizeof (request) - reqSkip;
            needed = 16;
            reqSkip = 0;
        }
        while (true) {
            auto skiped = cur.skipAtMost(bufSkip);
            if (skiped != bufSkip) {
                // part of req, wait the next one
                return true;
            }
            auto last = cur.pullAtMost((char *)&lastreq + reqSkip, needed);
            if (last != needed) {
                // read part of req, wait for the next one
                return true;
            }
            if (lastreq.requestid != nextReqid) {
                std::cout << "error: expect requestid " << nextReqid << ", got " << lastreq.requestid << std::endl;
                stop_benchmark();
                return false;
            }
            request_end_time[conn_id_][nextReqid - 1] = std::chrono::high_resolution_clock::now();
            nextReqid++;
            needed = 16;
            reqSkip = 0;
            bufSkip = sizeof (request) - 16;
        }
        return true;
    }
#endif

    void request_received(std::unique_ptr<IOBuf> readBuf)
    {
        received_bytes += readBuf->computeChainDataLength();
#ifdef CHECK_RESPONSE
        check(readBuf->computeChainDataLength(), readBuf.get());
#endif
        if (received_bytes == sizeof (request) * max_requests) {
            stop_benchmark();
        }
        if (PINGPANG_MODE) requests_send(1);
    }

private:
    std::shared_ptr<ConnectCallback> ccb_;
    BenchStats &bs;
};

std::atomic<int> Benchmark::active_connections_{0};
int Benchmark::total_connections_{0};
std::atomic<uint64_t> Benchmark::total_bandwidth_{0};
double Benchmark::sum_d{0};
double Benchmark::max_d{0};

void requestRead(void *opaque, std::unique_ptr<IOBuf> buf)
{
    Benchmark *bencp = reinterpret_cast<Benchmark *>(opaque);
    bencp->request_received(std::move(buf));
}

DEFINE_string(server_ip, "192.168.88.196", "Server IP Address");
DEFINE_int32(server_port, 10016, "Server Port");
DEFINE_int32(thread_num, 1, "Number of threads");
DEFINE_int32(conn_per_thread, 1, "Number of connections per thread");
DEFINE_int32(qdepth, 1, "Per-connection queue depth");
DEFINE_int32(total_requests, 80000, "Total number of requests per connection");
DEFINE_int32(qps_per_conn, 10, "Send QPS for each connection");
DEFINE_int32(batch_size, 1, "Send batch size");

// 线程函数
void run_benchmark_thread(int thread_id, const std::string &server_ip, int server_port, int conn_per_thread, int qdepth, int total_requests, BenchStats &bs, std::vector<std::unique_ptr<Benchmark>> &benchmarks, bool _pingpang_mode, int target_qps_per_conn)
{
    char tname[] = "c_bm0";
    tname[4] = '0' + thread_id;
    pthread_setname_np(pthread_self(), tname);

    auto evb = std::make_shared<EventBase>(EventBase::Options().setSkipTimeMeasurement(true));

    // 创建本线程的所有连接
    for (int i = 0; i < conn_per_thread; i++) {
        int conn_id = thread_id * conn_per_thread + i;
        benchmarks.emplace_back(std::make_unique<Benchmark>(server_ip, server_port, thread_id, conn_id, qdepth, evb, conn_per_thread, total_requests, bs, _pingpang_mode, target_qps_per_conn));
    }

    // 启动所有连接
    for (auto &b : benchmarks) {
        b->run();
    }

    //// 运行事件循环
    evb->loopForever();
}

void timer_thread(int i)
{
    char tname[] = "c_timer0";
    tname[7] = '0' + i;
    pthread_setname_np(pthread_self(), tname);

    bool pingpang_mode = false;
    if (!FLAGS_qps_per_conn) {
        FLAGS_qps_per_conn = 1;
        pingpang_mode = true;
    }
    int total_requests_per_thread = pingpang_mode ? 1 : FLAGS_total_requests;

    BenchStats bs;
    std::vector<std::unique_ptr<Benchmark>> benchmarks;
    std::thread io_thread([i, &bs, &benchmarks, &pingpang_mode] {
            run_benchmark_thread(i, FLAGS_server_ip, FLAGS_server_port, FLAGS_conn_per_thread, FLAGS_qdepth, FLAGS_total_requests, bs, benchmarks, pingpang_mode, FLAGS_qps_per_conn);
            // ~EventBase must be called by io_thread
            benchmarks.clear();
    });

    while (bs.started < FLAGS_conn_per_thread) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (auto &b : benchmarks) {
        b->start_benchmark();
    }
    int64_t total_qps = FLAGS_qps_per_conn * FLAGS_conn_per_thread;
    int64_t total_requests = FLAGS_total_requests * FLAGS_conn_per_thread;
    double ns_interval = 1.0e9 / total_qps;
    int npos = 0, n = (int)benchmarks.size();

    auto start_time = std::chrono::high_resolution_clock::now();
    if (!pingpang_mode) {
        for (int req = 0; req < total_requests; req += FLAGS_batch_size) {
            while (true) {
                auto cur_time = std::chrono::high_resolution_clock::now();
                double wt = std::chrono::duration_cast<std::chrono::nanoseconds>(cur_time - start_time).count();
                if (wt >= ns_interval * req) {
                    break;
                }
                asm volatile ("yield");
            }
#ifdef CHECK_RESPONSE
            for (int i = 0; i < FLAGS_batch_size; ++i) {
                Benchmark::request_start_time[npos][req + i] = std::chrono::high_resolution_clock::now();
            }
#endif
            benchmarks[npos]->evb_->runInEventBaseThread([be=benchmarks[npos].get()] {
                be->requests_send(FLAGS_batch_size);
            });
            npos = (npos + 1) % n;
        }
    }

    auto end_time2 = std::chrono::high_resolution_clock::now();
    double avg_qps2 = total_requests * 1e9 / std::chrono::duration_cast<std::chrono::nanoseconds>(end_time2 - start_time).count();

    std::cout << "Timer thread " << i << " stopping..." << std::endl;
    std::cout << "Timer thread target  send QPS " << std::fixed << std::setprecision(0) << total_qps <<  std::endl;
    std::cout << "Timer thread average send QPS " << std::fixed << std::setprecision(0) << avg_qps2 <<  std::endl;

    io_thread.join();
}

std::vector<std::vector<std::chrono::time_point<std::chrono::system_clock>>> Benchmark::request_start_time;
std::vector<std::vector<std::chrono::time_point<std::chrono::system_clock>>> Benchmark::request_end_time;

int main(int argc, char *argv[])
{
    folly::init(&argc, &argv);

    assert(1 <= FLAGS_thread_num);
    assert(1 <= FLAGS_conn_per_thread && FLAGS_conn_per_thread <= 4096);
    assert(1 <= FLAGS_batch_size);
    assert(1 <= FLAGS_total_requests && FLAGS_total_requests % FLAGS_batch_size == 0);

    int total_connections = FLAGS_conn_per_thread;
    Benchmark::setTotalConnections(total_connections);

    Benchmark::initRequestTimer(FLAGS_thread_num * FLAGS_conn_per_thread, FLAGS_total_requests);

    cout << "ThreadNum=" << FLAGS_thread_num 
         << ", ConnPerThread=" << FLAGS_conn_per_thread
         << ", TotalConn=" << FLAGS_conn_per_thread 
         << ", QDepth=" << FLAGS_qdepth
         << ", RequestsPerConn=" << FLAGS_total_requests
         << ", QPSPerConn=" << FLAGS_qps_per_conn
         << ", BatchSize=" << FLAGS_batch_size 
         << endl;

    std::vector<std::thread> threads;

    for (int i = 0; i < FLAGS_thread_num; i++) {
        threads.emplace_back([i]() {
            timer_thread(i);
        });
    }

    for (auto &t : threads) {
        t.join();
    }
#ifdef CHECK_RESPONSE
    Benchmark::timer_process();
#endif
    return 0;
}

