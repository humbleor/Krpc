#ifndef BENCHMARK_UTILS_H
#define BENCHMARK_UTILS_H

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>
#include <iomanip>
#include <iostream>

struct BenchResult {
    std::vector<double> latencies_us;
    double p50, p95, p99, min_val, max_val, mean;
    int total_requests;
    double qps;
    double elapsed_seconds;
};

inline BenchResult compute_percentiles(std::vector<double> latencies_us, double elapsed_seconds) {
    BenchResult result;
    result.total_requests = (int)latencies_us.size();
    result.elapsed_seconds = elapsed_seconds;

    if (result.total_requests == 0) {
        result.p50 = result.p95 = result.p99 = result.min_val = result.max_val = result.mean = 0;
        result.qps = 0;
        return result;
    }

    std::sort(latencies_us.begin(), latencies_us.end());

    int n = (int)latencies_us.size();
    result.min_val = latencies_us[0];
    result.max_val = latencies_us[n - 1];
    result.p50 = latencies_us[(int)(n * 0.50)];
    result.p95 = latencies_us[(int)(n * 0.95)];
    result.p99 = latencies_us[(int)(n * 0.99)];
    double sum = std::accumulate(latencies_us.begin(), latencies_us.end(), 0.0);
    result.mean = sum / n;
    result.qps = n / elapsed_seconds;
    result.latencies_us = std::move(latencies_us);
    return result;
}

inline void print_rpc_result(const std::string &label, const BenchResult &r) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  " << label << ":\n";
    std::cout << "    Requests: " << r.total_requests
              << "  Elapsed: " << r.elapsed_seconds << "s\n";
    std::cout << "    P50=" << r.p50 / 1000.0 << "ms"
              << "  P95=" << r.p95 / 1000.0 << "ms"
              << "  P99=" << r.p99 / 1000.0 << "ms"
              << "  QPS=" << r.qps << "\n";
    std::cout << std::defaultfloat;
}

inline std::string format_us(double us) {
    std::ostringstream oss;
    if (us >= 1000.0) {
        oss << std::fixed << std::setprecision(2) << us / 1000.0 << "ms";
    } else {
        oss << std::fixed << std::setprecision(1) << us << "us";
    }
    return oss.str();
}

#endif // BENCHMARK_UTILS_H
