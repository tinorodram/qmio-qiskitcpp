#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>

namespace pickle {
    // Deserialize a pickled Python object directly to nlohmann::json
    nlohmann::json loads(const uint8_t* data, size_t len);
}