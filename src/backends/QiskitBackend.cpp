#include "backends/QiskitBackend.hpp"
#include "utils/logger.hpp"
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <fstream>
#include <cstdlib>    

static const char* STATE_FILE = "backend_state.txt";
static auto& logger = Qmio::Logger::get("QPUBackend");

// ── Constructor ───────────────────────────────────────────────────────────────
QiskitBackend::QiskitBackend(
    const std::string& name
)
    : Qiskit::providers::BackendV2(name)
    , _backend(std::make_unique<QPUBackend>())
{
    build_target(32);
}

// Returns true if the Slurm job is still in the queue
static bool slurm_job_alive(const std::string& job_id)
{
    std::string cmd = "squeue -j " + job_id + " -h 2>/dev/null | wc -l";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;
    int count = 0;
    fscanf(pipe, "%d", &count);
    pclose(pipe);
    return count > 0;
}


// ── build_target ──────────────────────────────────────────────────────────────
void QiskitBackend::build_target(Qiskit::uint32_t num_qubits)
{
    using namespace Qiskit::transpiler;
    using namespace Qiskit::circuit;

    auto name_map = get_standard_gate_name_mapping();

    // Build the Rust target directly — bypass the buggy lazy build_target() path
    QkTarget* t = qk_target_new(num_qubits);

    // ── SX (1-qubit) ──────────────────────────────────────────────────────────
    QkTargetEntry* sx_entry = qk_target_entry_new(name_map.at("sx").gate_map());
    for (uint32_t i = 0; i < num_qubits; i++) {
        qk_target_entry_add_property(sx_entry, &i, 1, 1.0, 0.0);
    }
    qk_target_add_instruction(t, sx_entry);

    // ── X (1-qubit) ───────────────────────────────────────────────────────────
    QkTargetEntry* x_entry = qk_target_entry_new(name_map.at("x").gate_map());
    for (uint32_t i = 0; i < num_qubits; i++) {
        qk_target_entry_add_property(x_entry, &i, 1, 2.0, 0.0);
    }
    qk_target_add_instruction(t, x_entry);

    // ── RZ (1-qubit) ──────────────────────────────────────────────────────────
    QkTargetEntry* rz_entry = qk_target_entry_new(name_map.at("rz").gate_map());
    for (uint32_t i = 0; i < num_qubits; i++) {
        qk_target_entry_add_property(rz_entry, &i, 1, 0.0, 0.0);
    }
    qk_target_add_instruction(t, rz_entry);

    // ── ECR ──────────────────────────────────────────────────────────
    
    static const std::vector<std::pair<uint32_t,uint32_t>> ecr_edges = {
        { 0,  1}, { 2,  1}, { 2,  3}, { 4,  3}, { 5,  4},
        { 6,  3}, { 6, 13}, { 7,  0}, { 7, 10}, {10, 11},
        {12, 11}, {12, 13}, {14, 22}, {15, 12}, {15, 19},
        {16,  9}, {16, 17}, {19, 18}, {19, 20}, {21, 20},
        {23, 22}, {23, 31}, {24, 21}, {24, 30}, {25, 17},
        {25, 26}, {26, 27}, {28, 27}, {28, 29}, {30, 29},
        {30, 31}, { 8,  9}
    };

    QkTargetEntry* ecr_entry = qk_target_entry_new(name_map.at("ecr").gate_map());
    for (auto& [ctrl, tgt] : ecr_edges) {
        uint32_t qargs[2] = {ctrl, tgt};
        qk_target_entry_add_property(ecr_entry, qargs, 2, 5.0, 0.0);
    }
    qk_target_add_instruction(t, ecr_entry);

    // ── Measure ───────────────────────────────────────────────────────────────
    QkTargetEntry* measure = qk_target_entry_new_measure();
    for (uint32_t i = 0; i < num_qubits; i++) {
        qk_target_entry_add_property(measure, &i, 1, 0.0, 0.0);
    }
    qk_target_add_instruction(t, measure);

    // Hand ownership to Target(QkTarget*) — sets is_set_=true, num_qubits_ from
    // qk_target_num_qubits(t), and never calls the buggy build_target()
    _target = Target(t);
}

// ── target ────────────────────────────────────────────────────────────────────
const Qiskit::transpiler::Target& QiskitBackend::target() {
    return _target;
}

