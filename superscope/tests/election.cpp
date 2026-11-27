#include "protocols/election.h"
#include <cassert>
#include <deque>
#include <iostream>
#include <memory>
#include <unordered_map>

using namespace ss;

static NodeInfo node_info(NodeId id) {
    return {.id = id, .tcp = {"127.0.0.1", (uint16_t)(10000 + id)}, .hb = {"127.0.0.1", (uint16_t)(11000 + id)}};
}

struct TestRing {
    std::unordered_map<NodeId, std::unique_ptr<Election>> nodes;
    std::unordered_map<NodeId, NodeId>                    leader;
    std::deque<std::pair<NodeId, std::vector<uint8_t>>>   queue;

    void set(const std::vector<NodeId>& ids) {
        std::vector<NodeInfo> members;
        for (NodeId id : ids) {
            if (!nodes.count(id)) {
                nodes[id] = std::make_unique<Election>(
                    id,
                    [this](const Endpoint& dst, const std::vector<uint8_t>& msg) {
                        queue.push_back({dst.port - 10000, msg});
                    },
                    [this, id](NodeId l) { leader[id] = l; });
            }
            members.push_back(node_info(id));
        }
        for (NodeId id : ids) nodes[id]->on_membership_changed(members);
    }

    void run(const std::vector<NodeId>& starters) {
        for (NodeId id : starters) nodes[id]->start_election();
        while (!queue.empty()) {
            auto [to, msg] = std::move(queue.front());
            queue.pop_front();
            nodes[to]->on_message_lcr(msg);
        }
    }

    void expect(const std::vector<NodeId>& ids, NodeId expected) {
        for (NodeId id : ids) {
            assert(leader[id] == expected);
            assert(!nodes[id]->in_progress());
        }
    }
};

int main() {
    TestRing ring;

    // single initiator & simultaneous start
    ring.set({9, 2, 17, 5});
    ring.run({2});
    ring.expect({9, 2, 17, 5}, 17);

    ring.run({9, 2, 17, 5});
    ring.expect({9, 2, 17, 5}, 17);

    // leader 17 dies -> 9 wins
    ring.set({9, 2, 5});
    ring.run({2, 5, 9});
    ring.expect({9, 2, 5}, 9);

    // higher node 42 joins -> 42 wins
    ring.set({9, 2, 5, 42});
    ring.run({42});
    ring.expect({9, 2, 5, 42}, 42);

    // lone node elects itself without sending
    NodeId lone_leader = 0;
    Election lone(42, [](const Endpoint&, const std::vector<uint8_t>&) { assert(false); },
                      [&](NodeId l) { lone_leader = l; });
    lone.on_membership_changed({node_info(42)});
    lone.start_election();
    assert(lone_leader == 42 && !lone.in_progress());

    // absent from view / bad packets -> no-op
    Election stray(42, [](const Endpoint&, const std::vector<uint8_t>&) { assert(false); },
                       [](NodeId) { assert(false); });
    stray.on_membership_changed({node_info(43)});
    stray.start_election();
    stray.on_message_lcr({});
    stray.on_message_lcr({99, 0, 0, 0, 1});
    assert(!stray.in_progress());

    std::cout << "election tests passed\n";
}
