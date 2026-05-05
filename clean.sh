#!/bin/bash

# 获取脚本所在的当前目录
current_dir=$(cd $(dirname $0); pwd)
cd $current_dir

echo "正在清理目录..."

# 1. 删除 lib 文件夹下的全部内容
# 使用 * 匹配所有文件和隐藏文件（如果需要）
rm -rf ./lib/*

# 2. 删除 build 文件夹下的全部内容
rm -rf ./build/*

# 3. 删除 bin 文件夹下除 .conf 以外的全部内容
# 使用反向匹配模式 (extglob)
# 注意：如果是在脚本中执行，需要先开启 extglob 选项
shopt -s extglob
rm -rf ./bin/!(*.conf)

echo "清理完成！"