#include "stdafx.h"
#include "physical_targets.h"

namespace metadata_isolation {
namespace {

bool same_source_path(const char* left, const char* right) {
    int comparison = 0;
    if (filesystem::g_compare_paths(left, right, comparison)) return comparison == 0;
    return metadb::path_compare(left, right) == 0;
}

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

            bool duplicate = false;
            for (const auto& existing : result.handles) {
                if (same_source_path(existing->get_path(), canonical_path)) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                result.handles.add_item(metadb::get()->handle_create(canonical_path, 0));
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
