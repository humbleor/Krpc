#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Krpcapplication.h"
#include "Krpccontroller.h"
#include "Krpcheader.pb.h"
#include "user.pb.h"
#include "zookeeperutil.h"
#include "benchmark_utils.h"
#include <glog/logging.h>

// ============================================================================
// BenchmarkChannel: like KrpcChannel but supports connection reuse for benchmarking
// ============================================================================
class BenchmarkChannel : public google::protobuf::RpcChannel {
public:
    BenchmarkChannel() : m_clientfd(-1), m_reuse(false) {}
    ~BenchmarkChannel() override {
        if (m_clientfd >= 0) close(m_clientfd);
    }
    void set_reuse(bool v) { m_reuse = v; }
    bool connect(const std::string &ip, uint16_t port) {
        m_ip = ip; m_port = port;
        m_clientfd = socket(AF_INET, SOCK_STREAM, 0);
        if (m_clientfd < 0) return false;
        // 设置SO_REUSEADDR，避免TIME_WAIT导致端口耗尽
        int opt = 1;
        setsockopt(m_clientfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        if (::connect(m_clientfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close(m_clientfd); m_clientfd = -1; return false;
        }
        return true;
    }
    void CallMethod(const ::google::protobuf::MethodDescriptor *method,
                    ::google::protobuf::RpcController *controller,
                    const ::google::protobuf::Message *request,
                    ::google::protobuf::Message *response,
                    ::google::protobuf::Closure *done) override {
        (void)done;
        // Serialize header
        Krpc::RpcHeader header;
        header.set_service_name(method->service()->name());
        header.set_method_name(method->name());
        std::string args_str;
        if (!request->SerializeToString(&args_str)) {
            controller->SetFailed("serialize request"); return;
        }
        header.set_args_size((uint32_t)args_str.size());
        std::string header_str;
        if (!header.SerializeToString(&header_str)) {
            controller->SetFailed("serialize header"); return;
        }
        // Build send buffer
        std::string send_buf;
        {
            google::protobuf::io::StringOutputStream so(&send_buf);
            google::protobuf::io::CodedOutputStream co(&so);
            co.WriteVarint32((uint32_t)header_str.size());
            co.WriteString(header_str);
        }
        send_buf += args_str;
        if (send(m_clientfd, send_buf.c_str(), send_buf.size(), 0) < 0) {
            controller->SetFailed("send failed"); return;
        }
        // Receive response
        char recv_buf[1024] = {0};
        int recv_size = recv(m_clientfd, recv_buf, sizeof(recv_buf), 0);
        if (recv_size < 0) {
            controller->SetFailed("recv failed"); return;
        }
        if (!response->ParseFromArray(recv_buf, recv_size)) {
            controller->SetFailed("parse response"); return;
        }
        if (!m_reuse) {
            close(m_clientfd); m_clientfd = -1;
        }
    }
private:
    int m_clientfd;
    bool m_reuse;
    std::string m_ip;
    uint16_t m_port;
};

// ============================================================================
// Helpers
// ============================================================================
static std::string parse_arg(int argc, char **argv, const char *flag, const char *defval) {
    for (int i = 1; i < argc - 1; i++) {
        if (std::string(argv[i]) == flag) return argv[i + 1];
    }
    return defval;
}
static std::vector<int> parse_concurrency(const std::string &s) {
    std::vector<int> vals;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ',')) {
        vals.push_back(std::stoi(token));
    }
    return vals;
}

static std::string query_server_ip_port(ZkClient *zk, const std::string &service, const std::string &method, int &colon_pos) {
    std::string path = "/" + service + "/" + method;
    std::string data = zk->GetData(path.c_str());
    if (data.empty()) return "";
    colon_pos = data.find(":");
    return data;
}

