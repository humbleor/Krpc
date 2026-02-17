# ============================================================================
# Krpc - 分布式 RPC 框架
# 基于 Protobuf, Muduo, Zookeeper, Glog
# ============================================================================

cmake_minimum_required(VERSION 3.10)
project(Krpc
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "A distributed RPC framework based on protobuf"
)

# ============================================================================
# 全局编译设置
# ============================================================================

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 编译选项
add_compile_options(-Wall -Wextra -Wpedantic)

# ============================================================================
# 依赖查找
# ============================================================================

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

# ============================================================================
# 全局库列表 (用于外部依赖)
# ============================================================================

set(KRPC_EXTERNAL_LIBS
    Threads::Threads
    zookeeper_mt
    muduo_net
    muduo_base
    glog
    ${PROTOBUF_LIBRARIES}
)

# ============================================================================
# 子目录
# ============================================================================

add_subdirectory(src)
add_subdirectory(example)

# 可选：测试目录
option(KRPC_BUILD_TESTS "Build tests" OFF)
if(KRPC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# 安装配置
# ============================================================================

install(DIRECTORY include/ DESTINATION include)
install(TARGETS krpc_core EXPORT krpc-targets)
install(EXPORT krpc-targets FILE KrpcConfig.cmake DESTINATION lib/cmake/Krpc)
