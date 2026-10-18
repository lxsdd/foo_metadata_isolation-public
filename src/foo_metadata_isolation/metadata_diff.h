#pragma once

#include <map>
#include <string>
#include <vector>

namespace metadata_isolation {

enum class virtual_field_status {
    safe_direct,
    safe_transformed,
    candidate_global,
    ambiguous,
    structural,
    unsupported,
    unknown,
};

enum class metadata_change_kind {
    add,
    change,
    remove,
};

// A true foobar2000 multi-value transition is classified from the exact
// file_info value vectors. No display separator parsing or normalization is
// involved. This is deliberately independent from add/change/remove so the
// existing single-value transaction semantics stay unchanged.
enum class metadata_multivalue_operation {
    none,
    add,
    change,
    delete_one,
    delete_all,
    reorder,
};

struct virtual_multivalue_capability {
    bool read = false;
    bool add = false;
    bool change = false;
    bool delete_one = false;
    bool delete_all = false;
    bool reorder = false;
    bool duplicates = false;
    bool roundtrip = false;
};

enum class virtual_reject_reason {
    none,
    unsupported_field,
    ambiguous_scope,
    inheritance_conflict,
    partial_global_selection,
    structural_change,
    multivalue_unsupported,
    empty_value_unsafe,
    delete_semantics_unsafe,
    malformed_cue,
    postimage_mismatch,
};

enum class virtual_diff_class {
    committable,
    known_skippable,
    blocking,
};

struct metadata_field_change {
    std::string name;
    std::vector<std::string> before;
    std::vector<std::string> after;
    metadata_change_kind kind = metadata_change_kind::change;
    metadata_multivalue_operation multivalue_operation =
        metadata_multivalue_operation::none;
    bool multivalue_duplicates = false;
    virtual_field_status status = virtual_field_status::unknown;
    virtual_reject_reason reject_reason = virtual_reject_reason::none;
    virtual_diff_class diff_class = virtual_diff_class::blocking;
    std::string cue_command;
    std::string reason;
};

enum class replaygain_component {
    track_gain,
    track_peak,
    album_gain,
    album_peak,
};

struct replaygain_value {
    bool present = false;
    float value = 0;

    bool operator==(const replaygain_value& other) const {
        return present == other.present && (!present || value == other.value);
    }
    bool operator!=(const replaygain_value& other) const { return !(*this == other); }
};

struct replaygain_change {
    replaygain_component component = replaygain_component::track_gain;
    replaygain_value before;
    replaygain_value after;
    metadata_change_kind kind = metadata_change_kind::change;
};

struct metadata_diff_result {
    std::vector<metadata_field_change> changes;
    std::vector<replaygain_change> replaygain_changes;

    bool empty() const { return changes.empty() && replaygain_changes.empty(); }
    bool committable() const;
};

virtual_field_status classify_virtual_field(
    const std::string& canonical_name,
    std::string& cue_command,
    std::string& reason);

// Returns the document/global Cue directive for fields whose semantics can be
// resolved safely by the session planner. Track-capable fields may use this as
// a fallback only when every virtual subsong is selected and no track override
// exists.
bool global_cue_command_for_field(
    const std::string& canonical_name,
    std::string& cue_command);

const char* virtual_field_status_name(virtual_field_status status);
const char* metadata_change_kind_name(metadata_change_kind kind);
const char* metadata_multivalue_operation_name(metadata_multivalue_operation operation);
const char* virtual_reject_reason_name(virtual_reject_reason reason);
const char* virtual_diff_class_name(virtual_diff_class value);
const char* replaygain_component_name(replaygain_component component);
const char* replaygain_cue_command(replaygain_component component);
replaygain_value replaygain_component_value(
    const replaygain_info& state,
    replaygain_component component);
virtual_multivalue_capability multivalue_capability_for_field(
    const std::string& canonical_name);
metadata_diff_result diff_shadow_metadata(const file_info& before, const file_info& after);

} // namespace metadata_isolation