// ============================================================================
// Benchmark: End-to-End Latency & QPS (with connection reuse per thread)
// ============================================================================
static void bench_e2e(const std::string &ip, uint16_t port,
                      int concurrency, int requests) {
    std::cout << "--- End-to-End Latency (" << requests << " requests, concurrency=" << concurrency << ") ---\n";

    std::vector<double> all_latencies;
    std::mutex lat_mtx;
    std::atomic<int> success{0}, fail{0}, recv_fail{0}, parse_fail{0}, send_fail{0}, other_fail{0}, connect_fail{0};
    int requests_per_thread = requests / concurrency;

    auto start_all = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < concurrency; t++) {
        threads.emplace_back([&]() {
            std::vector<double> local_lat;
            // Each thread uses one KrpcChannel with connection reuse
            KrpcChannel ch(false);
            ch.set_reuse_connection(true);
            // Warm up: trigger ZK lookup + connect
            Kuser::UserServiceRpc_Stub stub0(&ch);
            Kuser::LoginRequest req0; req0.set_name("warmup"); req0.set_pwd("warmup");
            Kuser::LoginResponse resp0; Krpccontroller ctrl0;
            stub0.Login(&ctrl0, &req0, &resp0, nullptr);
            if (ctrl0.Failed()) { connect_fail++; fail++; return; }

            for (int j = 0; j < requests_per_thread; j++) {
                Kuser::UserServiceRpc_Stub stub(&ch);
                Kuser::LoginRequest req;
                req.set_name("zhangsan"); req.set_pwd("123456");
                Kuser::LoginResponse resp;
                Krpccontroller ctrl;
                auto t0 = std::chrono::high_resolution_clock::now();
                stub.Login(&ctrl, &req, &resp, nullptr);
                auto t1 = std::chrono::high_resolution_clock::now();
                double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
                local_lat.push_back(us);
                if (ctrl.Failed()) {
                    fail++;
                    std::string err = ctrl.ErrorText();
                    if (err.find("recv") != std::string::npos) recv_fail++;
                    else if (err.find("parse") != std::string::npos) parse_fail++;
                    else if (err.find("send") != std::string::npos) send_fail++;
                    else other_fail++;
                } else if (resp.result().errcode() != 0) { fail++; }
                else success++;
            }
            std::lock_guard<std::mutex> lock(lat_mtx);
            all_latencies.insert(all_latencies.end(), local_lat.begin(), local_lat.end());
        });
    }
    for (auto &t : threads) t.join();
    auto end_all = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end_all - start_all).count();

    BenchResult r = compute_percentiles(std::move(all_latencies), elapsed);
    print_rpc_result("", r);
    std::cout << "  Success: " << success << "  Failed: " << fail
              << " (recv=" << recv_fail << " parse=" << parse_fail
              << " send=" << send_fail << " connect=" << connect_fail
              << " other=" << other_fail << ")\n\n";
}

// ============================================================================
// Benchmark: Connection Reuse vs Per-Request Connect
// ============================================================================
static void bench_connection_reuse(const std::string &ip, uint16_t port, int requests) {
    std::cout << "--- Connection Reuse Impact (" << requests << " requests, 1 thread) ---\n";

    // Mode 1: per-request connect (no reuse) — uses BenchmarkChannel to avoid ZK overhead
    {
        std::vector<double> lats;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < requests; i++) {
            BenchmarkChannel ch;
            if (!ch.connect(ip, port)) continue;
            Kuser::UserServiceRpc_Stub stub(&ch);
            Kuser::LoginRequest req; req.set_name("zhangsan"); req.set_pwd("123456");
            Kuser::LoginResponse resp; Krpccontroller ctrl;
            auto t_start = std::chrono::high_resolution_clock::now();
            stub.Login(&ctrl, &req, &resp, nullptr);
            auto t_end = std::chrono::high_resolution_clock::now();
            lats.push_back(std::chrono::duration<double, std::micro>(t_end - t_start).count());
        }
        auto t3 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(t3 - t0).count();
        BenchResult r = compute_percentiles(std::move(lats), elapsed);
        std::cout << "  Per-request connect: P50=" << format_us(r.p50)
                  << "  P95=" << format_us(r.p95) << "  QPS=" << std::fixed << std::setprecision(1) << r.qps << "\n";
    }

    // Mode 2: connection reuse — uses real KrpcChannel with set_reuse_connection(true)
    {
        std::vector<double> lats;
        KrpcChannel ch(false);
        ch.set_reuse_connection(true);
        // Connect directly to known address (skip ZK)
        // Force connect by calling with a dummy stub to trigger the ZK path once
        // Instead, we just warm up: call once normally then reuse won't close
        Kuser::UserServiceRpc_Stub stub0(&ch);
        Kuser::LoginRequest req0; req0.set_name("warmup"); req0.set_pwd("warmup");
        Kuser::LoginResponse resp0; Krpccontroller ctrl0;
        stub0.Login(&ctrl0, &req0, &resp0, nullptr); // triggers ZK + connect
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < requests; i++) {
            Kuser::UserServiceRpc_Stub stub(&ch);
            Kuser::LoginRequest req; req.set_name("zhangsan"); req.set_pwd("123456");
            Kuser::LoginResponse resp; Krpccontroller ctrl;
            auto t_start = std::chrono::high_resolution_clock::now();
            stub.Login(&ctrl, &req, &resp, nullptr);
            auto t_end = std::chrono::high_resolution_clock::now();
            lats.push_back(std::chrono::duration<double, std::micro>(t_end - t_start).count());
        }
        auto t3 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(t3 - t0).count();
        BenchResult r = compute_percentiles(std::move(lats), elapsed);
        std::cout << "  Connection reused:   P50=" << format_us(r.p50)
                  << "  P95=" << format_us(r.p95) << "  QPS=" << std::fixed << std::setprecision(1) << r.qps << "\n\n";
    }
}

