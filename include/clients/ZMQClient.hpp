#pragma once
#include <string>
#include <optional>
#include <zmqpp/zmqpp.hpp>
#include <nlohmann/json.hpp>

class ZMQClient {
public:
    explicit ZMQClient(const std::string& address);
    ~ZMQClient();

    // Disable copy, allow move
    ZMQClient(const ZMQClient&)            = delete;
    ZMQClient& operator=(const ZMQClient&) = delete;
    ZMQClient(ZMQClient&&)                 = default;
    ZMQClient& operator=(ZMQClient&&)      = default;

    // Send a (circuit, config) job to the server
    void send_job(const std::string& circuit, const std::string& config);

    // Block until result is received from server
    std::string await_results();

    // Close the socket
    void close();

private:
    zmqpp::context _context;
    zmqpp::socket  _socket;
    std::string    _address;
    bool           _closed = false; 

    // Internal receive — returns nullopt if nothing available
    std::optional<std::string> check_received();
};