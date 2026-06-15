#define _USE_MATH_DEFINES
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include "backends/QiskitBackend.hpp"
#include "circuit/quantumcircuit.hpp"
#include "compiler/transpiler.hpp"

// Usage: bell <shots> <steps>
int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: bell <shots> <steps>\n";
        return 1;
    }

    int shots = std::atoi(argv[1]);
    int steps = std::atoi(argv[2]);

    // ── Connect ──────────────────────────────────────────
    QiskitBackend backend;
    backend.connect();

    // ── Open CSV ─────────────────────────────────────────
    std::ofstream fout("results.csv");
    fout << "theta,n00,n11,n01,n10,E\n";
    fout << std::fixed << std::setprecision(6);

    // ── Loop: rebuild circuit per angle, transpile each time ─
    for (int i = 0; i <= steps; ++i)
    {
        double angle = (2.0 * M_PI * i) / steps;

        // The four measurement configurations for the CHSH test
        std::vector<std::string> obs_vec = {"00", "01", "10", "11"};

        for (const auto& el : obs_vec)
        {
            // Initialize the 2-qubit circuit
            Qiskit::circuit::QuantumCircuit circ(2, 2);
            circ.h(0);
            circ.cx(0, 1);
            circ.ry(Qiskit::circuit::Parameter(angle), 0);

            // Change measurement bases based on the CHSH observer configuration
            // If el[0] == '1', qubit 0 is measured in X-basis 
            if (el[0] == '1') {
                circ.h(0);
            }
            // If el[1] == '1', qubit 1 is measured in X-basis 
            if (el[1] == '1') {
                circ.h(1);
            }

            // Measure 
            circ.measure(0, 0);
            circ.measure(1, 1);

            // Run
            auto transpiled = Qiskit::compiler::transpile(circ, backend, 2);
            auto job        = backend.run(transpiled, shots);
            auto result     = job->result();
            auto pub_result = result[0];
            auto bits       = pub_result.data();
            auto counts     = bits.get_counts();

            // Lambda helper to safely extract counts
            auto get = [&](const std::string& k) -> int {
                auto it = counts.find(k);
                return (it != counts.end()) ? it->second : 0;
            };

            int n00 = get("00");
            int n11 = get("11");
            int n01 = get("01");
            int n10 = get("10");
            
            // Expectation value <ZZ> for this specific basis choice
            double E = static_cast<double>(n00 + n11 - n01 - n10) / shots;

            // Log the data, including which basis ('el') was used
            fout << angle << "," << n00 << "," << n11 << ","
                << n01   << "," << n10 << "," << E   << "\n";
        }
    }

    // ── Close CSV ─────────────────────────────────────────────
    fout.close();

    return 0;
}
