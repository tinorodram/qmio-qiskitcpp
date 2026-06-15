#include "backends/QmioJob.hpp"
#include <sstream>
#include <stdexcept>

namespace Qmio {

// ── Constructor ───────────────────────────────────────────────────────────────
QmioJob::QmioJob(
    const std::string&                           json_str,
    std::vector<Qiskit::primitives::SamplerPub>& pubs
)
    : Qiskit::providers::Job()
    , raw_json_(json_str)
    , pubs_(pubs)
{}

// ── status ────────────────────────────────────────────────────────────────────
Qiskit::providers::JobStatus QmioJob::status() {
    if (is_cancelled_)     return Qiskit::providers::JobStatus::CANCELLED;
    if (has_error_)        return Qiskit::providers::JobStatus::FAILED;
    if (raw_json_.empty()) return Qiskit::providers::JobStatus::RUNNING;
    return Qiskit::providers::JobStatus::DONE;
}

// ── num_results ───────────────────────────────────────────────────────────────
Qiskit::uint_t QmioJob::num_results() {
    if (num_results_ == 0) read_results();
    return num_results_;
}

// ── result(index, SamplerPubResult&) — providers::Job override ───────────────
bool QmioJob::result(
    Qiskit::uint_t                        index,
    Qiskit::primitives::SamplerPubResult& result
) {
    if (num_results_ == 0) read_results();
    if (index >= num_results_) return false;
    result.set_pub(pubs_[index]);
    result.from_json(results_);
    return true;
}

// ── result() — returns PrimitiveResult ───────────────────────────────────────
Qiskit::primitives::PrimitiveResult QmioJob::result() {
    if (num_results_ == 0) read_results();
    if (has_error_)
        throw std::runtime_error("[QmioJob] Job failed: " + error_message_);

    Qiskit::primitives::PrimitiveResult prim_result;
    prim_result.allocate(pubs_.size());
    for (size_t i = 0; i < pubs_.size(); i++) {
        prim_result[i].set_pub(pubs_[i]);
        prim_result[i].from_json(results_);
    }
    return prim_result;
}

// ── Convenience status helpers ────────────────────────────────────────────────
bool QmioJob::done()          { return status() == Qiskit::providers::JobStatus::DONE;      }
bool QmioJob::running()       { return status() == Qiskit::providers::JobStatus::RUNNING;   }
bool QmioJob::cancelled()     { return status() == Qiskit::providers::JobStatus::CANCELLED; }

bool QmioJob::in_final_state() {
    auto st = status();
    return st == Qiskit::providers::JobStatus::DONE     ||
           st == Qiskit::providers::JobStatus::FAILED   ||
           st == Qiskit::providers::JobStatus::CANCELLED;
}

bool QmioJob::cancel() {
    if (running()) { is_cancelled_ = true; return true; }
    return false;
}

// ── read_results ──────────────────────────────────────────────────────────────
void QmioJob::read_results() {
    try {

        //std::cerr << "[DEBUG] Raw server response:\n" << raw_json_ << "\n";
        auto server = nlohmann::ordered_json::parse(raw_json_);

        if (!server.contains("results")) {
            has_error_     = true;
            error_message_ = "No 'results' key in server response";
            return;
        }

        auto& srv_results = server["results"];

        // Build structure SamplerPubResult::from_json expects:
        // {"data": {"c1": {"num_bits": N, "samples": ["0x0", ...]}}}
        nlohmann::ordered_json pub_result;
        pub_result["data"] = nlohmann::ordered_json::object();

        for (auto creg : pubs_[0].circuit().cregs()) {
            std::string creg_name = creg.name();
            if (!srv_results.contains(creg_name)) continue;

            // Expand counts → samples, convert binary → hex
            nlohmann::ordered_json samples = nlohmann::ordered_json::array();
            for (auto& [bitstring, count] : srv_results[creg_name].items()) {
                int val = std::stoi(bitstring, nullptr, 2);
                std::ostringstream hex;
                hex << "0x" << std::hex << val;
                int n = count.get<int>();
                for (int i = 0; i < n; i++)
                    samples.push_back(hex.str());
            }

            nlohmann::ordered_json creg_data;
            creg_data["num_bits"] = creg.size();
            creg_data["samples"]  = samples;
            pub_result["data"][creg_name] = creg_data;
        }

        results_     = pub_result;
        num_results_ = 1;

    } catch (const std::exception& e) {
        has_error_     = true;
        error_message_ = e.what();
    }
}

} // namespace Qmio
