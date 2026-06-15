#include "backends/QPUBackend.hpp"
#include "utils/errors.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdio>

static auto& logger = Qmio::Logger::get("QPUBackend");

// ── Optimization map ──────────────────────────────────────────────────────────
static int build_optimization(int level) {
    switch (level) {
        case 0: return 1;
        case 1: return 18;
        case 2: return 30;
        default:
            throw std::invalid_argument(
                "Invalid optimization level: " + std::to_string(level)
            );
    }
}

// ── Result format map ─────────────────────────────────────────────────────────
static std::pair<int,int> build_res_format(const std::string& fmt) {
    if (fmt == "binary_count")                return {1, 3};
    if (fmt == "raw")                         return {1, 2};
    if (fmt == "binary")                      return {2, 2};
    if (fmt == "squash_binary_result_arrays") return {2, 6};
    throw std::invalid_argument("Invalid result format: " + fmt);
}

// ── Slurm job alive check ─────────────────────────────────────────────────────
static bool slurm_job_alive(const std::string& job_id) {
    if (job_id.empty()) return false;
    std::string cmd = "squeue -j " + job_id + " -h 2>/dev/null | wc -l";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;
    int count = 0;
    fscanf(pipe, "%d", &count);
    pclose(pipe);
    return count > 0;
}

// ── Constructor ───────────────────────────────────────────────────────────────
QPUBackend::QPUBackend(
    const std::string& address,
    const std::string& time_limit,
    const std::string& reservation_name
)
    : _endpoint(!address.empty() ? address : ZMQ_SERVER)
    , _job_id("")
    , _tunnel_time_limit(time_limit)
    , _reservation_name(reservation_name)
    , _client(nullptr)
    , _slurmclient(nullptr)
{
}

// ── Destructor ────────────────────────────────────────────────────────────────
QPUBackend::~QPUBackend() {
    disconnect();
}

// ── attach ────────────────────────────────────────────────────────────────────
void QPUBackend::attach(const std::string& endpoint) {
    _endpoint = endpoint;
    if (!_client)
        _client = std::make_unique<ZMQClient>(endpoint);
}

// ── Accessors ─────────────────────────────────────────────────────────────────
std::string QPUBackend::endpoint()  const { return _endpoint; }
std::string QPUBackend::slurm_job() const { return _job_id;   }

// ── connect ───────────────────────────────────────────────────────────────────
void QPUBackend::connect() {

    // Check for a running job from a previous bell call
    std::ifstream state("backend_state.txt");
    if (state.good()) {
        std::string endpoint, job_id;
        std::getline(state, endpoint);
        std::getline(state, job_id);
        state.close();

        if (slurm_job_alive(job_id)) {
            _endpoint = endpoint;
            _job_id   = job_id;
            if (!_client)
                _client = std::make_unique<ZMQClient>(_endpoint);
            return;
        }
        std::remove("backend_state.txt");
    }

    // Fresh connect
    if (_endpoint.empty()) {
        if (!_slurmclient) {
            _slurmclient = std::make_unique<SlurmClient>(
                _tunnel_time_limit,
                _reservation_name
            );
        }

        auto [job_id, endpoint] = _slurmclient->submit_and_wait("qpu");
        _job_id   = job_id;
        _endpoint = endpoint;
    }

    if (_endpoint.empty()) {
        throw std::runtime_error(
            "No endpoint available. Set ZMQ_SERVER env var."
        );
    }

    if (!_client) {
        _client = std::make_unique<ZMQClient>(_endpoint);
    }

    // Save state so the next bell call can reuse this job
    std::ofstream out("backend_state.txt");
    out << _endpoint << "\n" << _job_id << "\n";
}

// ── disconnect ────────────────────────────────────────────────────────────────
void QPUBackend::disconnect() {
    if (!_job_id.empty() && _slurmclient) {
        while (_slurmclient->is_job_running(_job_id)) {
            try { _slurmclient->scancel(_job_id); } catch (...) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        logger.info("Connection terminated");
        _job_id   = "";
        _endpoint = "";
    }
    std::remove("backend_state.txt");
    if (_client) {
        _client->close();
        _client.reset();
    }
}

// ── _state_flush ──────────────────────────────────────────────────────────────
void QPUBackend::_state_flush() {
    disconnect();
    connect();
}

// ── build_config ──────────────────────────────────────────────────────────────
std::string QPUBackend::build_config(
    int                   shots,
    std::optional<double> repetition_period,
    int                   optimization,
    const std::string&    res_format
) {
    auto [inlineProc, resFmt] = build_res_format(res_format);
    int opt_value             = build_optimization(optimization);

    std::ostringstream ss;
    ss << R"({"$type":"<class 'qat.purr.compiler.config.CompilerConfig'>","$data":{)"
       << R"("repeats":)" << shots << ","
       << R"("repetition_period":)";

    if (repetition_period.has_value())
        ss << repetition_period.value();
    else
        ss << "null";

    ss << ","
       << R"("results_format":{"$type":"<class 'qat.purr.compiler.config.QuantumResultsFormat'>","$data":{)"
       << R"("format":{"$type":"<enum 'qat.purr.compiler.config.InlineResultsProcessing'>","$value":)"
       << inlineProc << "},"
       << R"("transforms":{"$type":"<enum 'qat.purr.compiler.config.ResultsFormatting'>","$value":)"
       << resFmt << "}"
       << "}},"
       << R"("metrics":{"$type":"<enum 'qat.purr.compiler.config.MetricsType'>","$value":6},)"
       << R"("active_calibrations":[],)"
       << R"("optimizations":{"$type":"<enum 'qat.purr.compiler.config.TketOptimizations'>","$value":)"
       << opt_value << "}"
       << "}}";

    return ss.str();
}

// ── run ───────────────────────────────────────────────────────────────────────
std::string QPUBackend::run(
    const std::string&    circuit,
    int                   shots,
    std::optional<double> repetition_period,
    int                   optimization,
    const std::string&    res_format
) {
    if (!_client)
        throw std::runtime_error("Not connected. Call connect() first.");

    // If job ended unexpectedly — flush and reconnect
    if (!_job_id.empty() && _slurmclient &&
        !_slurmclient->is_job_running(_job_id)) {
        _state_flush();
    }

    std::string config = build_config(
        shots, repetition_period, optimization, res_format
    );

    _client->send_job(circuit, config);
    return _client->await_results();
}