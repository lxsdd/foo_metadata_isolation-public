#pragma once

#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace metadata_isolation {

enum class operation_log_level {
    standard,
    verbose,
    critical,
};

struct operation_log_field {
    std::string key;
    std::string value;
};

operation_log_field log_count(const char* key, size_t value) noexcept;
operation_log_field log_number(const char* key, uint64_t value) noexcept;
operation_log_field log_duration(const char* key, uint64_t microseconds) noexcept;
operation_log_field log_flag(const char* key, bool value) noexcept;
operation_log_field log_token(const char* key, const char* value) noexcept;

class operation_logger {
public:
    operation_logger(const char* operation, const char* layer) noexcept;

    bool verbose_enabled() const noexcept;
    uint64_t elapsed_microseconds() const noexcept;

    void standard(
        const char* event,
        std::initializer_list<operation_log_field> fields = {}) const noexcept;
    void verbose(
        const char* event,
        std::initializer_list<operation_log_field> fields = {}) const noexcept;
    void critical(
        const char* event,
        std::initializer_list<operation_log_field> fields = {}) const noexcept;
    void finish(
        const char* outcome,
        const char* reason = nullptr,
        std::initializer_list<operation_log_field> fields = {}) const noexcept;

private:
    struct state;
    std::shared_ptr<state> m_state;
};

#if defined(FB2K_ENABLE_TEST_PROBES)
std::string format_operation_log_line_for_test(
    const char* event,
    std::initializer_list<operation_log_field> fields);
void clear_operation_log_capture_for_test() noexcept;
std::vector<std::string> snapshot_operation_log_capture_for_test();
#endif

} // namespace metadata_isolation
