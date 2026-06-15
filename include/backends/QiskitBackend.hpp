#ifndef __qiskit_backend_hpp__
#define __qiskit_backend_hpp__

#include "backends/QPUBackend.hpp"
#include "backends/QmioJob.hpp"
#include "circuit/quantumcircuit.hpp"
#include "providers/backend.hpp"
#include "transpiler/target.hpp"
#include "utils/types.hpp"
#include "utils/circuit_validator.hpp"
#include <memory>
#include <string>
#include <regex>
#include <optional>

class QiskitBackend : public Qiskit::providers::BackendV2 {
public:
    // Default constructor — creates its own QPUBackend
    QiskitBackend(
        const std::string& name       = "qmio"
    );

    // ── BackendV2 overrides ───────────────────────────────────────────────────
    const Qiskit::transpiler::Target& target() override;

    std::shared_ptr<Qiskit::providers::Job> run(
        std::vector<Qiskit::primitives::SamplerPub>& pubs,
        Qiskit::uint_t                               shots = 0
    ) override;

    // ── Convenience overload ──────────────────────────────────────────────────
    std::shared_ptr<Qmio::QmioJob> run(
        Qiskit::circuit::QuantumCircuit& circ,
        int                              shots,
        std::optional<double>            repetition_period = std::nullopt,
        int                              optimization      = 0,
        const std::string&               res_format        = "binary_count"
    );

    void connect();
    void disconnect();

private:
    std::unique_ptr<QPUBackend>        _backend;   // owned
    Qiskit::transpiler::Target         _target;

    void build_target(Qiskit::uint32_t num_qubits);
    std::string circuit_to_qasm2(Qiskit::circuit::QuantumCircuit& circ);
};

#endif // __qiskit_backend_hpp__