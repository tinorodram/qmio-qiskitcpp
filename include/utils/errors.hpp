#pragma once
#include <stdexcept>
#include <string>

// Raised when a shell command returns non-zero exit status
class RunCommandError : public std::runtime_error {
public:
    explicit RunCommandError(const std::string& msg)
        : std::runtime_error(msg) {}
};

// Raised when backend node IP cannot be resolved
class BackendError : public std::runtime_error {
public:
    explicit BackendError(const std::string& msg)
        : std::runtime_error(msg) {}
};

// Raised when output from a command cannot be parsed
class OutputParsingError : public std::runtime_error {
public:
    explicit OutputParsingError(const std::string& msg)
        : std::runtime_error(msg) {}
};

// Execute a shell command — returns {stdout, stderr}
// Throws RunCommandError if exit status is non-zero
std::pair<std::string, std::string> run(const std::string& cmd);