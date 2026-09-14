cmake_minimum_required(VERSION 3.16)
project(qmio-qiskitcpp-apps CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# Resolve roots from module environment variables
# ==============================================================================
set(QMIO_ROOT       "$ENV{EBROOTQMIOMINQISKITCPP}")
set(ZMQPP_ROOT      "$ENV{EBROOTZMQPP}")
set(QISKIT_ROOT     "$ENV{EBROOTQISKITMINIBMMINRUNTIMEMINC}")
set(QISKITCPP_ROOT  "$ENV{EBROOTQISKITMINCPP}")
set(NLOHMANN_ROOT   "$ENV{EBROOTNLOHMANN_JSON}")
set(PYTHON_ROOT     "$ENV{EBROOTPYTHON}")
set(ZEROMQ_ROOT     "$ENV{EBROOTZEROMQ}")

foreach(VAR QMIO_ROOT ZMQPP_ROOT QISKIT_ROOT QISKITCPP_ROOT NLOHMANN_ROOT PYTHON_ROOT ZEROMQ_ROOT)
    if(NOT EXISTS "${${VAR}}")
        message(FATAL_ERROR "${VAR} not found: ${${VAR}} — did you load the modules?")
    endif()
endforeach()

# ==============================================================================
# Imported targets
# ==============================================================================
add_library(qmio SHARED IMPORTED)
set_target_properties(qmio PROPERTIES
    IMPORTED_LOCATION               "${QMIO_ROOT}/lib/libqmio.so"
    INTERFACE_INCLUDE_DIRECTORIES "${QMIO_ROOT}/include/qmio;${ZMQPP_ROOT}/include;${QISKIT_ROOT}/include;${QISKITCPP_ROOT}/src;${NLOHMANN_ROOT}/include"
)

add_library(zmqpp SHARED IMPORTED)
set_target_properties(zmqpp PROPERTIES
    IMPORTED_LOCATION  "${ZMQPP_ROOT}/lib/libzmqpp.so"
    IMPORTED_NO_SONAME TRUE
)

add_library(zmq SHARED IMPORTED)
set_target_properties(zmq PROPERTIES
    IMPORTED_LOCATION  "${ZEROMQ_ROOT}/lib/libzmq.so"
    IMPORTED_NO_SONAME TRUE
)

add_library(qiskit SHARED IMPORTED)
set_target_properties(qiskit PROPERTIES
    IMPORTED_LOCATION "${QISKIT_ROOT}/lib/libqiskit.so"
)

add_library(python311 SHARED IMPORTED)
set_target_properties(python311 PROPERTIES
    IMPORTED_LOCATION "${PYTHON_ROOT}/lib/libpython3.11.so"
    INTERFACE_INCLUDE_DIRECTORIES "${PYTHON_ROOT}/include/python3.11"
)

target_link_libraries(qmio INTERFACE zmqpp zmq qiskit python311)

# ==============================================================================
# Executables
# ==============================================================================
foreach(TARGET bell_circuit basic_circuit)
    add_executable(${TARGET} examples/${TARGET}.cpp)
    target_link_libraries(${TARGET} PRIVATE qmio)
    target_link_options(${TARGET} PRIVATE
        -Wl,--allow-multiple-definition
        -Wl,-rpath,${QMIO_ROOT}/lib
        -Wl,-rpath,${ZMQPP_ROOT}/lib
        -Wl,-rpath,${ZEROMQ_ROOT}/lib
        -Wl,-rpath,${QISKIT_ROOT}/lib
        -Wl,-rpath,${PYTHON_ROOT}/lib
    )
endforeach()

# ==============================================================================
# Install
# ==============================================================================
include(GNUInstallDirs)

install(DIRECTORY scripts/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/scripts
)

install(TARGETS qmio
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qmio
)

install(TARGETS bell_circuit basic_circuit
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)