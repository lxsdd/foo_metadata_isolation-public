#include "stdafx.h"
#include "cue_patch.h"
#include "metadata_diff.h"
#include "physical_metadata_audit.h"

#include <algorithm>
#include <cctype>

namespace metadata_isolation {
namespace {

struct field_values {
    bool present = false;
    bool multivalue = false;
    std::string display;
    std::string single;
};

bool qualified_textual_cue_path(const char* path) {
    const char* extension = filesystem::g_get_extension(path);
    return pfc::stricmp_ascii(extension, "mp3") == 0 ||
        pfc::stricmp_ascii(extension, "flac") == 0 ||
        pfc::stricmp_ascii(extension, "wv") == 0;
}

std::string extension_name(const char* path) {
    const char* extension = filesystem::g_get_extension(path);
    if (extension == nullptr || *extension == 0) return {};
    std::string output(extension);
    std::transform(output.begin(), output.end(), output.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });
    return output;
}

void split_path(const char* path, std::string& folder, std::string& file_name) {
    const std::string value(path == nullptr ? "" : path);
    const size_t separator = value.find_last_of("\\/");
    if (separator == std::string::npos) {
        folder.clear();
        file_name = value;
    } else {
        folder = value.substr(0, separator);
        file_name = value.substr(separator + 1);
    }
}

bool has_non_ascii_space(const char* value) {
    if (value == nullptr) return false;
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(value);
         *cursor != 0; ++cursor) {
        if (*cursor > 0x20) return true;
    }
    return false;
}

std::string display_safe_value(const char* value) {
    std::string output(value == nullptr ? "" : value);
    for (char& ch : output) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (byte < 0x20 && ch != 0) ch = ' ';
    }
    return output;
}

field_values read_field(const file_info& info, const char* name) {
    field_values output;
    const size_t index = info.meta_find(name);
    if (index == SIZE_MAX) return output;
    const size_t count = info.meta_enum_value_count(index);
    bool first = true;
    for (size_t value_index = 0; value_index < count; ++value_index) {
        const char* raw = info.meta_enum_value(index, value_index);
        if (!has_non_ascii_space(raw)) continue;
        if (!first) output.display += " | ";
        output.display += display_safe_value(raw);
        if (output.single.empty()) output.single = raw;
        first = false;
    }
    output.present = !output.single.empty();
    output.multivalue = output.present && count > 1;
    return output;
}

void append_list(std::string& output, const char* value) {
    if (!output.empty()) output += ", ";
    output += value;
}

enum class cue_compare_state {
    missing,
    unique,
    ambiguous,
};

cue_compare_state read_unique_global_cue_value(
    const std::string& cue,
    const char* canonical_field,
    std::string& output) {
    std::string command;
    if (!global_cue_command_for_field(canonical_field, command)) {
        return cue_compare_state::missing;
    }
    const size_t count = cue_field_occurrence_count(cue, 0, command);
    if (count == 0) return cue_compare_state::missing;
    if (count != 1) return cue_compare_state::ambiguous;
    output = cue_field_value(cue, 0, command);
    if (!has_non_ascii_space(output.c_str())) return cue_compare_state::ambiguous;
    return cue_compare_state::unique;
}

void compare_field(
    const std::string& cue,
    const char* canonical_field,
    const field_values& physical,
    const char* display_name,
    std::string& cue_display,
    std::string& differences,
    std::string& review) {
    std::string cue_value;
    const cue_compare_state cue_state =
        read_unique_global_cue_value(cue, canonical_field, cue_value);
    if (cue_state == cue_compare_state::ambiguous) {
        append_list(review, display_name);
        return;
    }
    if (cue_state != cue_compare_state::unique) return;
    cue_display = cue_value;
    if (!physical.present) return;
    if (physical.multivalue) {
        append_list(review, display_name);
        return;
    }
    if (physical.single != cue_value) append_list(differences, display_name);
}

physical_audit_row audit_one(
    const metadb_handle_ptr& target,
    const file_info& physical,
    const std::string& cue) {
    physical_audit_row row;
    row.target = target;
    row.path = target->get_path();
    split_path(row.path.c_str(), row.folder, row.file_name);
    row.format = extension_name(row.path.c_str());

    const physical_audit_assessment assessment =
        assess_physical_metadata(physical, cue);
    row.artist = assessment.artist;
    row.album_artist = assessment.album_artist;
    row.album = assessment.album;
    row.date = assessment.date;
    row.genre = assessment.genre;
    row.cue_artist = assessment.cue_artist;
    row.cue_album = assessment.cue_album;
    row.cue_date = assessment.cue_date;
    row.cue_genre = assessment.cue_genre;
    row.missing_fields = assessment.missing_fields;
    row.cue_differences = assessment.cue_differences;
    row.review_reason = assessment.review_reason;
    row.status = assessment.status;
    return row;
}

} // namespace

