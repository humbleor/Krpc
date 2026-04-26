#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include "user.pb.h"

#include <jsoncpp/json/json.h>

static const int ITERATIONS = 500000;

struct SerResult {
    int size_bytes;
    double serialize_us;
    double deserialize_us;
};

template <typename Fn>
static double time_us(Fn fn, int iterations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        fn();
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(end - start).count();
}

static SerResult bench_protobuf_login() {
    Kuser::LoginRequest msg;
    msg.set_name("zhangsan");
    msg.set_pwd("123456");

    size_t sz = 0;
    auto ser_fn = [&]() {
        std::string out;
        msg.SerializeToString(&out);
        sz = out.size();
    };
    auto deser_fn = [&]() {
        std::string data;
        msg.SerializeToString(&data);
        sz = data.size();
        Kuser::LoginRequest tmp;
        tmp.ParseFromString(data);
    };

    double ser_us = time_us(ser_fn, ITERATIONS);
    double deser_us = time_us(deser_fn, ITERATIONS);
    return {(int)sz, ser_us, deser_us};
}

static SerResult bench_protobuf_register() {
    Kuser::RegisterRequest msg;
    msg.set_id(1001);
    msg.set_name("zhangsan");
    msg.set_pwd("123456");

    size_t sz = 0;
    auto ser_fn = [&]() {
        std::string out;
        msg.SerializeToString(&out);
        sz = out.size();
    };
    auto deser_fn = [&]() {
        std::string data;
        msg.SerializeToString(&data);
        sz = data.size();
        Kuser::RegisterRequest tmp;
        tmp.ParseFromString(data);
    };

    double ser_us = time_us(ser_fn, ITERATIONS);
    double deser_us = time_us(deser_fn, ITERATIONS);
    return {(int)sz, ser_us, deser_us};
}

static Json::Value make_json_login() {
    Json::Value v;
    v["name"] = "zhangsan";
    v["pwd"] = "123456";
    return v;
}

static Json::Value make_json_register() {
    Json::Value v;
    v["id"] = 1001;
    v["name"] = "zhangsan";
    v["pwd"] = "123456";
    return v;
}

static SerResult bench_json_login() {
    Json::Value root = make_json_login();
    size_t sz = 0;
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";

    auto ser_fn = [&]() {
        std::string out = Json::writeString(builder, root);
        sz = out.size();
    };
    auto deser_fn = [&]() {
        std::string data = Json::writeString(builder, root);
        sz = data.size();
        Json::CharReaderBuilder reader_builder;
        Json::Value tmp;
        std::string errs;
        std::istringstream iss(data);
        Json::parseFromStream(reader_builder, iss, &tmp, &errs);
    };

    double ser_us = time_us(ser_fn, ITERATIONS);
    double deser_us = time_us(deser_fn, ITERATIONS);
    return {(int)sz, ser_us, deser_us};
}

static SerResult bench_json_register() {
    Json::Value root = make_json_register();
    size_t sz = 0;
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";

    auto ser_fn = [&]() {
        std::string out = Json::writeString(builder, root);
        sz = out.size();
    };
    auto deser_fn = [&]() {
        std::string data = Json::writeString(builder, root);
        sz = data.size();
        Json::CharReaderBuilder reader_builder;
        Json::Value tmp;
        std::string errs;
        std::istringstream iss(data);
        Json::parseFromStream(reader_builder, iss, &tmp, &errs);
    };

    double ser_us = time_us(ser_fn, ITERATIONS);
    double deser_us = time_us(deser_fn, ITERATIONS);
    return {(int)sz, ser_us, deser_us};
}

static std::string format_time(double us) {
    std::ostringstream oss;
    if (us >= 1000.0) {
        oss << std::fixed << std::setprecision(1) << us / 1000.0 << "ms";
    } else {
        oss << std::fixed << std::setprecision(1) << us << "us";
    }
    return oss.str();
}

int main() {
    std::cout << "=== Serialization Benchmark (" << ITERATIONS << " iterations) ===\n\n";

    auto pb_login = bench_protobuf_login();
    auto pb_reg = bench_protobuf_register();
    auto json_login = bench_json_login();
    auto json_reg = bench_json_register();

    auto print_row = [&](const std::string &msg_name,
                         const SerResult &pb, const SerResult &json) {
        std::cout << "Message: " << msg_name << "\n";
        std::cout << "  +------------------+------------+------------+------------+\n";
        std::cout << "  |                  | Size (B)   | Serialize  | Deserialize|\n";
        std::cout << "  +------------------+------------+------------+------------+\n";
        std::cout << "  | Protobuf         | " << std::setw(10) << pb.size_bytes
                  << " | " << std::setw(10) << format_time(pb.serialize_us)
                  << " | " << std::setw(10) << format_time(pb.deserialize_us) << " |\n";
        std::cout << "  | JSON             | " << std::setw(10) << json.size_bytes
                  << " | " << std::setw(10) << format_time(json.serialize_us)
                  << " | " << std::setw(10) << format_time(json.deserialize_us) << " |\n";
        std::cout << "  +------------------+------------+------------+------------+\n";
        std::cout << "  | Ratio (JSON/PB)  | " << std::fixed << std::setprecision(2)
                  << std::setw(10) << (double)json.size_bytes / pb.size_bytes << "x"
                  << " | " << std::setw(10) << json.serialize_us / pb.serialize_us << "x"
                  << " | " << std::setw(10) << json.deserialize_us / pb.deserialize_us << "x |\n";
        std::cout << "  +------------------+------------+------------+------------+\n\n";
    };

    print_row("LoginRequest", pb_login, json_login);
    print_row("RegisterRequest", pb_reg, json_reg);

    return 0;
}
