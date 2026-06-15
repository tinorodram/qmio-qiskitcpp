#pragma once
#include <string>
#include <memory>
#include <optional>
#include "clients/ZMQClient.hpp"
#include "clients/SlurmClient.hpp"
 
class QPUBackend {
public:
    QPUBackend(
        const std::string& address          = "",
        const std::string& time_limit       = "",
        const std::string& reservation_name = ""
    );
    ~QPUBackend();
 
    // ── Lifecycle ─────────────────────────────────────────────
    void connect();
    void disconnect();
 
    // Attach to an already-running tunnel (ZMQ only, no Slurm)
    void attach(const std::string& endpoint);
 
    // ── Accessors ─────────────────────────────────────────────
    std::string endpoint()  const;
    std::string slurm_job() const;
 
    // ── Execution ─────────────────────────────────────────────
    std::string run(
        const std::string&    circuit,
        int                   shots,
        std::optional<double> repetition_period = std::nullopt,
        int                   optimization      = 0,
        const std::string&    res_format        = "binary_count"
    );
 
private:
    std::string _endpoint;
    std::string _job_id;
    std::string _tunnel_time_limit;
    std::string _reservation_name;
 
    std::unique_ptr<ZMQClient>   _client;
    std::unique_ptr<SlurmClient> _slurmclient;
 
    std::string build_config(
        int                   shots,
        std::optional<double> repetition_period,
        int                   optimization,
        const std::string&    res_format
    );
    void _state_flush();
};
