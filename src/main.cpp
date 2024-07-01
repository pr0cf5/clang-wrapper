#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "sha256.hpp"

namespace fs = std::filesystem;

class CompilerInvocation {
public:
    std::string cc_or_cxx;
    std::string output;
    std::vector<std::string> other_args;
    bool autotest;

    CompilerInvocation(int argc, char* argv[]) : output(""), autotest(false) {
        int i = 1;
        cc_or_cxx = argv[0];
        while (i < argc) {
            std::string arg = argv[i];
            if (arg == "-o" && i + 1 < argc) {
                output = argv[++i];
            } else {
                if (arg.find("autotest") != std::string::npos) {
                    autotest = true;
                }
                other_args.push_back(arg);
            }
            ++i;
        }
    }

    std::vector<std::string> output_dependencies_ci(const std::string& output_file) {
        std::vector<std::string> args = {cc_or_cxx, "-MM", "-o", output_file};
        args.insert(args.end(), other_args.begin(), other_args.end());
        return args;
    }
};

std::string sha256_hex(const std::string& str) {
    SHA256 hash;
    hash.update(str);
    auto digest = hash.digest();
    std::ostringstream ss;
    for (const auto& byte : digest) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    return ss.str();
}

void run_command(const std::vector<std::string>& command) {
    std::vector<char*> args;
    for (const auto& arg : command) {
        args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(nullptr);

    pid_t pid = fork();
    if (pid == 0) {
        execvp(args[0], args.data());
        _exit(EXIT_FAILURE);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            exit(WEXITSTATUS(status));
        } else {
            exit(EXIT_FAILURE);
        }
    } else {
        std::cerr << "Fork failed" << std::endl;
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char* argv[]) {
    std::string clang_name;
    if (strcmp(argv[0], "cc_wrapper") == 0) {
        clang_name = "clang";
    } else if (strcmp(argv[0], "cxx_wrapper") == 0) {
        clang_name = "clang++";
    } else {
        throw std::runtime_error("Unknown wrapper type");
    }

    fs::create_directories("/out/compile_commands/filesys_snapshots");
    std::string cmd_line;
    for (int i = 0; i < argc; ++i) {
        cmd_line += argv[i];
        cmd_line += " ";
    }
    std::string compile_command_id = sha256_hex(cmd_line);
    std::vector<std::string> real_argv = {clang_name};
    for (int i = 1; i < argc; ++i) {
        real_argv.push_back(argv[i]);
    }
    CompilerInvocation ci(argc, argv);
    if (ci.output.empty() || ci.autotest) {
        run_command(real_argv);
    }

    char temp_dep_file[] = "/tmp/depsXXXXXX";
    int fd = mkstemp(temp_dep_file);
    if (fd == -1) {
        throw std::runtime_error("Failed to create temporary file");
    }
    close(fd);

    std::vector<std::string> dep_ci = ci.output_dependencies_ci(temp_dep_file);
    run_command(dep_ci);

    std::ifstream ifs(temp_dep_file);
    std::string deps((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();
    fs::remove(temp_dep_file);

    fs::create_directories("/out/compile_commands/" + compile_command_id);
    std::ofstream ofs("/out/compile_commands/" + compile_command_id + "/deps.txt");
    ofs << deps;
    ofs.close();

    return 0;
}
