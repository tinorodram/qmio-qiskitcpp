#ifndef __qmio_job_hpp__
#define __qmio_job_hpp__

#include "primitives/containers/sampler_pub.hpp"
#include "primitives/containers/sampler_pub_result.hpp"
#include "primitives/containers/primitive_result.hpp"
#include "circuit/quantumcircuit.hpp"
#include "circuit/classicalregister.hpp"
#include "providers/job.hpp"
#include "utils/types.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Qmio {

class QmioJob : public Qiskit::providers::Job {
public:
    QmioJob() = default;

    QmioJob(
        const std::string&                           json_str,
        std::vector<Qiskit::primitives::SamplerPub>& pubs
    );

    // ── providers::Job pure virtual overrides ─────────────────────────────────
    Qiskit::providers::JobStatus status()                                  override;
    Qiskit::uint_t               num_results()                             override;
    bool                         result(Qiskit::uint_t index,
                                        Qiskit::primitives::SamplerPubResult& result) override;

    // ── Additional interface ──────────────────────────────────────────────────
    Qiskit::primitives::PrimitiveResult result();

    // ── Convenience status helpers (not overrides) ────────────────────────────
    bool done();
    bool running();
    bool cancelled();
    bool in_final_state();
    bool cancel();

protected:
    void read_results();

private:
    std::string                                  raw_json_;
    nlohmann::ordered_json                       results_;
    std::vector<Qiskit::primitives::SamplerPub>  pubs_;
    Qiskit::uint_t                               num_results_ = 0;
    bool                                         has_error_   = false;
    bool                                         is_cancelled_= false;
    std::string                                  error_message_;
};

} // namespace Qmio
#endif // __qmio_job_hpp__