#pragma once
#include <string>
#include <cstdlib>

// Reads ZMQ_SERVER from environment variable
inline std::string get_zmq_server() {
    const char* val = std::getenv("ZMQ_SERVER");
    return val ? std::string(val) : "";
}

inline const std::string ZMQ_SERVER            = get_zmq_server();
inline const std::string TUNNEL_TIME_LIMIT     = "00:03:00";
inline const std::string MAX_TUNNEL_TIME_LIMIT = "00:15:00";

// Slurm scripts live inside the project — set at build time via -DSCRIPTS_DIR
#ifndef SCRIPTS_DIR
    #define SCRIPTS_DIR "./scripts/"
#endif
inline const std::string SLURM_SCRIPTS_DIR = SCRIPTS_DIR;