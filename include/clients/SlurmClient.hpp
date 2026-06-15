#pragma once
#include <string>
#include <utility>

class SlurmClient {
public:
    explicit SlurmClient(
        const std::string& time_limit       = "",
        const std::string& reservation_name = ""
    );

    void scancel(const std::string& job_id = "");
    bool is_job_running(const std::string& job_id = "");

    // Submit tunnel job and wait — returns {job_id, endpoint}
    std::pair<std::string, std::string> submit_and_wait(
        const std::string& backend,
        const std::string& time_limit = ""
    );

private:
    std::string _job_id;
    int         _endpoint_port;
    std::string _tunnel_time_limit;
    std::string _reservation_name;
    int         _max_retries;

    std::string check_backend_node(const std::string& backend);
};