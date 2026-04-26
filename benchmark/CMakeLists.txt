# benchmark/CMakeLists.txt
# Build benchmark executables (opt-in via KRPC_BUILD_BENCHMARKS)

protobuf_generate_cpp(BENCH_PROTO_SRCS BENCH_PROTO_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/../example/user.proto)

# Serialization benchmark (standalone, no krpc_core needed)
add_executable(ser_bench serialization_bench.cc ${BENCH_PROTO_SRCS})
target_include_directories(ser_bench PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROTOBUF_INCLUDE_DIRS}
    /usr/include/jsoncpp
)
target_link_libraries(ser_bench PRIVATE
    ${PROTOBUF_LIBRARIES}
    jsoncpp
)
set_target_properties(ser_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

# RPC benchmark (requires full krpc_core)
add_executable(rpc_bench rpc_bench.cc ${BENCH_PROTO_SRCS})
target_include_directories(rpc_bench PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${PROTOBUF_INCLUDE_DIRS}
)
target_link_libraries(rpc_bench PRIVATE
    krpc_core
    jsoncpp
)
set_target_properties(rpc_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)
