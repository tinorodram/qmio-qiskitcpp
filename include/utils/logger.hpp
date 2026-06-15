#ifndef __qmio_logger_hpp__
#define __qmio_logger_hpp__

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <iostream>
#include <cstdlib>

namespace Qmio {

enum class LogLevel {
    DEBUG   = 0,
    INFO    = 1,
    WARNING = 2,
    ERROR   = 3,
    NONE    = 4    // silent
};

class Logger {
public:
    // Get or create a named logger — mirrors Python's logging.getLogger(name)
    static Logger& get(const std::string& name);

    // Set global log level — applies to all loggers
    static void set_global_level(LogLevel level);

    // Read level from LOG_LEVEL env var — mirrors Python's _setup_logging()
    static void setup_from_env();

    void debug  (const std::string& msg) const;
    void info   (const std::string& msg) const;
    void warning(const std::string& msg) const;
    void error  (const std::string& msg) const;

    void set_level(LogLevel level);

private:
    explicit Logger(const std::string& name);

    std::string name_;
    LogLevel    level_;

    static LogLevel                                        global_level_;

    void log(const std::string& level_str, const std::string& msg) const;
    bool should_log(LogLevel level) const;

    static LogLevel parse_level(const std::string& str);
    static std::string level_to_str(LogLevel level);
};

} // namespace Qmio
#endif // __qmio_logger_hpp__