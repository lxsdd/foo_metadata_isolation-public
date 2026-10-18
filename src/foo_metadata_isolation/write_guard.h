#pragma once

namespace metadata_isolation {

enum class protection_status {
    active,
    inactive,
    unknown,
};

protection_status get_protection_status();
const char* protection_status_name(protection_status status);

} // namespace metadata_isolation
