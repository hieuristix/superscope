#include "server.h"
#include "network/messages.h"
#include "network/framing.h"
#include "stitcher.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace ss {

LeaderCtx::LeaderCtx(NodeId id, uint16_t cp, const std::string& mg, uint16_t mp, const ITopology* topo)
    : m_id(id), m_mcast_group(mg), m_mcast_port(mp), m_topo(topo)
    , m_srv_sock(Socket::tcp_server(cp))
    , m_mcast_sock(Socket::mcast_sender(mg)) {}

LeaderCtx::~LeaderCtx() { stop(); }

void LeaderCtx::start() {
    m_running    = true;
    m_accept_thr = std::thread(&LeaderCtx::accept_loop, this);
}

void LeaderCtx::stop() {
    m_running = false;
    m_queue_cv.notify_all();
    m_srv_sock.close();
    if (m_accept_thr.joinable()) m_accept_thr.join();
}

void LeaderCtx::issue_capture() {
    SeqNum    s = m_seq.next_seq();
    LogicalTs t = m_seq.next_ts();
    CapturePayload p{};
    uint32_t shi = htonl((uint32_t)(s>>32)), slo = htonl((uint32_t)(s&0xFFFFFFFF));
    uint32_t thi = htonl((uint32_t)(t>>32)), tlo = htonl((uint32_t)(t&0xFFFFFFFF));
    std::memcpy(&p.seq_num,    &shi, 4); std::memcpy((uint8_t*)&p.seq_num+4,    &slo, 4);
    std::memcpy(&p.logical_ts, &thi, 4); std::memcpy((uint8_t*)&p.logical_ts+4, &tlo, 4);

    PacketHeader h = {
        .magic       = {'S', 'S'},
        .type        = MsgType::CaptureCmd,
        ._pad        = 0,
        .sender_id   = m_id,
        .payload_len = sizeof(p),
    };
    auto hb = encode_header(h);
    hb.insert(hb.end(), (uint8_t*)&p, (uint8_t*)&p+sizeof(p));

    sockaddr_in d{}; d.sin_family=AF_INET; d.sin_port=htons(m_mcast_port);
    inet_pton(AF_INET, m_mcast_group.c_str(), &d.sin_addr);
    sendto(m_mcast_sock.fd(), hb.data(), hb.size(), 0, (sockaddr*)&d, sizeof(d));
}

void LeaderCtx::on_final_tile(const Tile& t) {
    std::lock_guard<std::mutex> lk(m_tiles_mtx);
    m_tiles[t.seq] = t;
}

QueueStatusPayload LeaderCtx::queue_status(NodeId client_id) {
    std::lock_guard<std::mutex> lk(m_queue_mtx);
    QueueStatusPayload st = {
        .position = 0,
        .total    = (uint32_t)m_job_queue.size(),
    };
    for (size_t i = 0; i < m_job_queue.size(); ++i) {
        if (m_job_queue[i] == client_id) {
            st.position = (uint32_t)(i + 1);
            break;
        }
    }
    return st;
}

void LeaderCtx::accept_loop() {
    while (m_running) {
        sockaddr_in ca{}; socklen_t cl = sizeof(ca);
        int cfd = accept(m_srv_sock.fd(), (sockaddr*)&ca, &cl);
        if (cfd < 0) break;
        std::thread([this, cfd]() {
            handle_client(cfd);
            ::close(cfd);
        }).detach();
    }
}

