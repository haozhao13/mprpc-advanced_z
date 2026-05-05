#!/bin/bash

# 发生错误时立即退出脚本
set -e

# 进入项目根目录
cd "$(dirname "$0")"

# 1. 清理并重新构建
echo "Start building mprpc..."
rm -rf build/*
rm -rf lib/*
rm -rf example/bin
find bin/ -type f ! -name "*.conf" -delete  # 清理bin文件夹下除过.conf文件外的全部文件，即consumer和provider

mkdir -p build
cd build
cmake ..
make

# 回到项目根目录
cd ..


# # 2. 部署头文件到系统目录
# echo "Installing headers to /usr/include/mprpc..."
# mkdir -p /usr/include/mprpc  # 在系统目录下创建mprpc文件夹，用于存放头文件
# cp src/include/*.h /usr/include/mprpc/

# # 3. 部署动态链接库到系统目录
# echo "Installing libmprpc.so to /usr/lib..."
# cp lib/libmprpc.so /usr/lib/  # 在系统库目录下存放编译链接生成的动态链接库libmprpc.so

# # 4. 刷新系统动态库缓存
# ldconfig

echo "mprpc(Simplified version) framework build and compile success!"
