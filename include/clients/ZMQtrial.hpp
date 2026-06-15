#ifndef ZMQTRIAL_HPP
#define ZMQTRIAL_HPP

#include <string>
#include <zmqpp/zmqpp.hpp>

class ZMQtrial {
public:
    ZMQtrial();

    void send_message(const std::string& text);

private:
    zmqpp::context context;
    zmqpp::socket socket;
};

#endif