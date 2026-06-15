#include "backends/QiskitBackend.hpp"
#include "circuit/quantumcircuit.hpp"
#include "compiler/transpiler.hpp"
#include <iostream>

int main() {
    QiskitBackend backend;
    backend.connect();

    Qiskit::circuit::QuantumCircuit circ(2, 2);
    circ.h(0);
    circ.cx(0, 1);
    circ.measure(0, 0);
    circ.measure(1, 1);

    auto transpiled = Qiskit::compiler::transpile(circ, backend,0);

    transpiled.draw();

    int shots = 500;

    auto job        = backend.run(transpiled, shots);
    auto result     = job->result();
    auto pub_result = result[0];
    auto bits       = pub_result.data();
    auto counts     = bits.get_counts();

    std::cout << "===== counts =====" << std::endl;
    for (auto c : counts) {
        std::cout << c.first << " : " << c.second << std::endl;
    }

    backend.disconnect();
    return 0;
}