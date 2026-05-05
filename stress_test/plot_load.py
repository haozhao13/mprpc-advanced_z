import os
import matplotlib.pyplot as plt
import re

# 1. 动态获取项目根目录，避免路径失效
# 如果脚本在 bin 目录下运行，请根据实际位置调整
project_root = os.path.abspath(os.path.join(os.getcwd(), "..")) 
log_dir = os.path.join(project_root, "bin", "log")

# 2. 定义要统计的端口范围
ports = range(8000, 8030)
load_distribution = []
actual_active_ports = []

# 匹配 "Doing local service:" 关键字的正则，使其更具通用性
rpc_pattern = re.compile(r"Doing local service: \w+")

for port in ports:
    # 修正路径拼接逻辑
    log_file = os.path.join(log_dir, f"provider_{port}.log")
    count = 0
    
    if os.path.exists(log_file):
        with open(log_file, "r", encoding='utf-8', errors='ignore') as f:
            # 统计所有 RPC 服务的调用次数，不仅限于 Login[cite: 15, 17, 18]
            count = sum(1 for line in f if "Doing local service:" in line)
    
    load_distribution.append(count)
    actual_active_ports.append(str(port))

# 3. 绘图优化
plt.figure(figsize=(14, 7))
bars = plt.bar(actual_active_ports, load_distribution, color='steelblue', edgecolor='black', alpha=0.8)

# 在柱状图上方标注具体数值，解决“数值看起来是平的”观察困难问题[cite: 21]
for bar in bars:
    height = bar.get_height()
    plt.text(bar.get_x() + bar.get_width()/2., height + 0.5,
             f'{int(height)}', ha='center', va='bottom', fontsize=9)

# 绘制理想平均线
ideal_avg = 500 / 30
plt.axhline(y=ideal_avg, color='crimson', linestyle='--', linewidth=2, 
            label=f'Ideal Average ({ideal_avg:.1f})')

plt.title("RPC Load Balance Distribution Analysis", fontsize=14)
plt.xlabel("Server Port (Provider Instance)", fontsize=12)
plt.ylabel("Requests Handled (Count)", fontsize=12)
plt.xticks(rotation=45)
plt.grid(axis='y', linestyle=':', alpha=0.6)
plt.legend()

plt.tight_layout()
plt.savefig("load_distribution_fixed.png")
print(f"统计完成。总请求数: {sum(load_distribution)}，图表已保存至 load_distribution_fixed.png")