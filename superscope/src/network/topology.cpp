#include "network/topology.h"
#include <algorithm>

namespace ss {

RingTopology::RingTopology(NodeId local_id) : m_local_id(local_id) {}

void RingTopology::add_member(const NodeInfo& info) {
    std::lock_guard<std::mutex> lk(m_mtx);
    if (m_info.count(info.id)) return;
    m_info[info.id] = info;
    m_order.push_back(info.id);
    std::sort(m_order.begin(), m_order.end());
}

void RingTopology::remove_member(NodeId id) {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_info.erase(id);
    m_order.erase(std::remove(m_order.begin(), m_order.end(), id), m_order.end());
}

void RingTopology::set_members(const std::vector<NodeInfo>& members) {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_order.clear(); m_info.clear();
    for (const auto& m : members) {
        if (m_info.count(m.id)) continue;
        m_info[m.id] = m;
        m_order.push_back(m.id);
    }
    std::sort(m_order.begin(), m_order.end());
}

int RingTopology::local_index() const {
    for (int i = 0; i < (int)m_order.size(); ++i)
        if (m_order[i] == m_local_id) return i;
    return -1;
}

std::optional<Endpoint> RingTopology::next_hop() const {
    std::lock_guard<std::mutex> lk(m_mtx);
    if (m_order.size() < 2) return std::nullopt;
    int i = local_index();
    if (i < 0) return std::nullopt;
    return m_info.at(m_order[(i + 1) % m_order.size()]).tcp;
}

std::optional<Endpoint> RingTopology::upstream_hop() const { return next_hop(); }

std::vector<NodeInfo> RingTopology::peers() const {
    std::lock_guard<std::mutex> lk(m_mtx);
    std::vector<NodeInfo> out;
    int n = (int)m_order.size();
    if (n < 2) return out;
    int i = local_index();
    if (i < 0) return out;
    out.push_back(m_info.at(m_order[(i - 1 + n) % n]));
    out.push_back(m_info.at(m_order[(i + 1) % n]));
    return out;
}

std::vector<NodeId> RingTopology::all_ids() const {
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_order;
}

std::vector<NodeInfo> RingTopology::all_members() const {
    std::lock_guard<std::mutex> lk(m_mtx);
    std::vector<NodeInfo> out;
    out.reserve(m_order.size());
    for (NodeId id : m_order) out.push_back(m_info.at(id));
    return out;
}

} // namespace ss
