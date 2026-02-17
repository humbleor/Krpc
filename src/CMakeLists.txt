# src/CMakeLists.txt
# 构建核心 RPC 库 (krpc_core)

# ============================================================================
# 源文件收集
# ============================================================================

file(GLOB KRPC_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

# ============================================================================
# Protobuf 代码生成
# ============================================================================

# 生成 RPC 头文件
protobuf_generate_cpp(KRPC_PROTO_SRCS KRPC_PROTO_HDRS Krpcheader.proto)

# 生成用户服务定义 (example/user.proto)
protobuf_generate_cpp(USER_PROTO_SRCS USER_PROTO_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/../example/user.proto)

# ============================================================================
# 核心库目标
# ============================================================================

add_library(krpc_core STATIC
    ${KRPC_SOURCES}
    ${KRPC_PROTO_SRCS}
    ${USER_PROTO_SRCS}
)

# ============================================================================
# 目标属性
# ============================================================================

# 头文件路径
target_include_directories(krpc_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${PROTOBUF_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(krpc_core
    PUBLIC ${KRPC_EXTERNAL_LIBS}
)

# 编译特性
target_compile_features(krpc_core PUBLIC cxx_std_11)