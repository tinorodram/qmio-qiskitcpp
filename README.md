# qmio-qiskitcpp

A native C++ interface layer that bridges the **Qiskit C++ SDK** with the **Qmio quantum processing unit (QPU)**, enabling researchers to compile and execute quantum circuits on real quantum hardware directly from a C++ environment — without relying on Python-based Qiskit tooling.

Developed at [CESGA](https://www.cesga.es) (Galicia Supercomputing Center) as part of the Qmio quantum computing infrastructure.

---

## How it works

```
Your C++ code (Qiskit C++ SDK)
        │
        ▼
  QiskitBackend / QPUBackend
        │
        ▼
  ZMQ communication layer
        │
        ▼
  Slurm job scheduler (HPC cluster)
        │
        ▼
  Qmio QPU (real quantum hardware)
```

The library handles circuit serialization, job submission via ZMQ, and QPU node allocation via Slurm — abstracting all hardware communication behind a clean C++ backend interface.

---

## Dependencies

The following must be available in your environment before building:

| Dependency | Purpose |
|---|---|
| `g++` with C++17 | Compiler |
| `libzmq` + `zmqpp` | ZMQ communication with the QPU server |
| `qiskit-ibm-runtime-c` | Qiskit C headers and built library |
| `qiskit-cpp` | Qiskit C++ SDK headers |
| `nlohmann/json` | JSON parsing (fetched automatically by CMake) |
| Slurm | HPC job scheduler (available on the cluster) |

> **Note:** This project is designed to run in an HPC environment with access to the Qmio QPU. The `qmio/hpc` module must be loaded on the cluster.

---

## Build

### 1. Load the required modules (on the cluster)

```bash
ml qmio/hpc gcccore/12.3.0 python/3.11.9
```

### 2. Configure paths

The build system uses overridable variables. Set them to match your environment:

```bash
export ZMQPP_PREFIX=$HOME                          # where zmqpp is installed
export QISKIT_DIST=/path/to/qiskit-ibm-runtime-c/build/qiskit_srcdir/dist/c
```

### 3. Build the static library

```bash
make lib
```

Or with explicit path overrides:

```bash
make lib ZMQPP_PREFIX=/custom/path QISKIT_DIST=/custom/qiskit/dist
```

### 4. Build and run an example circuit

```bash
bash build.sh        # compiles bell_circuit.cpp against the static library
```

---

## Usage

### Running a circuit

After building, set the ZMQ server address and run:

```bash
ZMQ_SERVER=tcp://<ip>:<port> ./circuit
```

### Example circuits

Two example programs are included:

- **`basic_circuit.cpp`** — minimal example to verify the backend connection
- **`bell_circuit.cpp`** — parameterized Bell circuit with angle sweep, writes results to `results.csv`

### Running the sweep

```bash
bash run.sh
```

This runs a full angle sweep over the Bell circuit and collects QPU results into `results.csv`.

---

## Environment variables

| Variable | Description |
|---|---|
| `ZMQ_SERVER` | ZMQ endpoint of the QPU server (e.g. `tcp://192.168.1.1:5555`) |

---

## License

Apache-2.0 — see [LICENSE](LICENSE) for details.