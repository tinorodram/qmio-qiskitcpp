#include "utils/logger.hpp"
#include <algorithm>
#include <cctype>

namespace Qmio {

// ── Static members ────────────────────────────────────────────────────────────
LogLevel Logger::global_level_ = []() {
    LogLevel default_level = LogLevel::INFO;
    // 2. Intentamos leer el entorno directamente aquí
    const char* env = std::getenv("LOG_LEVEL");
    if (env) {
        std::string val(env);
        std::transform(val.begin(), val.end(), val.begin(), ::toupper);
        
        if (val == "DEBUG")   return LogLevel::DEBUG;
        if (val == "INFO")    return LogLevel::INFO;
        if (val == "WARNING") return LogLevel::WARNING;
        if (val == "ERROR")   return LogLevel::ERROR;
    }
    return default_level;
}(); //

// ── Constructor ───────────────────────────────────────────────────────────────
Logger::Logger(const std::string& name)
    : name_(name)
    , level_(LogLevel::NONE)   // NONE means use global level
{}

// ── get ───────────────────────────────────────────────────────────────────────
Logger& Logger::get(const std::string& name) {
    static std::unordered_map<std::string, Logger> registry;
    static std::mutex                              mutex;

    std::lock_guard<std::mutex> lock(mutex);
    auto it = registry.find(name);
    if (it == registry.end()) {
        registry.emplace(name, Logger(name));
    }
    return registry.at(name);
}

// ── setup_from_env — mirrors Python's _setup_logging() ───────────────────────
void Logger::setup_from_env() {
    const char* env = std::getenv("LOG_LEVEL");
    if (env) {
        std::string val(env);
        std::transform(val.begin(), val.end(), val.begin(), ::toupper);
        global_level_ = parse_level(val);
    }
}

// ── set_global_level ──────────────────────────────────────────────────────────
void Logger::set_global_level(LogLevel level) {
    global_level_ = level;
}

void Logger::set_level(LogLevel level) {
    level_ = level;
}

// ── Log methods ───────────────────────────────────────────────────────────────
void Logger::debug  (const std::string& msg) const { if (should_log(LogLevel::DEBUG))   log("DEBUG",   msg); }
void Logger::info   (const std::string& msg) const { if (should_log(LogLevel::INFO))    log("INFO",    msg); }
void Logger::warning(const std::string& msg) const { if (should_log(LogLevel::WARNING)) log("WARNING", msg); }
void Logger::error  (const std::string& msg) const { if (should_log(LogLevel::ERROR))   log("ERROR",   msg); }

// ── Internal helpers ──────────────────────────────────────────────────────────
bool Logger::should_log(LogLevel level) const {
    LogLevel effective = (level_ == LogLevel::NONE) ? global_level_ : level_;
    return level >= effective;
}

void Logger::log(const std::string& level_str, const std::string& msg) const {
    std::cout << "[" << level_str << "] "
              << "[" << name_     << "] "
              << msg              << "\n";
}

LogLevel Logger::parse_level(const std::string& str) {
    if (str == "DEBUG")   return LogLevel::DEBUG;
    if (str == "INFO")    return LogLevel::INFO;
    if (str == "WARNING") return LogLevel::WARNING;
    if (str == "ERROR")   return LogLevel::ERROR;
    return LogLevel::WARNING;  // default
}

} // namespace Qmio