#include "stdafx.h"
#include "apev2_cuesheet_patch.h"
#include "flac_cuesheet_patch.h"
#include "id3_cuesheet_patch.h"
#include "raw_cuesheet_patch.h"

namespace metadata_isolation {

raw_cuesheet_storage_kind identify_raw_cuesheet_storage(
    const char* source_path,
    const char* expected_cuesheet,
    abort_callback& abort) {
    const char* extension = filesystem::g_get_extension(source_path);
    if (pfc::stricmp_ascii(extension, "mp3") == 0) {
        return raw_cuesheet_storage_kind::id3v2;
    }
    if (pfc::stricmp_ascii(extension, "flac") == 0) {
        validate_raw_flac_vorbis_cuesheet(source_path, expected_cuesheet, abort);
        return raw_cuesheet_storage_kind::flac_vorbis_comment;
    }
    if (pfc::stricmp_ascii(extension, "wv") == 0) {
        validate_raw_wavpack_apev2_cuesheet(source_path, expected_cuesheet, abort);
        return raw_cuesheet_storage_kind::wavpack_apev2;
    }
    throw pfc::exception("Safe virtual routing supports only qualified MP3, FLAC and WavPack storage");
}

const char* raw_cuesheet_storage_extension(raw_cuesheet_storage_kind kind) {
    switch (kind) {
    case raw_cuesheet_storage_kind::id3v2: return "mp3";
    case raw_cuesheet_storage_kind::flac_vorbis_comment: return "flac";
    case raw_cuesheet_storage_kind::wavpack_apev2: return "wv";
    }
    throw pfc::exception("Unknown raw CUESHEET storage backend");
}

void write_raw_cuesheet_patch(
    raw_cuesheet_storage_kind kind,
    const char* source_path,
    const char* output_path,
    const char* expected_before,
    const char* replacement,
    abort_callback& abort) {
    switch (kind) {
    case raw_cuesheet_storage_kind::id3v2:
        (void)write_raw_id3v2_cuesheet_patch(
            source_path, output_path, expected_before, replacement, abort);
        return;
    case raw_cuesheet_storage_kind::flac_vorbis_comment:
        write_raw_flac_vorbis_cuesheet_patch(
            source_path, output_path, expected_before, replacement, abort);
        return;
    case raw_cuesheet_storage_kind::wavpack_apev2:
        write_raw_wavpack_apev2_cuesheet_patch(
            source_path, output_path, expected_before, replacement, abort);
        return;
    }
    throw pfc::exception("Unknown raw CUESHEET storage backend");
}

} // namespace metadata_isolation
