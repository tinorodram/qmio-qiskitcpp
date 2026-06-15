#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace pickle {
    // Serializes a (str, str) tuple using Python pickle protocol 4
    std::vector<uint8_t> dumps_tuple2(const std::string& a, const std::string& b);

}