void QiskitBackend::connect()
{
    // ── Check for existing tunnel ─────────────────────────────
    std::ifstream state(STATE_FILE);
    if (state.good()) {
        std::string endpoint, job_id;
        std::getline(state, endpoint);
        std::getline(state, job_id);
        state.close();
 
        if (!endpoint.empty() && !job_id.empty() && slurm_job_alive(job_id)) {
            logger.info("[QPUBackend] Reusing slurm job " + job_id + " at " + endpoint);
            _backend->attach(endpoint);   
            return;
        }
 
        // State file is stale — remove it and fall through to fresh connect
        std::remove(STATE_FILE);
    }
 
    // ── Fresh connect ─────────────────────────────────────────
    _backend->connect();
 
    // Save endpoint + job id for subsequent calls
    if (!_backend->slurm_job().empty()) {
        std::ofstream out(STATE_FILE);
        out << _backend->endpoint()  << "\n";
        out << _backend->slurm_job() << "\n";
    }
}
 
void QiskitBackend::disconnect()
{
    _backend->disconnect();
    //std::remove(STATE_FILE);
}


std::string QiskitBackend::circuit_to_qasm2(
    Qiskit::circuit::QuantumCircuit& circ
) {
    Qmio::assert_native_gates(circ);

    std::string qasm = circ.to_qasm3();

    // 1. Header
    qasm = std::regex_replace(qasm,
        std::regex(R"(OPENQASM 3\.0;)"),
        "OPENQASM 2.0;");

    // 2. Include — no sx definition, it's already in qelib1.inc
    qasm = std::regex_replace(qasm,
        std::regex(R"(include "stdgates\.inc";)"),
        "include \"qelib1.inc\";");

    // 3. Remove gate rzx definition — mirrors Python's re.sub
    qasm = std::regex_replace(qasm,
        std::regex(R"(\ngate rzx[^\n]*\n)"),
        "\n");

    // 4. Replace gate ecr definition with opaque version — mirrors Python
    qasm = std::regex_replace(qasm,
        std::regex(R"(\ngate ecr[^}]*\})"),
        "");

    // 5. qubit[N] name → qreg name[N]
    qasm = std::regex_replace(qasm,
        std::regex(R"(qubit\[(\d+)\] (\w+);)"),
        "qreg $2[$1];");

    // 6. bit[N] name → creg name[N]
    qasm = std::regex_replace(qasm,
        std::regex(R"(bit\[(\d+)\] (\w+);)"),
        "creg $2[$1];");

    // 7. c[i] = measure q[j]; → measure q[j] -> c[i];
    qasm = std::regex_replace(qasm,
        std::regex(R"((\w+\[\d+\]) = measure (\w+\[\d+\]);)"),
        "measure $2 -> $1;");

    return qasm;
}

// ── run(pubs, shots) — BackendV2 override ────────────────────────────────────
std::shared_ptr<Qiskit::providers::Job> QiskitBackend::run(
    std::vector<Qiskit::primitives::SamplerPub>& pubs,
    Qiskit::uint_t                               shots
) {
    try {
        Qiskit::uint_t effective_shots =
            (shots > 0) ? shots : static_cast<Qiskit::uint_t>(pubs[0].shots());

        std::string qasm_str = circuit_to_qasm2(
            const_cast<Qiskit::circuit::QuantumCircuit&>(pubs[0].circuit())
        );

        std::string json_str = _backend->run(qasm_str, effective_shots);

        return std::make_shared<Qmio::QmioJob>(json_str, pubs);

    } catch (const std::exception& e) {
        std::cerr << "[QiskitBackend] Run error: " << e.what() << "\n";
        std::vector<Qiskit::primitives::SamplerPub> empty;
        return std::make_shared<Qmio::QmioJob>("", empty);
    }
}

// ── run(circuit, shots) — convenience overload ───────────────────────────────
std::shared_ptr<Qmio::QmioJob> QiskitBackend::run(
    Qiskit::circuit::QuantumCircuit& circ,
    int                              shots,
    std::optional<double>            repetition_period,
    int                              optimization,
    const std::string&               res_format
) {
    try {
        Qiskit::circuit::QuantumCircuit circ_copy = circ.copy();
        Qiskit::primitives::SamplerPub pub(circ_copy, shots);
        std::vector<Qiskit::primitives::SamplerPub> pubs = {pub};

        std::string qasm_str = circuit_to_qasm2(circ_copy);

        logger.debug(std::string("QASM sent:\n") + qasm_str);

        std::string json_str = _backend->run(
            qasm_str, shots, repetition_period, optimization, res_format
        );

        return std::make_shared<Qmio::QmioJob>(json_str, pubs);

    } catch (const std::exception& e) {
        logger.debug(std::string("[QiskitBackend] Run error: ") + e.what());
        std::vector<Qiskit::primitives::SamplerPub> empty;
        return std::make_shared<Qmio::QmioJob>("", empty);
    }
}