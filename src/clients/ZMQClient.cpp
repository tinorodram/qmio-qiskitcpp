#include "clients/ZMQClient.hpp"
#include "utils/pickle_serializer.hpp"
#include "utils/pickle_deserializer.hpp"
#include "utils/logger.hpp"
#include <iostream>
#include <chrono>
#include <stdexcept>

static auto& logger = Qmio::Logger::get("QPUBackend");

using json = nlohmann::json;

ZMQClient::ZMQClient(const std::string& address)
    : _context()
    , _socket(_context, zmqpp::socket_type::request)  // REQ socket mirrors Python
    , _address(address)
{
    // LINGER=0: don't block on close if messages are unsent
    _socket.set(zmqpp::socket_option::linger, 0);
    _socket.connect(_address);
    logger.info("[ZMQClient] Connected to: "+_address);
}

ZMQClient::~ZMQClient() {
    close();
}

void ZMQClient::close() {
    if (_closed) return;    // ← guard against double close
    _closed = true;
    try {
        _socket.close();
    } catch (...) {}
}

void ZMQClient::send_job(const std::string& circuit, const std::string& config) {
    // Serialize (circuit, config) as a Python pickle protocol 4 tuple
    auto payload = pickle::dumps_tuple2(circuit, config);

    // Wrap in a zmqpp message and send
    zmqpp::message msg;
    msg.add_raw(payload.data(), payload.size());

    auto t0 = std::chrono::steady_clock::now();

    bool sent = false;
    while (!sent) {
        try {
            _socket.send(msg);
            sent = true;
        } catch (const std::exception& e) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - t0).count();
            if (elapsed > 30.0) {
                throw std::runtime_error(
                    "[ZMQClient] Timeout sending job to " + _address
                );
            }
            logger.debug(std::string("Send error, retrying: ")+e.what());
        }
    }

    logger.info("Circuit sent to server");
}

std::optional<std::string> ZMQClient::check_received() {
    zmqpp::message msg;
    if (_socket.receive(msg, /* dont_block = */ true)) {
        const void* raw  = msg.raw_data(0);
        size_t      size = msg.size(0);

        // Deserialize pickle → json → string
        json result = pickle::loads(
            reinterpret_cast<const uint8_t*>(raw), size
        );

        // DEBUG: print what the server actually sent
        //std::cout << "[ZMQClient] Server response:\n"
        //          << result.dump(2) << "\n";

        // Return as JSON string for QiskitBackend to parse
        return result.dump();
    }
    return std::nullopt;
}

std::string ZMQClient::await_results() {
    std::optional<std::string> result;
    while (!result.has_value()) {
        result = check_received();
    }
    logger.info("Results received");
    return result.value();
}