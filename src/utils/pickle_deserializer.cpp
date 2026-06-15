#include "utils/pickle_deserializer.hpp"
#include <stack>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cstring>
#include <cstdio>

using json = nlohmann::json;

namespace pickle {
namespace {

class Deserializer {
public:
    json loads(const uint8_t* data, size_t len) {
        data_ = data; len_ = len; pos_ = 0;
        memo_.clear(); stack_.clear(); marks_.clear(); memo_idx_ = 0;

        // PROTO
        if (pos_ >= len_ || data_[pos_] != 0x80)
            throw std::runtime_error("Expected PROTO opcode");
        pos_++;
        proto_ = read_u8();

        // FRAME (protocol 4+)
        if (proto_ >= 4 && pos_ < len_ && data_[pos_] == 0x95) {
            pos_++;
            read_u64_le(); // skip frame length
        }

        while (pos_ < len_) {
            uint8_t op = read_u8();
            if (!dispatch(op)) break;
        }

        if (stack_.empty())
            throw std::runtime_error("Empty stack at end of pickle");
        return stack_.back();
    }

private:
    const uint8_t*  data_     = nullptr;
    size_t          len_      = 0;
    size_t          pos_      = 0;
    uint8_t         proto_    = 0;
    size_t          memo_idx_ = 0;

    std::vector<json>                    stack_;
    std::vector<size_t>                  marks_;
    std::unordered_map<size_t, json>     memo_;

    // ── Readers ──────────────────────────────────────────────────────────────
    uint8_t read_u8() {
        if (pos_ >= len_) throw std::runtime_error("Unexpected end of data");
        return data_[pos_++];
    }
    uint16_t read_u16_le() {
        if (pos_ + 2 > len_) throw std::runtime_error("Unexpected end");
        uint16_t v = data_[pos_] | (uint16_t(data_[pos_+1]) << 8);
        pos_ += 2; return v;
    }
    uint32_t read_u32_le() {
        if (pos_ + 4 > len_) throw std::runtime_error("Unexpected end");
        uint32_t v = data_[pos_] | (uint32_t(data_[pos_+1])<<8) |
                     (uint32_t(data_[pos_+2])<<16) | (uint32_t(data_[pos_+3])<<24);
        pos_ += 4; return v;
    }
    int32_t read_i32_le() { return static_cast<int32_t>(read_u32_le()); }
    uint64_t read_u64_le() {
        if (pos_ + 8 > len_) throw std::runtime_error("Unexpected end");
        uint64_t v = 0;
        for (int i = 0; i < 8; i++) v |= uint64_t(data_[pos_+i]) << (8*i);
        pos_ += 8; return v;
    }
    double read_f64_be() {
        if (pos_ + 8 > len_) throw std::runtime_error("Unexpected end");
        uint8_t buf[8];
        for (int i = 0; i < 8; i++) buf[i] = data_[pos_ + 7 - i];
        double v; memcpy(&v, buf, 8);
        pos_ += 8; return v;
    }
    std::string read_str(size_t n) {
        if (pos_ + n > len_) throw std::runtime_error("Unexpected end");
        std::string s(reinterpret_cast<const char*>(data_ + pos_), n);
        pos_ += n; return s;
    }
    std::string read_line() {
        std::string s;
        while (pos_ < len_ && data_[pos_] != '\n') s += data_[pos_++];
        if (pos_ < len_) pos_++; // skip '\n'
        return s;
    }

    void memoize_top() {
        if (stack_.empty()) throw std::runtime_error("Empty stack on MEMOIZE");
        memo_[memo_idx_++] = stack_.back();
    }

    json get_memo(size_t idx) {
        auto it = memo_.find(idx);
        if (it == memo_.end()) throw std::runtime_error("Memo miss: " + std::to_string(idx));
        return it->second;
    }

