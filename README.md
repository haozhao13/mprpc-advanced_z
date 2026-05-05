# mprpc: 一个轻量级、高性能的 C++ 分布式 RPC 框架

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-11-blue.svg)]()
[![License](https://img.shields.io/badge/license-MIT-green.svg)]()
[![Last Commit](https://img.shields.io/badge/last--commit/2026-active-brightgreen.svg)]()
[![Location](https://img.shields.io/badge/location-Shaanxi,%20China-blue.svg)]()

**mprpc** 是一个为了深入理解分布式系统原理，从零开始设计和实现的高性能 RPC（远程过程调用）框架。该项目不依赖庞大的第三方网络库，而是基于 Linux **epoll** 系统调用手工打造了 Reactor 高并发网络模型。它结合了 **Google Protobuf** 的高效序列化能力、**Zookeeper** 的分布式协调能力以及 **一致性哈希 (Consistent Hashing)** 算法，实现了一个功能完备、可用于构建微服务架构的通信中间件。

> **项目更新日志**  
> 最近更新于 2026-05-05 | 陕西省西安市

## 核心特性

- **高性能网络引擎**：基于 Reactor 模型，采用 `epoll ET` 模式 + 非阻塞 I/O，配合自定义线程池，支持高并发连接。
- **智能服务治理**：集成 Zookeeper 实现服务自动注册与发现，利用临时节点 (Ephemeral Znode) 实现宕机自动剔除。
- **一致性哈希负载均衡**：客户端支持一致性哈希算法，保证相同 Key（如用户 ID）的请求始终路由到同一台机器，支持本地缓存亲和性。
- **实时 Watcher 机制**：服务端节点变动时，Zookeeper 客户端能实时感知并更新本地路由缓存，无需重启服务。
- **资源复用优化**：内置 TCP 连接池，支持长连接复用，减少握手开销；支持同步/异步调用模式。


## 技术栈

- **核心语言**：C++ 11 (智能指针, Lambda, 线程库)
- **序列化**：Google Protocol Buffers
- **网络库**：原生 Linux Socket + Epoll
- **服务发现**：Apache Zookeeper (C API)
- **构建系统**：CMake
- **设计模式**：Reactor, Singleton, Prototype, Factory


## 核心架构

### 1. 网络层 (Reactor 模型)
- **TcpServer / TcpConnection**：基于 `epoll` 实现的非阻塞网络模块，包含输入/输出缓冲区。
- **定时器管理**：基于最小堆/时间轮实现的定时器，用于管理死连接和超时任务。
- **线程池**：任务队列 + 预创建线程，处理业务逻辑解耦。

### 2. RPC 调用层
- **MprpcChannel (Client)**：
    - **序列化**：将 Protobuf 消息封装为 `RpcHeader + Args`。
    - **服务发现**：连接 Zookeeper 获取服务列表。
    - **负载均衡**：使用一致性哈希算法选择目标节点。
    - **连接池**：复用 TCP 连接发送数据。
- **RpcProvider (Server)**：
    - **反序列化**：解析 Header 和参数。
    - **反射调用**：利用 Protobuf 反射机制动态调用业务方法。

### 3. 分布式协调层
- **ZkClient**：封装 Zookeeper C API，实现节点创建、子节点监听 (Watcher)。
- **动态路由**：监听 `/Service/Method` 路径下的子节点变化，自动更新一致性哈希环。


## 快速开始

### 1. 环境准备
- 确保已安装 `protobuf-devel`, `zookeeper-c-client-devel`, `cmake`, `g++`。

### 2. 克隆项目
- 执行：git clone https://github.com/haozhao13/mprpc-advanced_z.git
- 拉取后执行：cd mprpc-advanced_z

### 3. 编译项目
- 执行自动化构建脚本：sudo ./original_autobuild.sh


## 项目结构
- mprpc-advanced_z/ 
- ├── bin/                                  # 可执行文件与配置
- │   └── client.conf                       # 压测专用配置文件 (仅含 ZK 地址)
- ├── src/                                  # 核心源码目录
- │   ├── include/                          # 头文件
- │   ├── mprpcchannel.cc                   # 客户端通道 (核心路由逻辑)
- │   ├── rpcprovider.cc                    # 服务端发布器
- │   ├── tcpserver.cc                      # Reactor 主从模型实现
- │   ├── connpool.cc                       # TCP 连接池
- │   ├── zookeeperutil.cc                  # ZooKeeper 工具类
- │   └── ...
- ├── stress_test/                          # 压力测试模块
- │   ├── plot_load.py                      # 日志数据可视化脚本
- │   └── start_cluster.sh                  # 集群启动脚本 (构建 NODE_COUNT 个节点)
- └── CMakeLists.txt                        # CMake 构建规则


# mprpc 框架压力测试执行手册

## 1. 前置准备：编写测试逻辑
在正式构建项目之前，需要先定义好客户端（Consumer）发送的具体请求逻辑。

*   **操作文件**：`calluserservice.cc`
*   **操作内容**：需要提前编写好该文件内的逻辑，核心是**循环开启指定次数的 RPC 请求**。
    *   *注：文档中示例为发起 500 次请求，循环调用 `Login` 方法。*

## 2. 第一阶段：构建与编译
确保代码就绪后，在项目根目录下执行构建脚本。

*   **执行命令**：
    ```bash
    sudo ./original_autobuild.sh
    ```
*   **预期结果**：
    *   在 `bin/` 目录下生成可执行文件 `consumer`（客户端）和 `provider`（服务端）。
    *   在 `lib/` 目录下生成链接库 `libmprpc.so`。

## 3. 第二阶段：启动 RPC 服务集群 (30 Nodes)
这是模拟生产环境的关键步骤，通过脚本自动部署 30 个服务节点。

*   **进入目录**：
    ```bash
    cd stress_test
    ```
*   **执行命令**：
    ```bash
    ./start_cluster.sh
    ```
*   **脚本核心行为解析**：
    1.  **配置生成**：脚本会在 `/bin/conf/` 目录下自动生成 **30 份 `.conf` 配置文件**。
    2.  **节点启动**：脚本会依次读取每份配置，启动对应的 RPC 服务节点。
    3.  **Zookeeper 注册**：每个节点启动后，会读取配置并向 Zookeeper 注册中心发布自身的服务地址（IP:Port）。
    4.  **日志管理**：指定日志生成地址为 `/bin/log/`，便于后续分析。
*   **验证结果**：
    *   此时，你应该能看到 **30 个临时节点（Ephemeral Nodes）** 成功注册到 Zookeeper 中。
    *   查看 `/bin/log/` 下的日志文件，确认出现 `ZK初始化成功` 和 `Znode创建成功` 的字样，表示所有节点已就绪，等待接收请求。

## 4. 第三阶段：发起压测 (500 Requests)
集群就绪后，正式启动客户端发起请求。

*   **操作步骤**：
    1.  **新开终端窗口**（保持服务端运行）。
    2.  **进入目录**：
        ```bash
        cd ../bin
        ```
    3.  **执行命令**：
        ```bash
        ./consumer -i client.conf
        ```
*   **配置说明**：
    *   `client.conf` 文件中仅配置了 Zookeeper 的 IP 地址。
    *   **注意**：配置文件中**没有**写死 RPC 服务的地址。因为在生产级环境下，服务地址是动态变化的，客户端需要通过 Zookeeper 实时获取可用的服务列表。
*   **预期流量**：
    *   客户端将发起 **500 次 RPC 请求**，这些请求将被打散并路由到刚才启动的 **30 个 RPC 服务节点**上。

## 5. 第四阶段：结果分析与可视化
压测结束后，需要通过脚本分析日志，验证负载均衡的效果。

*   **分析工具**：`plot_load.py` (Python 脚本)
*   **操作流程**：
    1.  **提取日志**：脚本会提取 `/bin/log/` 文件夹下的全部日志文件。
    2.  **统计分析**：统计每个服务节点在本次压测中处理了多少个 RPC 调用请求。
    3.  **可视化展示**：
        *   **图表解读**：文档中提到，右侧为测试阶段，左侧为实验阶段。
        *   **验证标准**：30 个柱状图（代表30个节点）高度应大致均衡。
*   **预期结论**：
    *   **负载均衡**：证明了“基于一致性哈希的软负载均衡”算法成功将服务均匀打到了不同节点。
    *   **结果重合**：两次实验结果完全重合，证明了算法的**稳定性**——相同 ID 的请求（如用户ID）每次都会被固定路由到同一个服务节点上。
    *   **缓存优势**：这种特性使得服务端可以利用本地缓存（Local Cache），大幅提升接口性能。

---

## 总结
这套流程完美复现了 **500次请求 -> 30个节点** 的分布式场景。通过一致性哈希算法，mprpc 框架不仅实现了负载均衡，还保证了请求的“粘性”，这在需要本地缓存的业务场景中至关重要。祝你在渭南的开发工作顺利！


