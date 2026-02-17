# example/CMakeLists.txt
# 构建 RPC 示例程序：server (服务端) 和 client (客户端)

# ============================================================================
# 源文件收集
# ============================================================================

# 获取 protobuf 生成的源文件
file(GLOB PROTO_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.pb.cc)

# ============================================================================
# Server 目标 (服务端示例)
# ============================================================================

file(GLOB SERVER_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/callee/*.cc)
add_executable(server ${SERVER_SRCS} ${PROTO_SRCS})

target_link_libraries(server PRIVATE krpc_core)

# 设置输出目录
set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

# ============================================================================
# Client 目标 (客户端示例)
# ============================================================================

file(GLOB CLIENT_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/caller/*.cc)
add_executable(client ${CLIENT_SRCS} ${PROTO_SRCS})

target_link_libraries(client PRIVATE krpc_core)

# 设置输出目录
set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)