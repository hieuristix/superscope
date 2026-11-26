#include <csignal>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

static std::vector<std::string> g_names;

static int run_cmd(const std::vector<std::string>& args) {
    pid_t pid = fork();
    if (pid == 0) {
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& s : args) argv.push_back(const_cast<char*>(s.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        std::perror("execvp");
        std::_Exit(1);
    }
    int status = 0;
    if (pid > 0) waitpid(pid, &status, 0);
    return status;
}

static void cleanup(int) {
    if (!g_names.empty()) {
        std::vector<std::string> cmd = {"container", "delete", "--force"};
        for (const auto& name : g_names) cmd.push_back(name);
        run_cmd(cmd);
        g_names.clear();
    }
    std::_Exit(0);
}

static bool spawn_container(const std::string& root,
                            const std::string& name,
                            uint32_t id,
                            double alt,
                            double inc,
                            double anom) {
    std::vector<std::string> cmd = {
        "container", "run", "-d",
        "--name", name,
        "-v", root + ":/src",
        "superscope-dev",
        "/src/build-linux/superscope/server",
        "--id",   std::to_string(id),
        "--alt",  std::to_string(alt),
        "--inc",  std::to_string(inc),
        "--anom", std::to_string(anom),
    };
    return run_cmd(cmd) == 0;
}

int main() {
    std::signal(SIGINT,  cleanup);
    std::signal(SIGTERM, cleanup);

#ifdef SS_ROOT_DIR
    std::string root = SS_ROOT_DIR;
#else
    std::string root = std::filesystem::current_path().string();
#endif

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> alt_dist(300.0, 1200.0);
    std::uniform_real_distribution<double> inc_dist(0.0, 180.0);
    std::uniform_real_distribution<double> anom_dist(0.0, 360.0);

    uint32_t next_id = 1;
    std::string line;

    std::cout << "Enter number of satellite containers to launch (or 'q' to quit):\n> " << std::flush;
    while (std::getline(std::cin, line)) {
        if (line == "q" || line == "quit") break;
        if (!line.empty()) {
            try {
                int count = std::stoi(line);
                if (count <= 0) {
                    std::cout << "Enter a positive number.\n";
                } else {
                    for (int i = 0; i < count; ++i) {
                        uint32_t id = next_id++;
                        std::string name = "sat-" + std::to_string(id);
                        if (spawn_container(root, name, id, alt_dist(rng), inc_dist(rng), anom_dist(rng))) {
                            g_names.push_back(name);
                        } else {
                            std::cerr << "Failed to start " << name << "\n";
                        }
                    }
                    std::cout << "Total running containers: " << g_names.size() << "\n";
                }
            } catch (...) {
                std::cout << "Invalid input.\n";
            }
        }
        std::cout << "> " << std::flush;
    }

    cleanup(0);
    return 0;
}
