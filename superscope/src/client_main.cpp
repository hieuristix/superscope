#include "client.h"
#include "client/repl.h"
#include <limits>
#include <random>
#include <string>

int main(int argc, char* argv[]) {
    using namespace ss;

    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> id_dist(1, std::numeric_limits<uint32_t>::max());
    NodeId id = id_dist(rng);

    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--id") id = (NodeId)std::stoul(argv[++i]);
    }

    Client client(id);
    ClientRepl repl(client);
    repl.run();
    return 0;
}
