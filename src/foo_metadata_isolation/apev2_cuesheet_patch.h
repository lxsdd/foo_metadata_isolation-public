#pragma once

namespace metadata_isolation {

// Validates the qualified WavPack carrier slice: one trailing APEv2.0 tag,
// exactly one NUL-free UTF-8 text item named CUESHEET, and no trailing data.
void validate_raw_wavpack_apev2_cuesheet(
    const char* source_path,
    const char* expected_cuesheet,
    abort_callback& abort);

// Creates a complete patched copy. Only the existing CUESHEET item value,
// its item length and the enclosing APEv2 size fields may change. The WavPack
// payload and every other APEv2 byte are copied unchanged.
void write_raw_wavpack_apev2_cuesheet_patch(
    const char* source_path,
    const char* output_path,
    const char* expected_before,
    const char* replacement,
    abort_callback& abort);

} // namespace metadata_isolation