    // ── Dispatch ─────────────────────────────────────────────────────────────
    bool dispatch(uint8_t op) {
        switch (op) {
        case 0x2e: return false;                    // STOP

        case 0x4e: stack_.push_back(nullptr); break; // NONE
        case 0x88: stack_.push_back(true);    break; // NEWTRUE
        case 0x89: stack_.push_back(false);   break; // NEWFALSE

        case 0x4b: stack_.push_back(read_u8());     break; // BININT1
        case 0x4d: stack_.push_back(read_u16_le()); break; // BININT2
        case 0x4a: stack_.push_back(read_i32_le()); break; // BININT
        case 0x47: stack_.push_back(read_f64_be()); break; // BINFLOAT

        case 0x8c: { size_t n=read_u8();       stack_.push_back(read_str(n)); break; } // SHORT_BINUNICODE
        case 0x58: { size_t n=read_u32_le();   stack_.push_back(read_str(n)); break; } // BINUNICODE
        case 0x8d: { size_t n=read_u64_le();   stack_.push_back(read_str(n)); break; } // BINUNICODE8
        case 0x55: { size_t n=read_u8();       stack_.push_back(read_str(n)); break; } // SHORT_BINSTRING
        case 0x54: { size_t n=read_u32_le();   stack_.push_back(read_str(n)); break; } // BINSTRING

        case 0x7d: stack_.push_back(json::object()); break; // EMPTY_DICT
        case 0x5d: stack_.push_back(json::array());  break; // EMPTY_LIST
        case 0x29: stack_.push_back(json::array());  break; // EMPTY_TUPLE

        case 0x28: marks_.push_back(stack_.size()); break;  // MARK

        case 0x94: memoize_top(); break; // MEMOIZE

        case 0x71: { size_t idx=read_u8();       if(!stack_.empty()) memo_[idx]=stack_.back(); break; } // BINPUT
        case 0x72: { size_t idx=read_u32_le();   if(!stack_.empty()) memo_[idx]=stack_.back(); break; } // LONG_BINPUT

        case 0x68: stack_.push_back(get_memo(read_u8()));     break; // BINGET
        case 0x6a: stack_.push_back(get_memo(read_u32_le())); break; // LONG_BINGET

        case 0x73: { // SETITEM: dict[key] = val
            json val = stack_.back(); stack_.pop_back();
            json key = stack_.back(); stack_.pop_back();
            if (!stack_.back().is_object()) throw std::runtime_error("SETITEM on non-dict");
            stack_.back()[key.get<std::string>()] = val;
            break;
        }
        case 0x75: { // SETITEMS: dict.update from mark
            if (marks_.empty()) throw std::runtime_error("No mark for SETITEMS");
            size_t mark = marks_.back(); marks_.pop_back();
            json& dict = stack_[mark - 1];
            if (!dict.is_object()) throw std::runtime_error("SETITEMS on non-dict");
            for (size_t i = mark; i + 1 < stack_.size(); i += 2)
                dict[stack_[i].get<std::string>()] = stack_[i+1];
            stack_.resize(mark);
            break;
        }
        case 0x61: { // APPEND
            json item = stack_.back(); stack_.pop_back();
            if (!stack_.back().is_array()) throw std::runtime_error("APPEND on non-list");
            stack_.back().push_back(item);
            break;
        }
        case 0x65: { // APPENDS: list.extend from mark
            if (marks_.empty()) throw std::runtime_error("No mark for APPENDS");
            size_t mark = marks_.back(); marks_.pop_back();
            json& list = stack_[mark - 1];
            if (!list.is_array()) throw std::runtime_error("APPENDS on non-list");
            for (size_t i = mark; i < stack_.size(); i++) list.push_back(stack_[i]);
            stack_.resize(mark);
            break;
        }
        case 0x74: { // TUPLE from mark
            if (marks_.empty()) throw std::runtime_error("No mark for TUPLE");
            size_t mark = marks_.back(); marks_.pop_back();
            json arr = json::array();
            for (size_t i = mark; i < stack_.size(); i++) arr.push_back(stack_[i]);
            stack_.resize(mark);
            stack_.push_back(arr);
            break;
        }
        case 0x85: { json a=stack_.back(); stack_.pop_back(); stack_.push_back(json::array({a})); break; }           // TUPLE1
        case 0x86: { json b=stack_.back(); stack_.pop_back(); json a=stack_.back(); stack_.pop_back(); stack_.push_back(json::array({a,b})); break; } // TUPLE2
        case 0x87: { json c=stack_.back(); stack_.pop_back(); json b=stack_.back(); stack_.pop_back(); json a=stack_.back(); stack_.pop_back(); stack_.push_back(json::array({a,b,c})); break; } // TUPLE3

        case 0x95: read_u64_le(); break; // FRAME — skip length
        case 0x80: proto_ = read_u8(); break; // PROTO again

        case 0x8a: { // LONG1: variable length int
            size_t n = read_u8();
            int64_t v = 0;
            for (size_t i = 0; i < n && i < 8; i++) v |= int64_t(read_u8()) << (8*i);
            stack_.push_back(v);
            break;
        }

        case 0x63: { // GLOBAL: module\nname\n
            std::string mod  = read_line();
            std::string name = read_line();
            stack_.push_back(json::object({{"__global__", mod+"."+name}}));
            break;
        }
        case 0x93: { // STACK_GLOBAL
            json name = stack_.back(); stack_.pop_back();
            json mod  = stack_.back(); stack_.pop_back();
            stack_.push_back(json::object({{"__global__", mod.get<std::string>()+"."+name.get<std::string>()}}));
            break;
        }
        case 0x52: { // REDUCE
            json args = stack_.back(); stack_.pop_back();
            json func = stack_.back(); stack_.pop_back();
            stack_.push_back(json::object({{"__reduce__", func},{"args",args}}));
            break;
        }
        case 0x81: { // NEWOBJ
            json args = stack_.back(); stack_.pop_back();
            json cls  = stack_.back(); stack_.pop_back();
            stack_.push_back(json::object({{"__newobj__", cls},{"args",args}}));
            break;
        }
        case 0x62: { // BUILD: apply state
            json state = stack_.back(); stack_.pop_back();
            if (stack_.back().is_object() && state.is_object())
                for (auto& [k,v] : state.items()) stack_.back()[k] = v;
            break;
        }

        default: {
            char buf[64];
            snprintf(buf, sizeof(buf), "Unknown pickle opcode: 0x%02x at pos %zu", op, pos_-1);
            throw std::runtime_error(buf);
        }
        }
        return true;
    }
};

} // anonymous namespace

json loads(const uint8_t* data, size_t len) {
    Deserializer d;
    return d.loads(data, len);
}

} // namespace pickle