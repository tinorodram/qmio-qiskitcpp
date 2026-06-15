#include "utils/pickle_serializer.hpp"
#include <cstring>

namespace pickle {

namespace {

// Append raw bytes to buffer
void append_bytes(std::vector<uint8_t>& buf, const void* data, size_t n) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
    buf.insert(buf.end(), p, p + n);
}

// Append a single byte
void append_byte(std::vector<uint8_t>& buf, uint8_t b) {
    buf.push_back(b);
}

// Append little-endian uint64
void append_le64(std::vector<uint8_t>& buf, uint64_t v) {
    for (int i = 0; i < 8; i++) {
        buf.push_back(v & 0xFF);
        v >>= 8;
    }
}

// Serialize a single Python unicode string (str)
void serialize_str(std::vector<uint8_t>& buf, const std::string& s) {
    if (s.size() < 256) {
        // SHORT_BINUNICODE: 0x8c + 1-byte length
        append_byte(buf, 0x8c);
        append_byte(buf, static_cast<uint8_t>(s.size()));
    } else {
        // BINUNICODE8: 0x8d + 8-byte LE length
        append_byte(buf, 0x8d);
        append_le64(buf, static_cast<uint64_t>(s.size()));
    }
    append_bytes(buf, s.data(), s.size());
    append_byte(buf, 0x94);  // MEMOIZE
}

// Compute the body length (everything after the FRAME header)
size_t compute_body_length(const std::string& a, const std::string& b) {
    size_t len = 0;
    // string a
    len += (a.size() < 256) ? 2 : 9;   // opcode + length bytes
    len += a.size();
    len += 1;                            // MEMOIZE
    // string b
    len += (b.size() < 256) ? 2 : 9;
    len += b.size();
    len += 1;                            // MEMOIZE
    // TUPLE2 + MEMOIZE + STOP
    len += 3;
    return len;
}

} // anonymous namespace

std::vector<uint8_t> dumps_tuple2(const std::string& a, const std::string& b) {
    std::vector<uint8_t> buf;
    buf.reserve(32 + a.size() + b.size());

    // Header
    append_byte(buf, 0x80);  // PROTO
    append_byte(buf, 0x04);  // protocol 4

    // FRAME opcode + 8-byte LE body length
    append_byte(buf, 0x95);
    append_le64(buf, static_cast<uint64_t>(compute_body_length(a, b)));

    // Serialize both strings
    serialize_str(buf, a);
    serialize_str(buf, b);

    // Build the tuple and stop
    append_byte(buf, 0x86);  // TUPLE2
    append_byte(buf, 0x94);  // MEMOIZE
    append_byte(buf, 0x2e);  // STOP

    return buf;
}


std::string loads_str(const uint8_t* data, size_t len) {
    size_t pos = 0;

    // Expect PROTO opcode 0x80
    if (pos + 2 > len || data[pos] != 0x80) {
        throw std::runtime_error("Not a valid pickle: missing PROTO opcode");
    }
    uint8_t proto = data[pos + 1];
    pos += 2;

    // Skip FRAME opcode 0x95 + 8-byte length (protocol 4+)
    if (proto >= 4 && pos < len && data[pos] == 0x95) {
        pos += 9;  // 1 opcode + 8 bytes length
    }

    if (pos >= len) {
        throw std::runtime_error("Unexpected end of pickle data");
    }

    // Read string opcode
    uint8_t opcode = data[pos++];

    if (opcode == 0x8c) {
        // SHORT_BINUNICODE: 1-byte length (< 256 chars)
        if (pos >= len) throw std::runtime_error("Unexpected end after SHORT_BINUNICODE");
        size_t str_len = data[pos++];
        if (pos + str_len > len) throw std::runtime_error("String extends beyond data");
        return std::string(reinterpret_cast<const char*>(data + pos), str_len);

    } else if (opcode == 0x8d) {
        // BINUNICODE8: 8-byte LE length
        if (pos + 8 > len) throw std::runtime_error("Unexpected end after BINUNICODE8");
        uint64_t str_len = 0;
        for (int i = 0; i < 8; i++) {
            str_len |= static_cast<uint64_t>(data[pos + i]) << (8 * i);
        }
        pos += 8;
        if (pos + str_len > len) throw std::runtime_error("String extends beyond data");
        return std::string(reinterpret_cast<const char*>(data + pos), str_len);

    } else if (opcode == 0x58) {
        // BINUNICODE: 4-byte LE length (protocol 3 fallback)
        if (pos + 4 > len) throw std::runtime_error("Unexpected end after BINUNICODE");
        uint32_t str_len = 0;
        for (int i = 0; i < 4; i++) {
            str_len |= static_cast<uint32_t>(data[pos + i]) << (8 * i);
        }
        pos += 4;
        if (pos + str_len > len) throw std::runtime_error("String extends beyond data");
        return std::string(reinterpret_cast<const char*>(data + pos), str_len);

    } else {
        // Print raw bytes for debugging if opcode is unexpected
        std::string hex;
        for (size_t i = 0; i < std::min(len, size_t(32)); i++) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02x ", data[i]);
            hex += buf;
        }
        throw std::runtime_error(
            "Unexpected pickle opcode: 0x" + std::to_string(opcode) +
            " — first bytes: " + hex
        );
    }
}

}