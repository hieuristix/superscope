#include "server.h"
#include "logger.h"
#include "network/socket.h"
#include <limits>
#include <random>
#include <thread>

int main(int argc, char* argv[]) {
    using namespace ss;
    ss_init_logger();

    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> id_dist(1, std::numeric_limits<uint32_t>::max());
    std::uniform_real_distribution<double> alt_dist(300.0, 1200.0);
    std::uniform_real_distribution<double> inc_dist(0.0, 180.0);
    std::uniform_real_distribution<double> anom_dist(0.0, 360.0);

    NodeId      id = id_dist(rng);
    std::string ip = detect_local_ipv4();
    OrbitalCoords orbit = {
        .altitude_km     = alt_dist(rng),
        .inclination_deg = inc_dist(rng),
        .anomaly_deg     = anom_dist(rng),
    };

    for (int i = 1; i < argc - 1; ++i) {
        std::string arg = argv[i];
        if (arg == "--id")        id = (NodeId)std::stoul(argv[++i]);
        else if (arg == "--ip")   ip = argv[++i];
        else if (arg == "--alt")  orbit.altitude_km     = std::stod(argv[++i]);
        else if (arg == "--inc")  orbit.inclination_deg = std::stod(argv[++i]);
        else if (arg == "--anom") orbit.anomaly_deg     = std::stod(argv[++i]);
    }

    bool local = (ip == "127.0.0.1");
    uint16_t tcp_port = local ? (uint16_t)(kRingTcpBase + id) : kRingTcpBase;
    uint16_t hb_port  = local ? (uint16_t)(kHbPortBase  + id) : kHbPortBase;

    Endpoint tcp = {.ip = ip, .port = tcp_port};
    Endpoint hb  = {.ip = ip, .port = hb_port};
    CameraFOV fov = {.h_deg = 60, .v_deg = 45, .w = 320, .h = 240};

    ServerNode node(id, tcp, hb, fov, orbit, kDiscoveryPort);
    node.start();

    SS_SERVER_INFO(id, "running at {}:{} (alt={:.1f} inc={:.1f} anom={:.1f})",
                   ip, tcp_port, orbit.altitude_km, orbit.inclination_deg, orbit.anomaly_deg);
    while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
