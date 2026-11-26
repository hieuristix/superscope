#include "protocols/sequencer.h"

namespace ss {

HoldBackQueue::HoldBackQueue(SeqNum first, NackFn nack, DeliverFn deliver)
    : m_expected(first), m_nack(std::move(nack)), m_deliver(std::move(deliver)) {}

void HoldBackQueue::insert(SeqNum seq, std::vector<uint8_t> payload) {
    std::lock_guard<std::mutex> lk(m_mtx);
    if (seq < m_expected) return;
    m_buf[seq] = std::move(payload);
    for (SeqNum gap = m_expected; gap < seq; ++gap)
        if (!m_buf.count(gap) && m_nack) m_nack(gap);
    drain();
}

void HoldBackQueue::drain() {
    while (true) {
        auto it = m_buf.find(m_expected);
        if (it == m_buf.end()) break;
        if (m_deliver) m_deliver(m_expected, it->second);
        m_buf.erase(it);
        ++m_expected;
    }
}

} // namespace ss
