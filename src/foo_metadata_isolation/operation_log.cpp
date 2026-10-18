#include "stdafx.h"
#include "operation_log.h"
#include "preferences.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <vector>

namespace metadata_isolation {
namespace {

using log_clock = std::chrono::steady_clock;

std::atomic<uint64_t> g_session_sequence{0};
std::mutex g_console_mutex;
#if defined(FB2K_ENABLE_TEST_PROBES)
std::vector<std::string> g_test_capture;
#endif

std::string ascii_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool contains_sensitive_marker(const std::string& text) {
    const std::string lowered = ascii_lower(text);
    static const char* markers[] = {
        "access_token", "refresh_token", "authorization", "password", "passwd",
        "secret", "cookie", "credential", "oauth", "auth_token",
        "token", "auth",
    };
    for (const char* marker : markers) {
        if (lowered.find(marker) != std::string::npos) return true;
    }
    return false;
}

std::string safe_identifier(const char* input) {
    std::string output;
    if (input == nullptr) return "unknown";
    for (const unsigned char character : std::string(input)) {
        output.push_back(std::isalnum(character) || character == '_' || character == '-' ||
                character == '.' ? static_cast<char>(character) : '_');
    }
    return output.empty() ? "unknown" : output;
}

std::string safe_value(const operation_log_field& field) {
    if (contains_sensitive_marker(field.key) || contains_sensitive_marker(field.value)) {
        return "<redacted>";
    }
    std::string output;
    output.reserve(field.value.size() + 2);
    output.push_back('"');
    for (unsigned char character : field.value) {
        switch (character) {
        case '\\': output += "\\\\"; break;
        case '"': output += "\\\""; break;
        case '\r': output += "\\r"; break;
        case '\n': output += "\\n"; break;
        case '\t': output += "\\t"; break;
        default:
            if (character < 0x20) output.push_back('?');
            else output.push_back(static_cast<char>(character));
            break;
        }
    }
    output.push_back('"');
    return output;
}

const char* level_name(operation_log_level level) {
    switch (level) {
    case operation_log_level::standard: return "STANDARD";
    case operation_log_level::verbose: return "VERBOSE";
    case operation_log_level::critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

std::string format_line(
    uint64_t session,
    operation_log_level level,
    const std::string& operation,
    const std::string& layer,
    const char* event,
    std::initializer_list<operation_log_field> fields) {
    std::ostringstream output;
    output << "[METAISO S" << std::setw(6) << std::setfill('0') << session << "]"
           << " level=" << level_name(level)
           << " event=" << safe_identifier(event)
           << " layer=" << safe_identifier(layer.c_str())
           << " operation=" << safe_identifier(operation.c_str());
    for (const auto& field : fields) {
        output << " " << safe_identifier(field.key.c_str()) << "=" << safe_value(field);
    }
    return output.str();
}

void print_line(const std::string& line) {
    const std::lock_guard<std::mutex> lock(g_console_mutex);
    FB2K_console_formatter() << line.c_str();
#if defined(FB2K_ENABLE_TEST_PROBES)
    g_test_capture.push_back(line);
#endif
}

} // namespace

struct operation_logger::state {
    uint64_t session = ++g_session_sequence;
    std::string operation;
    std::string layer;
    log_clock::time_point started = log_clock::now();
    bool verbose = cfg_verbose_logging;
    std::atomic<bool> finished{false};
};

operation_log_field log_count(const char* key, size_t value) noexcept {
    try {
        return {key == nullptr ? "unknown" : key, std::to_string(value)};
    } catch (...) {
        return {};
    }
}

operation_log_field log_number(const char* key, uint64_t value) noexcept {
    try {
        return {key == nullptr ? "unknown" : key, std::to_string(value)};
    } catch (...) {
        return {};
    }
}

operation_log_field log_duration(const char* key, uint64_t microseconds) noexcept {
    try {
        return {key == nullptr ? "unknown" : key, std::to_string(microseconds)};
    } catch (...) {
        return {};
    }
}

operation_log_field log_flag(const char* key, bool value) noexcept {
    try {
        return {key == nullptr ? "unknown" : key, value ? "true" : "false"};
    } catch (...) {
        return {};
    }
}

operation_log_field log_token(const char* key, const char* value) noexcept {
    try {
        return {key == nullptr ? "unknown" : key, value == nullptr ? "unknown" : value};
    } catch (...) {
        return {};
    }
}

operation_logger::operation_logger(const char* operation, const char* layer) noexcept {
    try {
        m_state = std::make_shared<state>();
        m_state->operation = operation == nullptr ? "unknown" : operation;
        m_state->layer = layer == nullptr ? "unknown" : layer;
    } catch (...) {
    }
}

bool operation_logger::verbose_enabled() const noexcept {
    return m_state != nullptr && m_state->verbose;
}

uint64_t operation_logger::elapsed_microseconds() const noexcept {
    if (m_state == nullptr) return 0;
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        log_clock::now() - m_state->started).count());
}

void operation_logger::standard(
    const char* event,
    std::initializer_list<operation_log_field> fields) const noexcept {
    if (m_state == nullptr) return;
    try {
        print_line(format_line(
            m_state->session, operation_log_level::standard, m_state->operation,
            m_state->layer, event, fields));
    } catch (...) {
    }
}

void operation_logger::verbose(
    const char* event,
    std::initializer_list<operation_log_field> fields) const noexcept {
    if (m_state == nullptr || !m_state->verbose) return;
    try {
        print_line(format_line(
            m_state->session, operation_log_level::verbose, m_state->operation,
            m_state->layer, event, fields));
    } catch (...) {
    }
}

void operation_logger::critical(
    const char* event,
    std::initializer_list<operation_log_field> fields) const noexcept {
    if (m_state == nullptr) return;
    try {
        print_line(format_line(
            m_state->session, operation_log_level::critical, m_state->operation,
            m_state->layer, event, fields));
    } catch (...) {
    }
}

void operation_logger::finish(
    const char* outcome,
    const char* reason,
    std::initializer_list<operation_log_field> fields) const noexcept {
    bool expected = false;
    if (m_state == nullptr) return;
    if (!m_state->finished.compare_exchange_strong(expected, true)) return;
    try {
        std::vector<operation_log_field> combined(fields);
        combined.push_back(log_token("outcome", outcome));
        if (reason != nullptr && *reason != 0) combined.push_back(log_token("reason", reason));
        combined.push_back(log_duration("total_us", elapsed_microseconds()));

        std::ostringstream output;
        output << "[METAISO S" << std::setw(6) << std::setfill('0') << m_state->session << "]"
               << " level=STANDARD event=session.complete"
               << " layer=" << safe_identifier(m_state->layer.c_str())
               << " operation=" << safe_identifier(m_state->operation.c_str());
        for (const auto& field : combined) {
            output << " " << safe_identifier(field.key.c_str()) << "=" << safe_value(field);
        }
        print_line(output.str());
    } catch (...) {
    }
}

#if defined(FB2K_ENABLE_TEST_PROBES)
std::string format_operation_log_line_for_test(
    const char* event,
    std::initializer_list<operation_log_field> fields) {
    return format_line(
        42, operation_log_level::verbose, "diagnostic", "test", event, fields);
}

void clear_operation_log_capture_for_test() noexcept {
    try {
        const std::lock_guard<std::mutex> lock(g_console_mutex);
        g_test_capture.clear();
    } catch (...) {
    }
}

std::vector<std::string> snapshot_operation_log_capture_for_test() {
    const std::lock_guard<std::mutex> lock(g_console_mutex);
    return g_test_capture;
}
#endif

} // namespace metadata_isolation
