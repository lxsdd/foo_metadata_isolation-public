#pragma once

namespace metadata_isolation {

struct id3_cuesheet_patch_result {
    unsigned major_version = 0;
    t_filesize original_tag_extent = 0;
    t_filesize patched_tag_extent = 0;
    size_t old_padding_bytes = 0;
    size_t new_padding_bytes = 0;
};

// Creates a complete patched copy at output_path. Only one existing, plain
// ID3v2.3/v2.4 TXXX:CUESHEET value may change; every byte after the leading
// ID3v2 tag and every non-target frame byte is copied unchanged.
id3_cuesheet_patch_result write_raw_id3v2_cuesheet_patch(
    const char* source_path,
    const char* output_path,
    const char* expected_before,
    const char* replacement,
    abort_callback& abort);

} // namespace metadata_isolation
