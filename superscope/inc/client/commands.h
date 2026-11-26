#pragma once
#include <string>

namespace ss {

// supported interactive commands in the client repl
enum class ClientCmd {
    Help,
    Leader,
    Swarm,
    Queue,
    Capture,
    Status,
    Quit,
    Unknown,
};

// metadata describing one client repl command
struct CmdSpec {
    ClientCmd id;

    // primary command keyword
    const char* name;

    // syntax hint printed by help
    const char* usage;

    // short description of what the command does
    const char* desc;
};

// master list of all commands exposed in the client repl
static constexpr CmdSpec kClientCommands[] = {
    {ClientCmd::Help,    "help",    "help",               "List available commands"},
    {ClientCmd::Leader,  "leader",  "leader",             "Query swarm for the current leader"},
    {ClientCmd::Swarm,   "swarm",   "swarm",              "Query leader for swarm size and active nodes"},
    {ClientCmd::Queue,   "queue",   "queue",              "Query this client's position in the leader's job queue"},
    {ClientCmd::Capture, "capture", "capture [out.ppm]",  "Submit a panorama capture job to the leader"},
    {ClientCmd::Status,  "status",  "status",             "Show client ID and cached leader"},
    {ClientCmd::Quit,    "quit",    "quit",               "Exit the client"},
};

// map a typed command word to its ClientCmd enum value
inline ClientCmd parse_command(const std::string& tok) {
    if (tok == "help" || tok == "?") return ClientCmd::Help;
    if (tok == "leader")             return ClientCmd::Leader;
    if (tok == "swarm")              return ClientCmd::Swarm;
    if (tok == "queue")              return ClientCmd::Queue;
    if (tok == "capture")            return ClientCmd::Capture;
    if (tok == "status")             return ClientCmd::Status;
    if (tok == "quit" || tok == "exit" || tok == "q") return ClientCmd::Quit;
    return ClientCmd::Unknown;
}

} // namespace ss
