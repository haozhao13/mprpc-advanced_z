cmake_minimum_required(VERSION 3.22)
project(mprpc)

# 强制使用 C++11 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置二进制可执行文件输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# 设置库文件输出路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 包含框架对外暴露的头文件路径
include_directories(${PROJECT_SOURCE_DIR}/src/include)

# 链接外部依赖库配置 (Zookeeper, Protobuf, 线程库)
link_libraries(zookeeper_mt protobuf pthread)

# 添加子构建目录
add_subdirectory(src)
add_subdirectory(example)

target_link_libraries(mprpc pthread protobuf zookeeper_mt)