#include "stdafx.h"
#include "physical_targets.h"

#include <set>
#include <string>

namespace metadata_isolation {
namespace {

// Both operands have already passed g_get_canonical_path() and are local
// physical sources. Preserve the existing filesystem-aware path comparison
// (with the same metadb fallback) while avoiding quadratic scans of a large
// selection of *distinct* files.
struct physical_source_less {
    bool operator()(const std::string& left, const std::string& right) const {
        int comparison = 0;
        if (filesystem::g_compare_paths(left.c_str(), right.c_str(), comparison)) {
            return comparison < 0;
        }
        return metadb::path_compare(left.c_str(), right.c_str()) < 0;
    }
};

void reject_target(
    physical_target_map& result,
    const char* path,
    const char* reason) {
    ++result.rejected_count;
    if (result.first_rejected_path.is_empty()) {
        result.first_rejected_path = path;
        result.first_rejected_reason = reason;
    }
}

} // namespace

physical_target_map map_to_physical_targets(metadb_handle_list_cref selected) {
    physical_target_map result;
    result.input_count = selected.get_count();

    // O(N log N) distinct-source deduplication, rather than N linear scans
    // through previously created foobar handles.
    std::set<std::string, physical_source_less> unique_physical_sources;
    for (const auto& item : selected) {
        const char* source_path = item->get_path();
        try {
            pfc::string8 canonical_path;
            filesystem::g_get_canonical_path(source_path, canonical_path);

            filesystem::ptr source_filesystem = filesystem::tryGet(canonical_path);
            if (source_filesystem.is_empty()) {
                reject_target(result, source_path, "No filesystem service recognizes this source.");
                continue;
            }
            if (source_filesystem->is_remote(canonical_path)) {
                reject_target(result, source_path, "Remote sources are not physical file targets.");
                continue;
            }

            pfc::string8 native_path;
            if (!filesystem::g_get_native_path(canonical_path, native_path)) {
                reject_target(result, source_path, "The source has no local physical file path.");
                continue;
            }

            const std::string key(canonical_path.c_str());
            if (unique_physical_sources.find(key) == unique_physical_sources.end()) {
                // Create and record a real physical handle before marking its
                // canonical source as seen. A failed handle_create must not
                // silently suppress a later valid selected track.
                result.handles.add_item(metadb::get()->handle_create(canonical_path, 0));
                unique_physical_sources.insert(key);
            }
        } catch (const std::exception& error) {
            reject_target(result, source_path, error.what());
        }
    }

    return result;
}

void format_physical_mapping_error(
    const physical_target_map& mapping,
    pfc::string_base& output) {
    pfc::string_formatter message;
    message << "Metadata Isolation could not map the complete selection to local physical files.\n\n"
            << "Selected items: " << mapping.input_count << "\n"
            << "Rejected items: " << mapping.rejected_count;
    if (!mapping.first_rejected_path.is_empty()) {
        message << "\n\nFirst rejected source:\n" << mapping.first_rejected_path;
    }
    if (!mapping.first_rejected_reason.is_empty()) {
        message << "\n\nReason:\n" << mapping.first_rejected_reason;
    }
    message << "\n\nNo operation was started.";
    output = message;
}

} // namespace metadata_isolation
