#include "utils/errors.hpp"
#include <array>
#include <cstdio>

std::pair<std::string, std::string> run(const std::string& cmd) {
    std::string stdout_str;
    std::string stderr_str;

    std::string full_cmd = cmd + " 2>/tmp/qmio_cmd_stderr";
    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) {
        throw RunCommandError("popen failed for command: " + cmd);
    }

    std::array<char, 256> buf;
    while (fgets(buf.data(), buf.size(), pipe)) {
        stdout_str += buf.data();
    }

    int ret = pclose(pipe);

    FILE* err_file = fopen("/tmp/qmio_cmd_stderr", "r");
    if (err_file) {
        while (fgets(buf.data(), buf.size(), err_file)) {
            stderr_str += buf.data();
        }
        fclose(err_file);
    }

    if (ret != 0) {
        throw RunCommandError(
            "Command failed: " + cmd + "\nstderr: " + stderr_str
        );
    }

    return {stdout_str, stderr_str};
}