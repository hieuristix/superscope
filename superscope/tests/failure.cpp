#include "protocols/failure.h"
#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

namespace {

uint16_t unused_udp_port() {
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    assert(fd >= 0);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    const int bind_result = bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    assert(bind_result == 0);

    socklen_t length = sizeof(address);
    const int name_result = getsockname(fd, reinterpret_cast<sockaddr*>(&address), &length);
    assert(name_result == 0);
    const uint16_t port = ntohs(address.sin_port);
    close(fd);
    return port;
}

ss::NodeInfo member(ss::NodeId id, uint16_t port) {
    return {
        .id = id,
        .tcp = {"127.0.0.1", static_cast<uint16_t>(port + 1)},
        .hb = {"127.0.0.1", port},
    };
}

} // namespace

int main() {
    const uint16_t port_a = unused_udp_port();
    uint16_t port_b = unused_udp_port();
    while (port_b == port_a) port_b = unused_udp_port();

    std::mutex mutex;
    std::condition_variable changed;
    int failures = 0;
    ss::FailureDetector detector_a(1, {"127.0.0.1", port_a}, [&](ss::NodeId id) {
        assert(id == 2);
        {
            std::lock_guard<std::mutex> lk(mutex);
            ++failures;
        }
        changed.notify_one();
    });
    ss::FailureDetector detector_b(2, {"127.0.0.1", port_b}, [](ss::NodeId) {});

    detector_a.update_peers({member(1, port_a), member(2, port_b)});
    detector_b.update_peers({member(1, port_a), member(2, port_b)});
    detector_a.start();
    detector_b.start();

    std::this_thread::sleep_for(std::chrono::seconds(2));
    assert(detector_a.is_alive(2));
    assert(detector_b.is_alive(1));

    detector_b.stop();
    {
        std::unique_lock<std::mutex> lk(mutex);
        assert(changed.wait_for(lk, std::chrono::seconds(3), [&] {
            return failures == 1;
        }));
    }
    assert(!detector_a.is_alive(2));
    detector_a.stop();
}
