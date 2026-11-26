#pragma once
#include "network/types.h"
#include <functional>
#include <map>
#include <mutex>
#include <vector>

namespace ss {

// leader-side generator for total-order sequence numbers and logical timestamps
struct Sequencer {
    // return the next monotonic sequence number
    SeqNum next_seq() { return m_seq++; }

    // return the next logical clock timestamp
    LogicalTs next_ts() { return m_ts++; }

    // peek at the next sequence number to be issued
    SeqNum current_seq() const { return m_seq; }

private:
    // next sequence number to stamp onto a multicast command
    SeqNum m_seq{1};

    // logical clock counter for synchronized captures
    LogicalTs m_ts{0};
};

// worker-side reorder buffer that delivers multicast packets strictly in sequence order
struct HoldBackQueue {
    // invoked for each missing sequence number detected in a gap
    using NackFn    = std::function<void(SeqNum)>;

    // invoked in strict sequence order as contiguous packets become ready
    using DeliverFn = std::function<void(SeqNum, const std::vector<uint8_t>&)>;

    // initialize queue expecting `first` as the next deliverable sequence number
    HoldBackQueue(SeqNum first, NackFn nack, DeliverFn deliver);

    // buffer an incoming packet, emit nacks for any gaps, and deliver ready packets
    void insert(SeqNum seq, std::vector<uint8_t> payload);

private:
    // deliver all contiguous buffered packets starting at m_expected
    void drain();

    // next sequence number expected for ordered delivery
    SeqNum m_expected;

    // callback fired when a missing sequence number is detected
    NackFn m_nack;

    // callback fired for each packet delivered in order
    DeliverFn m_deliver;

    // out-of-order packets keyed by sequence number
    std::map<SeqNum, std::vector<uint8_t>> m_buf;

    // guards concurrent inserts from network threads
    std::mutex m_mtx;
};

} // namespace ss
