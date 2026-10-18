#include "stdafx.h"
#include "metadata_diff.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace metadata_isolation {
namespace {

using metadata_map = std::map<std::string, std::vector<std::string>>;

std::string canonicalize(const char* value) {
    std::string output(value == nullptr ? "" : value);
    std::transform(output.begin(), output.end(), output.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return output;
}

metadata_map capture_metadata(const file_info& info) {
    metadata_map output;
    for (size_t field = 0; field < info.meta_get_count(); ++field) {
        std::vector<std::string> values;
        for (size_t value = 0; value < info.meta_enum_value_count(field); ++value) {
            values.emplace_back(info.meta_enum_value(field, value));
        }
        output[canonicalize(info.meta_enum_name(field))] = std::move(values);
    }
    return output;
}

bool is_replaygain_name(const std::string& name) {
    return name == "REPLAYGAIN_TRACK_GAIN" || name == "REPLAYGAIN_TRACK_PEAK" ||
        name == "REPLAYGAIN_ALBUM_GAIN" || name == "REPLAYGAIN_ALBUM_PEAK";
}

using value_counts = std::map<std::string, size_t>;

value_counts count_values(const std::vector<std::string>& values) {
    value_counts output;
    for (const auto& value : values) ++output[value];
    return output;
}

bool has_duplicate_values(const std::vector<std::string>& values) {
    const value_counts counts = count_values(values);
    return std::any_of(counts.begin(), counts.end(), [](const auto& item) {
        return item.second > 1;
    });
}

bool is_one_value_addition(
    const std::vector<std::string>& before,
    const std::vector<std::string>& after) {
    if (after.size() != before.size() + 1) return false;
    value_counts remaining = count_values(after);
    for (const auto& value : before) {
        auto found = remaining.find(value);
        if (found == remaining.end() || found->second == 0) return false;
        --found->second;
    }
    return true;
}

metadata_multivalue_operation classify_multivalue_operation(
    const std::vector<std::string>& before,
    const std::vector<std::string>& after) {
    if (before.size() <= 1 && after.size() <= 1) {
        return metadata_multivalue_operation::none;
    }
    if (after.empty()) return metadata_multivalue_operation::delete_all;
    if (before.empty()) return metadata_multivalue_operation::add;
    if (count_values(before) == count_values(after)) {
        return metadata_multivalue_operation::reorder;
    }
    if (is_one_value_addition(before, after)) {
        return metadata_multivalue_operation::add;
    }
    if (is_one_value_addition(after, before)) {
        return metadata_multivalue_operation::delete_one;
    }
    return metadata_multivalue_operation::change;
}

bool is_known_skippable_field(const metadata_field_change& change) {
    // STYLE has no foobar-compatible Embedded-Cue representation in the
    // field lab, repeated-directive replay, Discogger and Masstagger evidence.
    // Keep this deliberately narrow: arbitrary custom and third-party fields
    // remain blocking until they receive their own equivalent qualification.
    return change.name == "STYLE" &&
        (change.status == virtual_field_status::unsupported ||
         change.status == virtual_field_status::unknown);
}

virtual_diff_class classify_diff_class(const metadata_field_change& change) {
    const bool one_safe_value =
        change.before.size() <= 1 && change.after.size() <= 1 &&
        (change.before.empty() || !change.before[0].empty()) &&
        (change.after.empty() || !change.after[0].empty());
    // A single-value TRACKNUMBER change is an already-qualified structural
    // candidate. The document-level planner still proves canonical numbers,
    // identity, uniqueness and raw order before any write. Structural
    // add/delete and every other structural field remain blocking here.
    if (change.name == "TRACKNUMBER" &&
        change.kind == metadata_change_kind::change && one_safe_value) {
        return virtual_diff_class::committable;
    }
    if (one_safe_value &&
        (change.status == virtual_field_status::safe_direct ||
         change.status == virtual_field_status::safe_transformed ||
         change.status == virtual_field_status::candidate_global)) {
        return virtual_diff_class::committable;
    }
    if (is_known_skippable_field(change)) {
        return virtual_diff_class::known_skippable;
    }
    return virtual_diff_class::blocking;
}

} // namespace

bool metadata_diff_result::committable() const {
    for (const auto& change : changes) {
        if (change.diff_class == virtual_diff_class::blocking) return false;
    }
    return true;
}

const char* replaygain_component_name(replaygain_component component) {
    switch (component) {
    case replaygain_component::track_gain: return "Track Gain";
    case replaygain_component::track_peak: return "Track Peak";
    case replaygain_component::album_gain: return "Album Gain";
    case replaygain_component::album_peak: return "Album Peak";
    }
    return "ReplayGain";
}

const char* replaygain_cue_command(replaygain_component component) {
    switch (component) {
    case replaygain_component::track_gain: return "REM REPLAYGAIN_TRACK_GAIN";
    case replaygain_component::track_peak: return "REM REPLAYGAIN_TRACK_PEAK";
    case replaygain_component::album_gain: return "REM REPLAYGAIN_ALBUM_GAIN";
    case replaygain_component::album_peak: return "REM REPLAYGAIN_ALBUM_PEAK";
    }
    return "";
}

replaygain_value replaygain_component_value(
    const replaygain_info& state,
    replaygain_component component) {
    replaygain_value output;
    switch (component) {
    case replaygain_component::track_gain:
        output.present = state.is_track_gain_present();
        output.value = state.m_track_gain;
        break;
    case replaygain_component::track_peak:
        output.present = state.is_track_peak_present();
        output.value = state.m_track_peak;
        break;
    case replaygain_component::album_gain:
        output.present = state.is_album_gain_present();
        output.value = state.m_album_gain;
        break;
    case replaygain_component::album_peak:
        output.present = state.is_album_peak_present();
        output.value = state.m_album_peak;
        break;
    }
    return output;
}

virtual_field_status classify_virtual_field(
    const std::string& canonical_name,
    std::string& cue_command,
    std::string& reason) {
    cue_command.clear();
    reason.clear();

    if (canonical_name == "TITLE") {
        cue_command = "TITLE";
        reason = "Maps directly to the existing track-level Cue TITLE token.";
        return virtual_field_status::safe_direct;
    }
    if (canonical_name == "ARTIST") {
        cue_command = "PERFORMER";
        reason = "Maps from foobar2000 ARTIST to the existing track-level Cue PERFORMER token.";
        return virtual_field_status::safe_transformed;
    }
    if (canonical_name == "SONGWRITER" || canonical_name == "ISRC") {
        cue_command = canonical_name;
        reason = "Maps directly to the existing native track-level Cue token; single-value roundtrip passed.";
        return virtual_field_status::safe_direct;
    }
    if (canonical_name == "DATE" || canonical_name == "COMMENT") {
        cue_command = "REM " + canonical_name;
        reason = "Maps to the existing track-level Cue REM token; single-value roundtrip passed.";
        return virtual_field_status::safe_direct;
    }
    if (canonical_name == "TRACKNUMBER" || canonical_name == "TOTALTRACKS" ||
        canonical_name == "TRACKTOTAL" || canonical_name == "DISCTOTAL" ||
        canonical_name == "INDEX" || canonical_name == "FILE") {
        reason = "This field identifies or changes Cue structure and is never patched by the metadata router.";
        return virtual_field_status::structural;
    }
    if (canonical_name == "ALBUM" || canonical_name == "ALBUM ARTIST" ||
        canonical_name == "GENRE" || canonical_name == "DISCNUMBER" ||
        canonical_name == "TOTALDISCS") {
        global_cue_command_for_field(canonical_name, cue_command);
        reason = "Requires complete-document selection and a unique global Cue representation.";
        return virtual_field_status::candidate_global;
    }
    if (canonical_name == "YEAR") {
        reason = "YEAR versus DATE semantics are not release-proven.";
        return virtual_field_status::ambiguous;
    }
    if (canonical_name == "COMPOSER") {
        reason = "A Cue representation exists or is plausible, but round-trip semantics are not release-proven.";
        return virtual_field_status::unknown;
    }
    if (is_replaygain_name(canonical_name)) {
        reason = "ReplayGain is classified through the dedicated numeric state, not normal metadata.";
        return virtual_field_status::unsupported;
    }
    reason = "No lossless Embedded-Cue representation has been proven for this field and scope.";
    return virtual_field_status::unknown;
}

bool global_cue_command_for_field(
    const std::string& canonical_name,
    std::string& cue_command) {
    cue_command.clear();
    if (canonical_name == "ALBUM") cue_command = "TITLE";
    else if (canonical_name == "ALBUM ARTIST" || canonical_name == "ARTIST") {
        cue_command = "PERFORMER";
    } else if (canonical_name == "SONGWRITER") cue_command = "SONGWRITER";
    else if (canonical_name == "DATE" || canonical_name == "COMMENT" ||
             canonical_name == "GENRE" || canonical_name == "DISCNUMBER" ||
             canonical_name == "TOTALDISCS") {
        cue_command = "REM " + canonical_name;
    }
    return !cue_command.empty();
}

const char* virtual_field_status_name(virtual_field_status status) {
    switch (status) {
    case virtual_field_status::safe_direct: return "SAFE_DIRECT";
    case virtual_field_status::safe_transformed: return "SAFE_TRANSFORMED";
    case virtual_field_status::candidate_global: return "CANDIDATE_GLOBAL";
    case virtual_field_status::ambiguous: return "AMBIGUOUS";
    case virtual_field_status::structural: return "STRUCTURAL";
    case virtual_field_status::unsupported: return "UNSUPPORTED";
    case virtual_field_status::unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* metadata_change_kind_name(metadata_change_kind kind) {
    switch (kind) {
    case metadata_change_kind::add: return "ADD";
    case metadata_change_kind::change: return "CHANGE";
    case metadata_change_kind::remove: return "DELETE";
    }
    return "CHANGE";
}

const char* metadata_multivalue_operation_name(metadata_multivalue_operation operation) {
    switch (operation) {
    case metadata_multivalue_operation::none: return "NONE";
    case metadata_multivalue_operation::add: return "MULTIVALUE_ADD";
    case metadata_multivalue_operation::change: return "MULTIVALUE_CHANGE";
    case metadata_multivalue_operation::delete_one: return "MULTIVALUE_DELETE_ONE";
    case metadata_multivalue_operation::delete_all: return "MULTIVALUE_DELETE_ALL";
    case metadata_multivalue_operation::reorder: return "MULTIVALUE_REORDER";
    }
    return "NONE";
}

virtual_multivalue_capability multivalue_capability_for_field(
    const std::string&) {
    // The field lab and Cue-only replay prove that no tested field currently
    // has an exact value-vector roundtrip through foobar2000's embedded-Cue
    // text model. Keep the shape granular so a future field can be enabled per
    // operation only after field-specific runtime evidence exists.
    return {};
}

const char* virtual_reject_reason_name(virtual_reject_reason reason) {
    switch (reason) {
    case virtual_reject_reason::none: return "NONE";
    case virtual_reject_reason::unsupported_field: return "UNSUPPORTED_FIELD";
    case virtual_reject_reason::ambiguous_scope: return "AMBIGUOUS_SCOPE";
    case virtual_reject_reason::inheritance_conflict: return "INHERITANCE_CONFLICT";
    case virtual_reject_reason::partial_global_selection: return "PARTIAL_GLOBAL_SELECTION";
    case virtual_reject_reason::structural_change: return "STRUCTURAL_CHANGE";
    case virtual_reject_reason::multivalue_unsupported: return "MULTIVALUE_UNSUPPORTED";
    case virtual_reject_reason::empty_value_unsafe: return "EMPTY_VALUE_UNSAFE";
    case virtual_reject_reason::delete_semantics_unsafe: return "DELETE_SEMANTICS_UNSAFE";
    case virtual_reject_reason::malformed_cue: return "MALFORMED_CUE";
    case virtual_reject_reason::postimage_mismatch: return "POSTIMAGE_MISMATCH";
    }
    return "UNSUPPORTED_FIELD";
}

const char* virtual_diff_class_name(virtual_diff_class value) {
    switch (value) {
    case virtual_diff_class::committable: return "COMMITTABLE";
    case virtual_diff_class::known_skippable: return "KNOWN_SKIPPABLE";
    case virtual_diff_class::blocking: return "BLOCKING";
    }
    return "BLOCKING";
}

metadata_diff_result diff_shadow_metadata(const file_info& before, const file_info& after) {
    const metadata_map before_map = capture_metadata(before);
    const metadata_map after_map = capture_metadata(after);
    std::set<std::string> names;
    for (const auto& field : before_map) names.insert(field.first);
    for (const auto& field : after_map) names.insert(field.first);

    metadata_diff_result output;
    const replaygain_info before_rg = before.get_replaygain();
    const replaygain_info after_rg = after.get_replaygain();
    constexpr replaygain_component replaygain_components[] = {
        replaygain_component::track_gain,
        replaygain_component::track_peak,
        replaygain_component::album_gain,
        replaygain_component::album_peak,
    };
    for (replaygain_component component : replaygain_components) {
        const replaygain_value before_value = replaygain_component_value(before_rg, component);
        const replaygain_value after_value = replaygain_component_value(after_rg, component);
        if (before_value == after_value) continue;
        replaygain_change change;
        change.component = component;
        change.before = before_value;
        change.after = after_value;
        change.kind = !before_value.present ? metadata_change_kind::add :
            !after_value.present ? metadata_change_kind::remove : metadata_change_kind::change;
        output.replaygain_changes.push_back(change);
    }
    for (const auto& name : names) {
        const auto before_it = before_map.find(name);
        const auto after_it = after_map.find(name);
        const std::vector<std::string> before_values =
            before_it == before_map.end() ? std::vector<std::string>() : before_it->second;
        const std::vector<std::string> after_values =
            after_it == after_map.end() ? std::vector<std::string>() : after_it->second;
        if (before_values == after_values) continue;

        metadata_field_change change;
        change.name = name;
        change.before = before_values;
        change.after = after_values;
        change.kind = before_values.empty() ? metadata_change_kind::add :
            after_values.empty() ? metadata_change_kind::remove : metadata_change_kind::change;
        change.multivalue_operation = classify_multivalue_operation(
            before_values, after_values);
        change.multivalue_duplicates = has_duplicate_values(before_values) ||
            has_duplicate_values(after_values);
        change.status = classify_virtual_field(name, change.cue_command, change.reason);
        if (change.status == virtual_field_status::structural) {
            change.reject_reason = virtual_reject_reason::structural_change;
        } else if (change.status == virtual_field_status::unsupported ||
                   change.status == virtual_field_status::unknown) {
            change.reject_reason = virtual_reject_reason::unsupported_field;
        } else if (change.status == virtual_field_status::ambiguous) {
            change.reject_reason = virtual_reject_reason::ambiguous_scope;
        }
        if (change.multivalue_operation != metadata_multivalue_operation::none) {
            const virtual_multivalue_capability capability =
                multivalue_capability_for_field(name);
            change.status = virtual_field_status::unsupported;
            change.reject_reason = virtual_reject_reason::multivalue_unsupported;
            change.reason = capability.roundtrip
                ? "This exact multi-value operation is not qualified for the field."
                : "This operation produced a true multi-value virtual field that cannot be represented losslessly in this embedded Cuesheet. The original file has not been modified.";
        } else if ((!before_values.empty() && before_values[0].empty()) ||
                   (!after_values.empty() && after_values[0].empty())) {
            change.status = virtual_field_status::unsupported;
            change.reject_reason = virtual_reject_reason::empty_value_unsafe;
            change.reason = "An empty value is not treated as deletion and has no proven lossless Cue roundtrip.";
        }
        change.diff_class = classify_diff_class(change);
        output.changes.push_back(std::move(change));
    }
    return output;
}

} // namespace metadata_isolation
