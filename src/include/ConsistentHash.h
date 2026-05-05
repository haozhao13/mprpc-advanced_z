// 基于一致性哈希的软负载均衡算法
#include <map>
#include <string>
#include <vector>
#include <stdint.h>

class ConsistentHash {
public:
    // virtual_node_num 建议设为 100-200，防止数据倾斜
    ConsistentHash(int virtual_node_num = 150) : m_virtual_node_num(virtual_node_num) {}

    // 将 host_list 中的物理节点及其虚拟节点加入哈希环
    void AddNodes(const std::vector<std::string>& hosts) {
        m_ring.clear();
        for (const auto& host : hosts) {
            for (int i = 0; i < m_virtual_node_num; ++i) {
                // 为每个物理节点生成多个虚拟节点标识
                std::string virtual_node_name = host + "#" + std::to_string(i);
                uint32_t hash = Hash(virtual_node_name);
                m_ring[hash] = host; // 建立虚拟节点到物理节点的映射[cite: 11]
            }
        }
    }

    // 根据 key（如方法名）顺时针寻找最近的节点[cite: 11]
    std::string GetTargetHost(const std::string& key) {
        if (m_ring.empty()) return "";

        uint32_t hash = Hash(key);
        // lower_bound 寻找第一个大于或等于 hash 的位置[cite: 11]
        auto it = m_ring.lower_bound(hash);
        
        // 如果到了环的末尾，则取第一个节点（闭合成环）[cite: 11]
        if (it == m_ring.end()) {
            return m_ring.begin()->second;
        }
        return it->second;
    }

private:
    uint32_t Hash(const std::string& key) {
        // 使用简单的 FNV-1a 哈希或 std::hash，确保分布均匀
        static std::hash<std::string> hash_fn;
        return static_cast<uint32_t>(hash_fn(key));
    }

    int m_virtual_node_num;
    std::map<uint32_t, std::string> m_ring; // 模拟哈希环[cite: 11]
};

