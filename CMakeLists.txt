cmake_minimum_required(VERSION 3.16)
project(c_qiskit_qmio CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# ==============================================================================
# External paths
# ==============================================================================
set(BASE_DIR "/mnt/Q_SWAP/crodriguez/c_qiskit_qmio")

set(ZMQPP_INC       "$ENV{HOME}/include")
set(ZMQPP_LIB       "$ENV{HOME}/lib")

set(QISKIT_DIST     "${BASE_DIR}/qiskit-ibm-runtime-c/build/qiskit_srcdir/dist/c")
set(QISKIT_INC      "${QISKIT_DIST}/include")
set(QISKIT_LIB      "${QISKIT_DIST}/lib")

set(QISKIT_CPP_INC  "${BASE_DIR}/qiskit-cpp/src")
set(NLOHMANN_INC    "${BASE_DIR}/qiskit-cpp/samples/build/_deps/json-src/include")

set(SCRIPTS_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/scripts/")

set(PYTHON_ROOT "/opt/cesga/qmio/hpc/software/Compiler/gcccore/12.3.0/python/3.11.9")
set(PYTHON_INC  "${PYTHON_ROOT}/include/python3.11")
set(PYTHON_LIB  "${PYTHON_ROOT}/lib/libpython3.11.so")

# ==============================================================================
# External libraries (imported targets)
# ==============================================================================

add_library(python311 SHARED IMPORTED)
set_target_properties(python311 PROPERTIES
    IMPORTED_LOCATION             "${PYTHON_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${PYTHON_INC}"
)

add_library(zmqpp SHARED IMPORTED)
set_target_properties(zmqpp PROPERTIES
    IMPORTED_LOCATION             "${ZMQPP_LIB}/libzmqpp.so.4"
    INTERFACE_INCLUDE_DIRECTORIES "${ZMQPP_INC}"
)

add_library(zmq SHARED IMPORTED)
set_target_properties(zmq PROPERTIES
    IMPORTED_LOCATION "${ZMQPP_LIB}/libzmq.so"
)

add_library(qiskit SHARED IMPORTED)
set_target_properties(qiskit PROPERTIES
    IMPORTED_LOCATION             "${QISKIT_LIB}/libqiskit.so"
    INTERFACE_INCLUDE_DIRECTORIES "${QISKIT_INC}"
)

# ==============================================================================
# qmio shared library
# ==============================================================================
add_library(qmio SHARED
    src/utils/pickle_serializer.cpp
    src/utils/pickle_deserializer.cpp
    src/utils/errors.cpp
    src/utils/circuit_validator.cpp
    src/utils/logger.cpp
    src/clients/ZMQClient.cpp
    src/clients/SlurmClient.cpp
    src/backends/QPUBackend.cpp
    src/backends/QmioJob.cpp
    src/backends/QiskitBackend.cpp
)

target_include_directories(qmio
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${QISKIT_CPP_INC}"
            "${NLOHMANN_INC}"
            "${QISKIT_INC}"
            "${ZMQPP_INC}"
)

target_compile_definitions(qmio
    PRIVATE SCRIPTS_DIR="${SCRIPTS_DIR}"
)

target_link_libraries(qmio
    PUBLIC zmqpp zmq qiskit python311
)

target_link_options(qmio PRIVATE
    -Wl,--allow-multiple-definition
    -Wl,-rpath,${ZMQPP_LIB}
    -Wl,-rpath,${QISKIT_LIB}
    -Wl,-rpath,${Python_ROOT_DIR}/lib
)

# ==============================================================================
# Executables
# ==============================================================================
foreach(TARGET bell_circuit basic_circuit)
    add_executable(${TARGET} ${TARGET}.cpp)
    target_link_libraries(${TARGET} PRIVATE qmio)
    target_link_options(${TARGET} PRIVATE -Wl,-rpath,${CMAKE_CURRENT_BINARY_DIR})
endforeach()


# ==============================================================================
# Install
# ==============================================================================
include(GNUInstallDirs)

install(TARGETS qmio
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qmio
)

install(TARGETS bell_circuit basic_circuit
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)