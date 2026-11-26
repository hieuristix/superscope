#include "protocols/election.h"
#include <cassert>
#include <deque>
#include <memory>
#include <unordered_map>
#include <utility>

namespace {

struct Message {
    ss::NodeId from;
    ss::NodeId to;
    std::vector<uint8_t> payload;
};

ss::NodeInfo member(ss::NodeId id) {
    return {
        .id = id,
        .tcp = {"127.0.0.1", static_cast<uint16_t>(10000 + id)},
        .hb = {"127.0.0.1", static_cast<uint16_t>(11000 + id)},
    };
}

void deliver_all(std::deque<Message>& messages,
                 std::unordered_map<ss::NodeId, ss::Election*>& nodes) {
    while (!messages.empty()) {
        Message message = std::move(messages.front());
        messages.pop_front();
        nodes.at(message.to)->on_message(message.from, message.payload);
    }
}

} // namespace

int main() {
    const std::vector<ss::NodeId> ids{9, 2, 17, 5};
    std::deque<Message> messages;
    std::unordered_map<ss::NodeId, ss::Election*> nodes;
    std::unordered_map<ss::NodeId, ss::NodeId> endpoint_ids;
    std::unordered_map<ss::NodeId, int> election_count;
    std::unordered_map<ss::NodeId, ss::NodeId> elected_leader;
    std::vector<std::unique_ptr<ss::Election>> elections;
    const std::vector<ss::NodeInfo> members{
        member(9), member(2), member(17), member(5),
    };

    for (ss::NodeId id : ids) {
        endpoint_ids[10000 + id] = id;
        elections.push_back(std::make_unique<ss::Election>(
            id,
            [&, id](const ss::Endpoint& endpoint, const std::vector<uint8_t>& payload) {
                messages.push_back({id, endpoint_ids.at(endpoint.port), payload});
            },
            [&, id](ss::NodeId leader) {
                ++election_count[id];
                elected_leader[id] = leader;
            }));
        nodes[id] = elections.back().get();
        elections.back()->on_membership_changed(members);
    }

    for (ss::NodeId id : ids) nodes.at(id)->start_election();
    deliver_all(messages, nodes);

    for (ss::NodeId id : ids) {
        assert(election_count[id] == 1);
        assert(elected_leader[id] == 17);
        assert(!nodes.at(id)->in_progress());
    }

    const std::vector<ss::NodeInfo> reduced_members{
        member(9), member(2), member(5),
    };
    for (auto& election : elections)
        election->on_membership_changed(reduced_members);

    std::unordered_map<ss::NodeId, int> re_election_count;
    for (ss::NodeId id : ids) {
        if (id == 17) continue;
        nodes.at(id)->start_election();
    }
    while (!messages.empty()) {
        Message message = std::move(messages.front());
        messages.pop_front();
        nodes.at(message.to)->on_message(message.from, message.payload);
    }
    for (ss::NodeId id : {2, 5, 9}) {
        assert(election_count[id] == 2);
        assert(elected_leader[id] == 9);
        assert(!nodes.at(id)->in_progress());
    }

    ss::Election single(42, [](const ss::Endpoint&, const std::vector<uint8_t>&) {
        assert(false);
    }, [&](ss::NodeId leader) {
        assert(leader == 42);
        ++re_election_count[42];
    });
    single.on_membership_changed({member(42)});
    single.start_election();
    assert(re_election_count[42] == 1);

    ss::Election invalid_view(42, [](const ss::Endpoint&, const std::vector<uint8_t>&) {
        assert(false);
    }, [](ss::NodeId) {
        assert(false);
    });
    invalid_view.on_membership_changed({member(43)});
    invalid_view.start_election();
    assert(!invalid_view.in_progress());
}