physical_audit_assessment assess_physical_metadata(
    const file_info& physical,
    const std::string& cue) {
    physical_audit_assessment result;

    const field_values artist = read_field(physical, "ARTIST");
    const field_values album_artist = read_field(physical, "ALBUM ARTIST");
    const field_values album = read_field(physical, "ALBUM");
    const field_values date = read_field(physical, "DATE");
    const field_values genre = read_field(physical, "GENRE");

    result.artist = artist.display;
    result.album_artist = album_artist.display;
    result.album = album.display;
    result.date = date.display;
    result.genre = genre.display;

    const bool artist_group_present = album_artist.present || artist.present;
    if (!artist_group_present) append_list(result.missing_fields, "ARTIST / ALBUM ARTIST");
    if (!album.present) append_list(result.missing_fields, "ALBUM");
    if (!date.present) append_list(result.missing_fields, "DATE");
    if (!genre.present) append_list(result.missing_fields, "GENRE");

    const size_t missing_groups =
        (artist_group_present ? 0u : 1u) +
        (album.present ? 0u : 1u) +
        (date.present ? 0u : 1u) +
        (genre.present ? 0u : 1u);

    try {
        const field_values& artist_for_compare =
            album_artist.present ? album_artist : artist;
        compare_field(
            cue, "ALBUM ARTIST", artist_for_compare, "ARTIST / ALBUM ARTIST",
            result.cue_artist, result.cue_differences, result.review_reason);
        compare_field(
            cue, "ALBUM", album, "ALBUM",
            result.cue_album, result.cue_differences, result.review_reason);
        compare_field(
            cue, "DATE", date, "DATE",
            result.cue_date, result.cue_differences, result.review_reason);
        compare_field(
            cue, "GENRE", genre, "GENRE",
            result.cue_genre, result.cue_differences, result.review_reason);
    } catch (const std::exception&) {
        result.review_reason = "CUESHEET syntax / global scope";
    }

    if (missing_groups == 4) {
        result.status = physical_audit_status::empty;
    } else if (missing_groups != 0) {
        result.status = physical_audit_status::incomplete;
    } else if (!result.review_reason.empty()) {
        result.status = physical_audit_status::review;
    } else if (!result.cue_differences.empty()) {
        result.status = physical_audit_status::mismatch;
    } else {
        result.status = physical_audit_status::ok;
    }
    return result;
}

const char* physical_audit_status_name(physical_audit_status status) noexcept {
    switch (status) {
    case physical_audit_status::ok: return "OK";
    case physical_audit_status::empty: return "EMPTY";
    case physical_audit_status::incomplete: return "INCOMPLETE";
    case physical_audit_status::mismatch: return "MISMATCH";
    case physical_audit_status::review: return "REVIEW";
    }
    return "REVIEW";
}

bool physical_audit_status_is_problem(physical_audit_status status) noexcept {
    return status != physical_audit_status::ok;
}

physical_audit_batch audit_physical_metadata(
    metadb_handle_list_cref physical_targets,
    threaded_process_status& status,
    abort_callback& abort,
    const std::function<void(size_t done, size_t total)>& progress) {
    physical_audit_batch output;
    output.mapped_sources = physical_targets.get_count();
    const size_t total = physical_targets.get_count();
    status.set_progress(0, total);

    for (size_t index = 0; index < total; ++index) {
        abort.check();
        const metadb_handle_ptr target = physical_targets[index];
        status.set_item(target->get_path());
        status.set_progress(index, total);
        if (progress) progress(index, total);

        if (!qualified_textual_cue_path(target->get_path())) {
            ++output.skipped_unqualified_carriers;
            continue;
        }

        try {
            input_info_reader::ptr reader;
            input_entry::g_open_for_info_read(
                reader, nullptr, target->get_path(), abort);
            file_info_impl physical;
            reader->get_info(0, physical, abort);

            const size_t cue_index = physical.meta_find("CUESHEET");
            if (cue_index == SIZE_MAX) continue;
            const size_t cue_values = physical.meta_enum_value_count(cue_index);
            ++output.textual_cue_sources;

            if (cue_values != 1) {
                physical_audit_row row;
                row.target = target;
                row.path = target->get_path();
                split_path(row.path.c_str(), row.folder, row.file_name);
                row.format = extension_name(row.path.c_str());
                row.status = physical_audit_status::review;
                row.review_reason = "Physical CUESHEET has multiple values";
                output.rows.push_back(std::move(row));
                continue;
            }

            output.rows.push_back(audit_one(
                target, physical, physical.meta_enum_value(cue_index, 0)));
        } catch (const exception_aborted&) {
            throw;
        } catch (const std::exception&) {
            ++output.read_failures;
            physical_audit_row row;
            row.target = target;
            row.path = target->get_path();
            split_path(row.path.c_str(), row.folder, row.file_name);
            row.format = extension_name(row.path.c_str());
            row.status = physical_audit_status::review;
            row.review_reason = "Physical metadata could not be read";
            output.rows.push_back(std::move(row));
        }
    }

    status.set_progress(total, total);
    if (progress) progress(total, total);
    return output;
}

} // namespace metadata_isolation
