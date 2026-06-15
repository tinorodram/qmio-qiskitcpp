#include "utils/circuit_validator.hpp"
#include <stdexcept>

namespace Qmio {

// Native gates supported by the QMIO QPU
const std::set<std::string> NATIVE_GATES = {
    "sx", "x", "rz", "ecr", "measure", "barrier"
};

std::vector<std::string> find_non_native_gates(
    Qiskit::circuit::QuantumCircuit& circ
) {
    std::vector<std::string> non_native;
    Qiskit::uint_t n = circ.num_instructions();

    for (Qiskit::uint_t i = 0; i < n; i++) {
        const std::string& name = circ[i].instruction().name();
        if (!NATIVE_GATES.count(name)) {
            // Avoid duplicates
            bool already = false;
            for (auto& g : non_native)
                if (g == name) { already = true; break; }
            if (!already)
                non_native.push_back(name);
        }
    }
    return non_native;
}

void assert_native_gates(
    Qiskit::circuit::QuantumCircuit& circ
) {
    auto non_native = find_non_native_gates(circ);
    if (!non_native.empty()) {
        std::string gates_list;
        for (size_t i = 0; i < non_native.size(); i++) {
            gates_list += "'" + non_native[i] + "'";
            if (i + 1 < non_native.size()) gates_list += ", ";
        }
        throw std::runtime_error(
            "[QmioBackend] Circuit contains non-native gates: " + gates_list + "\n"
            "  Native gates are: sx, x, rz, ecr, measure.\n"
            "  Please transpile your circuit before running:\n"
            "    auto transpiled = Qiskit::compiler::transpile(circ, backend);\n"
            "    auto job = backend.run(transpiled, shots);"
        );
    }
}

} // namespace Qmio