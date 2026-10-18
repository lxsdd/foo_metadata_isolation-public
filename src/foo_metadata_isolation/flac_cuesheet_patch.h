#pragma once

namespace metadata_isolation {

// Validates the exact raw storage used by the first FLAC release slice:
// one Vorbis Comment block, exactly one CUESHEET entry and no competing
// native FLAC CUESHEET metadata block.
void validate_raw_flac_vorbis_cuesheet(
    const char* source_path,
    const char* expected_cuesheet,
    abort_callback& abort);

// Creates a complete patched copy. Only the existing CUESHEET comment value,
// its field length and the enclosing Vorbis Comment block length may change.
// Every other metadata-block byte and the complete audio-frame payload are
// copied unchanged.
void write_raw_flac_vorbis_cuesheet_patch(
    const char* source_path,
    const char* output_path,
    const char* expected_before,
    const char* replacement,
    abort_callback& abort);

} // namespace metadata_isolation
