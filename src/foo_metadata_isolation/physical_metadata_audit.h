#pragma once

#include <functional>
#include <string>
#include <vector>

namespace metadata_isolation {

enum class physical_audit_status {
    ok,
    empty,
    incomplete,
    mismatch,
    review,
};

struct physical_audit_assessment {
    std::string artist;
    std::string album_artist;
    std::string album;
    std::string date;
    std::string genre;
    std::string cue_artist;
    std::string cue_album;
    std::string cue_date;
    std::string cue_genre;
    std::string missing_fields;
    std::string cue_differences;
    std::string review_reason;
    physical_audit_status status = physical_audit_status::ok;
};

physical_audit_assessment assess_physical_metadata(
    const file_info& physical,
    const std::string& cue);

struct physical_audit_row {
    metadb_handle_ptr target;
    std::string path;
    std::string file_name;
    std::string folder;
    std::string format;
    std::string artist;
    std::string album_artist;
    std::string album;
    std::string date;
    std::string genre;
    std::string cue_artist;
    std::string cue_album;
    std::string cue_date;
    std::string cue_genre;
    std::string missing_fields;
    std::string cue_differences;
    std::string review_reason;
    physical_audit_status status = physical_audit_status::ok;
};

struct physical_audit_batch {
    std::vector<physical_audit_row> rows;
    size_t mapped_sources = 0;
    size_t textual_cue_sources = 0;
    size_t skipped_unqualified_carriers = 0;
    size_t read_failures = 0;
};

const char* physical_audit_status_name(physical_audit_status status) noexcept;
bool physical_audit_status_is_problem(physical_audit_status status) noexcept;

// Reads the physical source only (subsong 0), never writes. Only the qualified
// MP3/FLAC/WavPack textual-CUESHEET carriers are audited in the v1.0 scope.
physical_audit_batch audit_physical_metadata(
    metadb_handle_list_cref physical_targets,
    threaded_process_status& status,
    abort_callback& abort,
    const std::function<void(size_t done, size_t total)>& progress = {});

} // namespace metadata_isolation
