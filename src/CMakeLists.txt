# 搜集当前 src 目录下的所有源文件
aux_source_directory(. SRC_LIST)

# 将源文件编译为动态链接库 libmprpc.so
# 输出路径已在顶层 CMakeLists.txt 中定义为 mprpc/lib
add_library(mprpc SHARED ${SRC_LIST})