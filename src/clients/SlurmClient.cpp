#include "clients/SlurmClient.hpp"
#include "utils/errors.hpp"        // ← renamed from utils
#include "utils/config.hpp"
#include "utils/logger.hpp"
#include <iostream>
#include <regex>
#include <random>
#include <chrono>
#include <thread>
#include <stdexcept>

static auto& logger = Qmio::Logger::get("QPUBackend");

SlurmClient::SlurmClient(
    const std::string& time_limit,
    const std::string& reservation_name
)
    : _job_id("")
    , _endpoint_port(0)
    , _tunnel_time_limit(time_limit.empty() ? TUNNEL_TIME_LIMIT : time_limit)
    , _reservation_name(reservation_name)
    , _max_retries(288000)
{}

void SlurmClient::scancel(const std::string& job_id) {
    const std::string& id = job_id.empty() ? _job_id : job_id;
    if (id.empty()) return;
    try {
        run("scancel " + id);
        logger.info("Cancelled job: "+ _job_id);
    } catch (const RunCommandError& e) {
        logger.debug(std::string("[SlurmClient] scancel error: ") + e.what());
    }
}

bool SlurmClient::is_job_running(const std::string& job_id) {
    const std::string& id = job_id.empty() ? _job_id : job_id;
    if (id.empty()) return false;
    try {
        auto [out, err] = run("scontrol show job " + id);
        return out.find("RUNNING") != std::string::npos;
    } catch (const RunCommandError&) {
        return false;
    }
}

std::string SlurmClient::check_backend_node(const std::string& backend) {
    if (backend.empty())
        throw BackendError("No backend specified");

    auto [out, err] = run("scontrol show partition " + backend);

    // Nodes=c7-23 → 10.120.7.23
    std::regex  pattern(R"(Nodes=c(\d+)-(\d+))");
    std::smatch match;

    if (std::regex_search(out, match, pattern)) {
        std::string ip = "10.120." + match[1].str() + "." + match[2].str();
        return ip;
    }

    throw BackendError("Could not parse node IP from partition: " + backend);
}

std::pair<std::string, std::string> SlurmClient::submit_and_wait(
    const std::string& backend,
    const std::string& time_limit
) {
    if (backend.empty())
        throw std::invalid_argument("Backend not specified");

    // Random port 600–699
    std::random_device rd;
    std::mt19937 rng(rd());
    _endpoint_port = std::uniform_int_distribution<int>(600, 699)(rng);

    const std::string& tl = !time_limit.empty()        ? time_limit
                          : !_tunnel_time_limit.empty() ? _tunnel_time_limit
                          : TUNNEL_TIME_LIMIT;

    // Build sbatch command pointing to project scripts/
    std::string submit_cmd;
    if (_reservation_name.empty()) {
        submit_cmd = "sbatch --time=" + tl
                   + " " + SLURM_SCRIPTS_DIR + backend + ".sh"
                   + " " + std::to_string(_endpoint_port);
    } else {
        submit_cmd = "sbatch --reservation='" + _reservation_name + "'"
                   + " --time=" + tl
                   + " " + SLURM_SCRIPTS_DIR + backend + ".sh"
                   + " " + std::to_string(_endpoint_port);
    }

    logger.debug(std::string("shell command") + submit_cmd);

    auto [out, err] = run(submit_cmd);

    // Parse "Submitted batch job 12345"
    std::regex  id_pat(R"(Submitted batch job (\d+))");
    std::smatch id_match;
    if (!std::regex_search(out, id_match, id_pat))
        throw OutputParsingError("Failed to parse job ID from: " + out);

    _job_id = id_match[1].str();
    logger.info("Slurm job sent. ID: " + _job_id);

    logger.info("Waiting for resources...");

    int count = 0;
    while (!is_job_running(_job_id) && count < _max_retries) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (count % 30 == 0)
        count++;
    }

    if (count >= _max_retries)
        throw std::runtime_error("Job did not start within time limit");

    logger.info("Slurm job started");

    std::string ip       = check_backend_node(backend);
    std::string endpoint = "tcp://" + ip + ":" + std::to_string(_endpoint_port);

    return {_job_id, endpoint};
}