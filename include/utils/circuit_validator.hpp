#ifndef __qmio_circuit_validator_hpp__
#define __qmio_circuit_validator_hpp__

#include "circuit/quantumcircuit.hpp"
#include "circuit/circuitinstruction.hpp"
#include <set>
#include <string>
#include <vector>

namespace Qmio {

// Native gates supported by the QMIO QPU
extern const std::set<std::string> NATIVE_GATES;

// Returns names of non-native gates found in the circuit
// Empty vector means all gates are native
std::vector<std::string> find_non_native_gates(
    Qiskit::circuit::QuantumCircuit& circ
);

// Throws a descriptive error if any non-native gate is found
void assert_native_gates(
    Qiskit::circuit::QuantumCircuit& circ
);

} // namespace Qmio
#endif // __qmio_circuit_validator_hpp__