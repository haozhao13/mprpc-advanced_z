#!/bin/bash
# 启动 NODE_COUNT 个 RPC Server 实例，端口从 8000 到 (8000 + NODE_COUNT) - 1

# 切换到脚本所在目录（stress_test）
BASE_DIR="$(dirname "$(readlink -f "$0")")"
cd "$BASE_DIR"

# 项目根目录
PROJECT_ROOT="$(dirname "$BASE_DIR")"

# 创建配置和日志目录（在 project_root/bin 下）
mkdir -p "$PROJECT_ROOT/bin/conf"
mkdir -p "$PROJECT_ROOT/bin/log"

NODE_COUNT=30

for ((i = 0; i <= NODE_COUNT; i++))
do
    port=$((8000 + i))
    conf_file="$PROJECT_ROOT/bin/conf/server_${port}.conf"
    log_file="$PROJECT_ROOT/bin/log/provider_${port}.log"

    # 动态生成配置文件
    cat > "$conf_file" <<EOF
rpcserver_ip=127.0.0.1
rpcserver_port=${port}
zookeeperip=127.0.0.1
zookeeperport=2181
EOF

    # 启动 provider（使用绝对路径）
    "$PROJECT_ROOT/bin/provider" -i "$conf_file" > "$log_file" 2>&1 &

    echo "Started provider on port $port"
done

echo "$NODE_COUNT nodes started."
echo "Run 'pkill -f RpcProvider' or 'killall provider' to stop them later."