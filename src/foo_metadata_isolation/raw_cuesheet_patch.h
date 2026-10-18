#pragma once

namespace metadata_isolation {

enum class raw_cuesheet_storage_kind {
    id3v2,
    flac_vorbis_comment,
    wavpack_apev2,
};

raw_cuesheet_storage_kind identify_raw_cuesheet_storage(
    const char* source_path,
    const char* expected_cuesheet,
    abort_callback& abort);

const char* raw_cuesheet_storage_extension(raw_cuesheet_storage_kind kind);

void write_raw_cuesheet_patch(
    raw_cuesheet_storage_kind kind,
    const char* source_path,
    const char* output_path,
    const char* expected_before,
    const char* replacement,
    abort_callback& abort);

} // namespace metadata_isolation
