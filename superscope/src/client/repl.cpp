#include "client/repl.h"
#include "client/commands.h"
#include <cstdio>
#include <iostream>
#include <sstream>

namespace ss {

ClientRepl::ClientRepl(Client& client) : m_client(client) {}

void ClientRepl::run() {
    std::cout << "Client " << m_client.id() << " ready. Type 'help' for commands.\n> " << std::flush;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string tok;
        if (iss >> tok) {
            switch (parse_command(tok)) {
                case ClientCmd::Help:   print_help();   break;
                case ClientCmd::Status: print_status(); break;
                case ClientCmd::Leader: handle_leader(); break;
                case ClientCmd::Swarm:  handle_swarm();  break;
                case ClientCmd::Queue:  handle_queue();  break;
                case ClientCmd::Capture: {
                    std::string out = "panorama.ppm";
                    iss >> out;
                    handle_capture(out);
                    break;
                }
                case ClientCmd::Quit: return;
                case ClientCmd::Unknown:
                    std::cout << "Unknown command '" << tok << "'. Type 'help'.\n";
                    break;
            }
        }
        std::cout << "> " << std::flush;
    }
}

void ClientRepl::print_help() const {
    for (const auto& c : kClientCommands)
        std::cout << "  " << c.usage << "  -  " << c.desc << "\n";
}

void ClientRepl::print_status() const {
    auto leader = m_client.cached_leader();
    std::cout << "client_id=" << m_client.id() << " | leader=";
    if (leader)
        std::cout << leader->id << " (" << leader->ep.ip << ":" << leader->ep.port << ")\n";
    else
        std::cout << "none\n";
}

void ClientRepl::handle_leader() {
    auto leader = m_client.discover_leader();
    if (leader)
        std::cout << "Leader: node " << leader->id
                  << " at " << leader->ep.ip << ":" << leader->ep.port << "\n";
    else
        std::cout << "No leader responded.\n";
}

void ClientRepl::handle_swarm() {
    auto info = m_client.query_swarm();
    if (!info) {
        std::cout << "Failed to query swarm (no reachable leader).\n";
        return;
    }
    std::cout << "Swarm size: " << info->size << " (nodes:";
    for (size_t i = 0; i < info->nodes.size(); ++i)
        std::cout << (i == 0 ? " " : ", ") << info->nodes[i];
    std::cout << ")\n";
}

void ClientRepl::handle_queue() {
    auto info = m_client.query_queue();
    if (!info) {
        std::cout << "Failed to query queue (no reachable leader).\n";
        return;
    }
    if (info->position == 0)
        std::cout << "Not in queue (total queued: " << info->total << ").\n";
    else
        std::cout << "Queue position: " << info->position << " / " << info->total << "\n";
}

void ClientRepl::handle_capture(const std::string& out_path) {
    std::cout << "Submitting capture job...\n";
    auto data = m_client.request_capture();
    if (!data) {
        std::cout << "Capture failed (no reachable leader or no panorama received).\n";
        return;
    }
    FILE* f = std::fopen(out_path.c_str(), "wb");
    if (f) {
        std::fwrite(data->data(), 1, data->size(), f);
        std::fclose(f);
        std::cout << "Saved panorama to " << out_path << " (" << data->size() << " bytes)\n";
    } else {
        std::cout << "Failed to write " << out_path << "\n";
    }
}

} // namespace ss
