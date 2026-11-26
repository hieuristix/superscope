#pragma once
#include "client.h"
#include <string>

namespace ss {

// interactive command parser and output loop backed by a Client instance
// it's mostly used for parsing the command
struct ClientRepl {
    // attach repl to an existing network client
    explicit ClientRepl(Client& client);

    // read, parse, and dispatch commands from stdin until quit or eof
    void run();

private:
    // print all supported commands and their usage
    void print_help() const;

    // print the local client id and cached leader
    void print_status() const;

    // dispatch leader lookup and print result
    void handle_leader();

    // dispatch swarm size query and print result
    void handle_swarm();

    // dispatch queue position query and print result
    void handle_queue();

    // dispatch capture request and write returned bytes to out_path
    void handle_capture(const std::string& out_path);

    // underlying network client
    Client& m_client;
};

} // namespace ss
