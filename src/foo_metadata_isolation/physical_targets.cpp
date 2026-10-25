#include "stdafx.h"
#include "physical_targets.h"

#include <climits>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <tuple>

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

// Windows path case equivalence is Unicode-aware, not ASCII byte folding.
// Convert only valid foobar UTF-8 paths to Windows UTF-16. An invalid path
// fails closed before any physical-writing target is constructed.
std::wstring unicode_path_from_utf8(const std::string& utf8) {
    if (utf8.empty() || utf8.size() > INT_MAX) {
        throw pfc::exception("Physical source has an invalid UTF-8 path length.");
    }
    const int length = static_cast<int>(utf8.size());
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          utf8.data(), length, nullptr, 0);
    if (needed == 0) {
        throw pfc::exception("Physical source has an invalid UTF-8 path.");
    }
    std::wstring wide(static_cast<size_t>(needed), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            utf8.data(), length, wide.data(), needed) != needed) {
        throw pfc::exception("Physical source UTF-8 conversion failed.");
    }
    return wide;
}

struct windows_path_case_less {
    bool operator()(const std::wstring& left, const std::wstring& right) const {
        const int comparison = CompareStringOrdinal(
            left.c_str(), static_cast<int>(left.size()),
            right.c_str(), static_cast<int>(right.size()), TRUE);
        // Both values passed strict UTF-8 decoding; a failed comparison must
        // never misrepresent distinct paths as equivalent.
        if (comparison == 0) throw pfc::exception("Windows path comparison failed.");
        return comparison == CSTR_LESS_THAN;
    }
};

bool same_physical_file(const std::string& native, const std::string& previous) {
    const std::u8string current_utf8(native.begin(), native.end());
    const std::u8string previous_utf8(previous.begin(), previous.end());
    std::error_code error;
    const bool equal = std::filesystem::equivalent(
        std::filesystem::path(current_utf8),
        std::filesystem::path(previous_utf8),
        error);
    return !error && equal;
}

// A different path (for example a Windows hardlink or a junction) can still
// resolve to the *same* physical file. Query the actual local Windows file
// identity using a read-only attribute handle; no tag or data is read/written.
// A failed identity query is not permission to schedule a physical write.
struct physical_file_identity {
    DWORD volume_serial = 0;
    DWORD file_index_high = 0;
    DWORD file_index_low = 0;
    bool operator==(const physical_file_identity& other) const noexcept {
        return std::tie(volume_serial, file_index_high, file_index_low) ==
            std::tie(other.volume_serial, other.file_index_high, other.file_index_low);
    }
    bool operator<(const physical_file_identity& other) const noexcept {
        return std::tie(volume_serial, file_index_high, file_index_low) <
            std::tie(other.volume_serial, other.file_index_high, other.file_index_low);
    }
};

physical_file_identity read_physical_file_identity(const std::wstring& native) {
    const HANDLE handle = CreateFileW(
        native.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        throw pfc::exception("Unable to verify physical source file identity.");
    }
    BY_HANDLE_FILE_INFORMATION info = {};
    const bool success = GetFileInformationByHandle(handle, &info) != 0;
    CloseHandle(handle);
    if (!success || (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (info.nFileIndexHigh == 0 && info.nFileIndexLow == 0)) {
        throw pfc::exception("Physical source has no trustworthy regular-file identity.");
    }
    return {info.dwVolumeSerialNumber, info.nFileIndexHigh, info.nFileIndexLow};
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

    // O(N log N) distinct-source deduplication, rather than N linear scans
    // through previously created foobar handles.
    std::set<std::string, physical_source_less> unique_physical_sources;
    // Filesystem and Windows ordinal Unicode case rules are not identical.
    // Preserve the old foobar-aware comparator, while ensuring *every*
    // non-byte-identical duplicate proves actual file identity. This also
    // handles the rare case-sensitive Windows directory without writing.
    std::map<std::string, std::string, physical_source_less> canonical_native_lookup;
    // A repeated canonical string is not a permanent file identity: its
    // underlying target can change while the selection is being resolved.
    std::map<std::string, physical_file_identity, physical_source_less> canonical_file_id_lookup;
    std::map<std::wstring, std::string, windows_path_case_less> canonical_case_lookup;
    std::map<physical_file_identity, std::string> seen_physical_file_ids;
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
            const std::string native(native_path.c_str());
            const auto original = unique_physical_sources.find(key);
            if (original != unique_physical_sources.end()) {
                // The foobar-aware comparator can equate *different strings*.
                // Never accept that as proof the underlying physical file
                // is the same on a case-sensitive Windows directory.
                if (*original != key) {
                    const auto previously_mapped = canonical_native_lookup.find(*original);
                    if (previously_mapped == canonical_native_lookup.end() ||
                        !same_physical_file(native, previously_mapped->second)) {
                        reject_target(
                            result, source_path,
                            "Equivalent source path could not be verified as the same physical file.");
                    }
                } else {
                    // Even a byte-identical canonical path may have been
                    // replaced/repointed since its first occurrence. Never
                    // silently drop a second selected file on that basis.
                    const auto previous_id = canonical_file_id_lookup.find(*original);
                    const physical_file_identity current_duplicate_id =
                        read_physical_file_identity(unicode_path_from_utf8(native));
                    if (previous_id == canonical_file_id_lookup.end() ||
                        !(current_duplicate_id == previous_id->second)) {
                        reject_target(
                            result, source_path,
                            "Repeated canonical path no longer identifies the original physical file.");
                    }
                }
                continue;
            }

            const std::wstring unicode_native = unicode_path_from_utf8(native);
            const auto collision = canonical_case_lookup.find(unicode_native);
            if (collision != canonical_case_lookup.end()) {
                if (!same_physical_file(native, collision->second)) {
                    reject_target(
                        result, source_path,
                        "Case-only source alias could not be verified as the same physical file.");
                }
                // Proven same file: already mapped. Ambiguous/different file:
                // rejected selection blocks all delegated physical writes.
                continue;
            }

            // Distinct filenames and hardlinks can refer to one physical file,
            // even when no case folding or canonical-path comparison matches.
            // Never open or write the same underlying file twice through
            // distinct names in a multi-selection.
            const physical_file_identity file_id =
                read_physical_file_identity(unicode_native);
            const auto known_identity = seen_physical_file_ids.find(file_id);
            if (known_identity != seen_physical_file_ids.end()) {
                // An identical reported file ID is not by itself proof that
                // two different names resolve to the same file (for example,
                // on a filesystem with a wider native identity). Never
                // silently omit a selected physical target on an ambiguous
                // identity: fail the entire delegated write selection closed.
                if (!same_physical_file(native, known_identity->second)) {
                    reject_target(
                        result, source_path,
                        "Matching file ID could not be verified as the same physical file.");
                }
                continue;
            }

            // Add the handle before marking source as mapped. Failed handle
            // construction must never conceal a later valid selection.
            result.handles.add_item(metadb::get()->handle_create(canonical_path, 0));
            unique_physical_sources.insert(key);
            canonical_native_lookup.emplace(key, native);
            canonical_file_id_lookup.emplace(key, file_id);
            canonical_case_lookup.emplace(unicode_native, native);
            seen_physical_file_ids.emplace(file_id, native);
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