void LeaderCtx::handle_client(int fd) {
    std::vector<uint8_t> req;
    if (!recv_framed(fd, req)) return;

    PacketHeader h{};
    if (!decode_header(req.data(), req.size(), h)) return;

    if (h.type == MsgType::QueueQuery) {
        QueueStatusPayload st = queue_status(h.sender_id);
        QueueStatusPayload wire = {
            .position = htonl(st.position),
            .total    = htonl(st.total),
        };
        PacketHeader rh = {
            .magic       = {'S', 'S'},
            .type        = MsgType::QueueStatus,
            ._pad        = 0,
            .sender_id   = m_id,
            .payload_len = sizeof(wire),
        };
        auto out = encode_header(rh);
        out.insert(out.end(), (uint8_t*)&wire, (uint8_t*)&wire + sizeof(wire));
        send_framed(fd, out);
        return;
    }

    if (h.type == MsgType::SwarmQuery) {
        auto ids = m_topo ? m_topo->all_ids() : std::vector<NodeId>{m_id};
        SwarmStatusPayload st = {
            .size = htonl((uint32_t)ids.size()),
        };
        uint32_t plen = (uint32_t)(sizeof(st) + ids.size() * sizeof(uint32_t));
        PacketHeader rh = {
            .magic       = {'S', 'S'},
            .type        = MsgType::SwarmStatus,
            ._pad        = 0,
            .sender_id   = m_id,
            .payload_len = plen,
        };
        auto out = encode_header(rh);
        out.insert(out.end(), (uint8_t*)&st, (uint8_t*)&st + sizeof(st));
        for (NodeId nid : ids) {
            uint32_t be = htonl(nid);
            out.insert(out.end(), (uint8_t*)&be, (uint8_t*)&be + sizeof(be));
        }
        send_framed(fd, out);
        return;
    }

    if (h.type != MsgType::JobRequest) return;

    {
        std::unique_lock<std::mutex> lk(m_queue_mtx);
        m_job_queue.push_back(h.sender_id);
        m_queue_cv.wait(lk, [&]() {
            return !m_running || (!m_job_queue.empty() && m_job_queue.front() == h.sender_id);
        });
    }

    if (m_running) {
        issue_capture();
        SeqNum last = m_seq.current_seq() - 1;
        for (int i = 0; i < 60 && m_running; ++i) {
            {
                std::lock_guard<std::mutex> lk(m_tiles_mtx);
                auto it = m_tiles.find(last);
                if (it != m_tiles.end()) {
                    auto buf = Stitcher::to_buffer(it->second);
                    send_framed(fd, buf);
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    {
        std::lock_guard<std::mutex> lk(m_queue_mtx);
        if (!m_job_queue.empty() && m_job_queue.front() == h.sender_id)
            m_job_queue.pop_front();
    }
    m_queue_cv.notify_all();
}

WorkerCtx::WorkerCtx(NodeId id, const CameraFOV& fov, const OrbitalCoords& orbit,
                     const std::string& mg, uint16_t mp, uint16_t trp)
    : m_id(id), m_orbit(orbit), m_mcast_group(mg), m_mcast_port(mp)
    , m_tile_recv_port(trp)
    , m_sensor(fov)
    , m_hbq(1,
        [this](SeqNum s){ std::cerr << "[worker " << m_id << "] NACK seq=" << s << "\n"; },
        [this](SeqNum s, const std::vector<uint8_t>& p){ on_capture(s, p); })
    , m_agg(id,
        [this](const Endpoint& dst, const Tile& t){ send_tile(dst, t); },
        nullptr)
    , m_mcast_sock(Socket::mcast_recv(mg, mp))
    , m_tile_srv(Socket::tcp_server(trp)) {}

WorkerCtx::~WorkerCtx() { stop(); }

void WorkerCtx::start() {
    m_running   = true;
    m_mcast_thr = std::thread(&WorkerCtx::mcast_loop,     this);
    m_tile_thr  = std::thread(&WorkerCtx::tile_recv_loop, this);
}

void WorkerCtx::stop() {
    m_running = false;
    m_mcast_sock.close(); m_tile_srv.close();
    if (m_mcast_thr.joinable()) m_mcast_thr.join();
    if (m_tile_thr.joinable())  m_tile_thr.join();
}

void WorkerCtx::update_topology(const std::vector<NodeInfo>& members, NodeId leader_id) {
    m_agg.update_topology(members, leader_id);
}

void WorkerCtx::mcast_loop() {
    uint8_t buf[2048];
    while (m_running) {
        ssize_t n = recv(m_mcast_sock.fd(), buf, sizeof(buf), 0);
        if (n < (ssize_t)sizeof(PacketHeader)) continue;
        PacketHeader h{};
        if (!decode_header(buf, (size_t)n, h)) continue;
        if (h.type != MsgType::CaptureCmd || h.payload_len < sizeof(CapturePayload)) continue;
        CapturePayload cp{}; std::memcpy(&cp, buf+sizeof(PacketHeader), sizeof(cp));
        uint32_t hi, lo;
        std::memcpy(&hi, &cp.seq_num, 4); std::memcpy(&lo, (uint8_t*)&cp.seq_num+4, 4);
        SeqNum seq = ((uint64_t)ntohl(hi)<<32)|ntohl(lo);
        std::vector<uint8_t> pl(buf+sizeof(PacketHeader), buf+sizeof(PacketHeader)+h.payload_len);
        m_hbq.insert(seq, std::move(pl));
    }
}

void WorkerCtx::tile_recv_loop() {
    while (m_running) {
        sockaddr_in pa{}; socklen_t pl = sizeof(pa);
        int pfd = accept(m_tile_srv.fd(), (sockaddr*)&pa, &pl);
        if (pfd < 0) break;
        while (m_running) {
            std::vector<uint8_t> frm;
            if (!recv_framed(pfd, frm)) break;
            Tile t;
            if (decode_tile(frm.data(), frm.size(), t)) m_agg.on_remote_tile(t.id, t);
        }
        ::close(pfd);
    }
}

void WorkerCtx::on_capture(SeqNum seq, const std::vector<uint8_t>&) {
    Tile local = m_sensor.capture(m_id, seq, m_orbit);
    m_agg.submit_local(local);
}

void WorkerCtx::send_tile(const Endpoint& dst, const Tile& t) {
    try {
        Socket c = Socket::tcp_client(dst.ip, dst.port);
        send_framed(c.fd(), encode_tile(t));
    } catch (const std::exception& e) {
        std::cerr << "[worker " << m_id << "] send tile failed: " << e.what() << "\n";
    }
}

ServerNode::ServerNode(NodeId id, const Endpoint& tcp, const Endpoint& hb,
                       const CameraFOV& fov, const OrbitalCoords& orbit,
                       std::unique_ptr<ITopology> topo, uint16_t disc_port)
    : m_id(id), m_tcp(tcp), m_hb(hb), m_fov(fov), m_orbit(orbit)
    , m_topo(std::move(topo))
{
    m_disc = std::make_unique<Discovery>(m_id, m_tcp, m_hb, disc_port);
    m_fd   = std::make_unique<FailureDetector>(
        m_id, m_hb,
        [this](NodeId dead){ on_fail(dead); });
    m_elec = std::make_unique<Election>(
        m_id,
        [this](const Endpoint& dst, const std::vector<uint8_t>& msg){ send_election_msg(dst, msg); },
        [this](NodeId leader){ on_elected(leader); });
}

ServerNode::~ServerNode() { stop(); }

void ServerNode::start() {
    m_running = true;
    transition(NodeState::Discovering);
    m_elec_srv = Socket::tcp_server(m_tcp.port);
    m_elec_thr = std::thread(&ServerNode::elec_loop, this);
    m_disc_thr = std::thread(&ServerNode::disc_loop, this);
    m_fd->start();
    sync_protocols();
    m_disc->broadcast_join();
    m_boot_thr = std::thread(&ServerNode::bootstrap_loop, this);
}

void ServerNode::stop() {
    m_running = false;
    m_elec_srv.close();
    if (m_boot_thr.joinable()) m_boot_thr.join();
    if (m_disc_thr.joinable()) m_disc_thr.join();
    if (m_elec_thr.joinable()) m_elec_thr.join();
    if (m_leader) m_leader->stop();
    if (m_worker) m_worker->stop();
    if (m_fd)     m_fd->stop();
}

void ServerNode::sync_protocols() {
    auto members = m_topo->all_members();
    if (m_elec)   m_elec->on_membership_changed(members);
    if (m_fd)     m_fd->update_peers(members);
    if (m_worker) m_worker->update_topology(members, m_leader_id.load());
}

void ServerNode::bootstrap_loop() {
    int disc_ticks = 0;
    int leader_ticks = 0;
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (!m_running) break;
        NodeState st = m_state.load();
        if (st == NodeState::Discovering) {
            if (++disc_ticks < 3) {
                m_disc->broadcast_join();
            } else {
                transition(NodeState::Electing);
                m_elec->start_election();
            }
        } else if (st == NodeState::Leader) {
            if (++leader_ticks >= 4) {
                leader_ticks = 0;
                m_disc->broadcast_view(m_topo->all_members());
            }
        }
    }
}

void ServerNode::transition(NodeState s) { m_state = s; }

void ServerNode::become_leader() {
    if (m_state == NodeState::Leader && m_leader) return;
    if (m_worker) { m_worker->stop(); m_worker.reset(); }
    transition(NodeState::Leader);
    uint16_t client_port = (m_tcp.ip == "127.0.0.1") ? (uint16_t)(kClientPort + m_id) : kClientPort;
    m_leader = std::make_unique<LeaderCtx>(m_id, client_port, kMulticastGroup, kMulticastPort, m_topo.get());
    m_leader->start();
    std::cout << "[server " << m_id << "] became LEADER (" << m_tcp.ip << ":" << client_port << ")\n" << std::flush;
}

void ServerNode::become_worker(NodeId leader_id) {
    if (m_leader) { m_leader->stop(); m_leader.reset(); }
    if (m_worker) { m_worker->stop(); m_worker.reset(); }
    transition(NodeState::Worker);
    uint16_t tp = (m_tcp.ip == "127.0.0.1") ? (uint16_t)(kTilePortBase + m_id) : kTilePortBase;
    m_worker = std::make_unique<WorkerCtx>(m_id, m_fov, m_orbit, kMulticastGroup, kMulticastPort, tp);
    m_worker->update_topology(m_topo->all_members(), leader_id);
    m_worker->start();
    std::cout << "[server " << m_id << "] became WORKER (" << m_tcp.ip << ")\n" << std::flush;
}

void ServerNode::on_joined(const NodeInfo& info, const Endpoint&) {
    size_t before = m_topo->all_ids().size();
    m_topo->add_member(info);
    if (m_topo->all_ids().size() > before) {
        sync_protocols();
        m_disc->broadcast_view(m_topo->all_members());
    } else if (m_state == NodeState::Leader) {
        m_disc->broadcast_view(m_topo->all_members());
    }
}

void ServerNode::on_view(const std::vector<NodeInfo>& members) {
    size_t before = m_topo->all_ids().size();
    for (const auto& m : members) m_topo->add_member(m);
    if (m_topo->all_ids().size() > before) {
        sync_protocols();
        if (m_state == NodeState::Leader)
            m_disc->broadcast_view(m_topo->all_members());
    }
    if (m_state == NodeState::Discovering && m_topo->all_ids().size() > 1) {
        transition(NodeState::Electing);
        m_elec->start_election();
    }
}

void ServerNode::on_fail(NodeId dead) {
    m_topo->remove_member(dead);
    sync_protocols();
    transition(NodeState::Electing);
    m_elec->start_election();
}

void ServerNode::on_elected(NodeId leader) {
    m_leader_id = leader;
    if (leader == m_id) become_leader();
    else                become_worker(leader);
}

void ServerNode::disc_loop() {
    while (m_running)
        m_disc->poll_once(
            [this](const NodeInfo& i, const Endpoint& ep) { on_joined(i, ep); },
            [this](const std::vector<NodeInfo>& m)        { on_view(m); },
            [](NodeId, const Endpoint&){},
            [this](const Endpoint& requester) {
                if (m_state == NodeState::Leader) {
                    uint16_t cp = (m_tcp.ip == "127.0.0.1") ? (uint16_t)(kClientPort + m_id) : kClientPort;
                    m_disc->reply_leader(requester, {m_tcp.ip, cp});
                }
            });
}

void ServerNode::elec_loop() {
    while (m_running) {
        sockaddr_in pa{}; socklen_t pl = sizeof(pa);
        int pfd = accept(m_elec_srv.fd(), (sockaddr*)&pa, &pl);
        if (pfd < 0) break;
        std::vector<uint8_t> frm;
        if (recv_framed(pfd, frm)) {
            PacketHeader h{};
            if (decode_header(frm.data(), frm.size(), h) && h.type == MsgType::ElectionMsg) {
                std::vector<uint8_t> payload(frm.begin() + sizeof(PacketHeader), frm.end());
                m_elec->on_message(h.sender_id, payload);
            }
        }
        ::close(pfd);
    }
}

void ServerNode::send_election_msg(const Endpoint& dst, const std::vector<uint8_t>& payload) {
    PacketHeader h = {
        .magic       = {'S', 'S'},
        .type        = MsgType::ElectionMsg,
        ._pad        = 0,
        .sender_id   = m_id,
        .payload_len = (uint32_t)payload.size(),
    };
    auto pkt = encode_header(h);
    pkt.insert(pkt.end(), payload.begin(), payload.end());

    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            Socket c = Socket::tcp_client(dst.ip, dst.port);
            if (send_framed(c.fd(), pkt)) return;
        } catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

} // namespace ss