// ============================================================================
// Benchmark: Connection Overhead (ZK lookup + TCP connect)
// ============================================================================
static void bench_connection_overhead(const std::string &ip, uint16_t port, int iterations) {
    std::cout << "--- Connection Overhead (" << iterations << " iterations) ---\n";

    // ZK lookup only
    {
        std::vector<double> lats;
        int colon_pos = 0;
        ZkClient zk; zk.Start();
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            auto t1 = std::chrono::high_resolution_clock::now();
            query_server_ip_port(&zk, "UserServiceRpc", "Login", colon_pos);
            auto t2 = std::chrono::high_resolution_clock::now();
            lats.push_back(std::chrono::duration<double, std::micro>(t2 - t1).count());
        }
        auto t3 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(t3 - t0).count();
        BenchResult r = compute_percentiles(std::move(lats), elapsed);
        std::cout << "  ZK lookup:     P50=" << format_us(r.p50)
                  << "  P95=" << format_us(r.p95) << "\n";
    }

    // TCP connect only
    {
        std::vector<double> lats;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) continue;
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = inet_addr(ip.c_str());
            auto t1 = std::chrono::high_resolution_clock::now();
            ::connect(fd, (struct sockaddr *)&addr, sizeof(addr));
            auto t2 = std::chrono::high_resolution_clock::now();
            lats.push_back(std::chrono::duration<double, std::micro>(t2 - t1).count());
            close(fd);
        }
        auto t3 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(t3 - t0).count();
        BenchResult r = compute_percentiles(std::move(lats), elapsed);
        std::cout << "  TCP connect:   P50=" << format_us(r.p50)
                  << "  P95=" << format_us(r.p95) << "\n\n";
    }
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char **argv) {
    // 抑制ZK客户端INFO日志，避免打断benchmark输出
    zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);
    // 抑制glog INFO日志（我们的 zookkeeperutil.cc 使用了 LOG(INFO)）
    FLAGS_minloglevel = 1; // WARNING 及以上才输出

    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " -i <config_file> [options]\n";
        std::cout << "Options:\n";
        std::cout << "  --concurrency 1,4,8,16,32   concurrency levels (default: 1,4,8)\n";
        std::cout << "  --requests 1000             requests per level (default: 1000)\n";
        std::cout << "  --iterations 1000           iterations for overhead bench (default: 1000)\n";
        return 1;
    }

    std::string config_file = parse_arg(argc, argv, "-i", "");
    std::string concurrency_str = parse_arg(argc, argv, "--concurrency", "1,4,8");
    int requests = std::stoi(parse_arg(argc, argv, "--requests", "1000"));
    int iterations = std::stoi(parse_arg(argc, argv, "--iterations", "1000"));

    // Build filtered argv for KrpcApplication::Init (only -i <config>)
    // Init uses getopt which will reject --concurrency etc.
    std::vector<const char *> init_argv = {argv[0], "-i", config_file.c_str()};
    int init_argc = (int)init_argv.size();
    KrpcApplication::Init(init_argc, const_cast<char **>(init_argv.data()));

    std::string ip = KrpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    int port = std::stoi(KrpcApplication::GetInstance().GetConfig().Load("rpcserverport"));
    std::vector<int> concurrencies = parse_concurrency(concurrency_str);

    std::cout << "=== RPC Performance Benchmark ===\n";
    std::cout << "Server: " << ip << ":" << port << "\n";
    std::cout << "Method: UserServiceRpc.Login\n\n";

    // Run e2e benchmarks
    for (int c : concurrencies) {
        bench_e2e(ip, (uint16_t)port, c, requests);
    }

    // Connection reuse
    bench_connection_reuse(ip, (uint16_t)port, requests);

    // Connection overhead
    bench_connection_overhead(ip, (uint16_t)port, iterations);

    return 0;
}
