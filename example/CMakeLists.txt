cmake_minimum_required(VERSION 3.0)
project(mprpc_example)

# 1. 设置可执行文件输出路径到 example/bin
# set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 2. 包含系统级 RPC 框架头文件目录以及当前目录（为了找到 user.pb.h）
include_directories(/usr/include/mprpc)
include_directories(${PROJECT_SOURCE_DIR})

# 3. 编译 Provider 服务端，直接链接【系统动态库 mprpc】
add_executable(provider callee/userservice.cc user.pb.cc)
target_link_libraries(provider mprpc protobuf pthread)

# 4. 编译 Consumer 客户端，直接链接【系统动态库 mprpc】
add_executable(consumer caller/calluserservice.cc user.pb.cc)
target_link_libraries(consumer mprpc protobuf pthread)