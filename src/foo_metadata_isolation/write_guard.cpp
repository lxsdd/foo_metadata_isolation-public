#include "stdafx.h"
#include "guid.h"
#include "preferences.h"
#include "virtual_edit_session.h"
#include "write_guard.h"

#include <atomic>

namespace metadata_isolation {
namespace {

std::atomic_bool g_filter_callback_observed = false;

bool is_unsafe_embedded_cue_subsong(const playable_location& location) {
    if (location.get_subsong() == 0) return false;
    const char* extension = pfc::string_extension(location.get_path());
    return pfc::stricmp_ascii(extension, "mp3") == 0 ||
        pfc::stricmp_ascii(extension, "flac") == 0 ||
        pfc::stricmp_ascii(extension, "wv") == 0;
}

class unsafe_subsong_write_guard : public input_info_filter {
public:
    void filter_info_read(
        const playable_location&,
        file_info&,
        abort_callback&) override {
        g_filter_callback_observed.store(true, std::memory_order_relaxed);
    }

    bool filter_info_write(
        const playable_location& location,
        file_info& info,
        abort_callback&) override {
        g_filter_callback_observed.store(true, std::memory_order_relaxed);
        if (!cfg_enabled || !is_unsafe_embedded_cue_subsong(location)) return true;
        // A registered session shadow is already isolated from the original.
        // Capture the writer's exact semantic postimage, but suppress its
        // destructive Cue serialization so all projected subsongs continue to
        // observe the same immutable backing document during the command.
        if (capture_virtual_shadow_write(location, info)) return false;

        fb2k::inMainThread([] {
            popup_message::g_show(
                "Direct virtual metadata writing was blocked because it can rewrite the embedded "
                "Cue Sheet and remove physical metadata.\n\n"
                "Use Metadata Isolation > Virtual metadata: Run context command safely... for "
                "virtual tracks, or choose a Physical file metadata command for physical tags.\n\n"
                "No change was written.",
                "Metadata Isolation: write blocked");
        });
        return false;
    }

    void on_info_remove(const char*, abort_callback&) override {}
    GUID get_guid() override { return guids::write_guard_filter; }
    GUID get_preferences_guid() override { return guids::preferences_page; }
    const char* get_name() override { return "Block unsafe virtual MP3/FLAC/WavPack subsong writes"; }
    bool supports_fallback() override { return false; }
    bool write_fallback(
        const playable_location&,
        const file_info&,
        abort_callback&) override { return false; }
    void remove_tags_fallback(const char*, abort_callback&) override {}
};

service_factory_single_t<unsafe_subsong_write_guard> g_write_guard_factory;

} // namespace

protection_status get_protection_status() {
    if (!cfg_enabled) return protection_status::inactive;
    if (g_filter_callback_observed.load(std::memory_order_relaxed)) {
        return protection_status::active;
    }
    return protection_status::unknown;
}

const char* protection_status_name(protection_status status) {
    switch (status) {
    case protection_status::active: return "ACTIVE";
    case protection_status::inactive: return "INACTIVE";
    case protection_status::unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

} // namespace metadata_isolation
