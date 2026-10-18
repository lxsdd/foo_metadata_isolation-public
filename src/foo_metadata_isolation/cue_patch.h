#pragma once

#include <string>
#include <vector>

namespace metadata_isolation {

enum class cue_field_edit_kind {
    replace,
    insert,
    remove,
};

struct cue_field_edit {
    // 0 addresses the unique document/global scope before the first TRACK.
    unsigned track_number = 0;
    std::string command;
    std::string new_value;
    cue_field_edit_kind kind = cue_field_edit_kind::replace;
};

struct cue_patch_result {
    std::string text;
    size_t replacement_count = 0;
};

struct cue_track_number_edit {
    unsigned before = 0;
    unsigned after = 0;
};

enum class cue_timing_directive {
    index,
    pregap,
};

enum class cue_timing_edit_kind {
    replace,
    insert,
    remove,
};

struct cue_timing_value {
    cue_timing_directive directive = cue_timing_directive::index;
    unsigned index_number = 1;
    unsigned ticks = 0;
};

struct cue_track_timing {
    unsigned track_number = 0;
    std::vector<cue_timing_value> values;
};

struct cue_timing_document {
    std::vector<cue_track_timing> tracks;
};

struct cue_timing_edit {
    unsigned track_number = 0;
    cue_timing_directive directive = cue_timing_directive::index;
    unsigned index_number = 1;
    cue_timing_edit_kind kind = cue_timing_edit_kind::replace;
    unsigned before_ticks = 0;
    unsigned after_ticks = 0;
};

// Applies minimal directive insertions, value-span replacements and whole-line
// removals to the original Cue text. The function never serializes a Cue model.
// Duplicate or missing targets, unsafe syntax, ambiguous anchors, overlapping
// requests and changed source bytes throw.
cue_patch_result patch_cue_track_fields(
    const std::string& original,
    const std::vector<cue_field_edit>& edits);

// Replaces only the decimal number tokens of existing TRACK directives. The
// complete original and resulting document must use unique, strictly
// increasing TRACK numbers in raw block order, all in Cue's 1..999 range.
// No block movement, insertion or removal is inferred from number changes.
cue_patch_result patch_cue_track_numbers(
    const std::string& original,
    const std::vector<cue_track_number_edit>& edits);

// Parses foobar-visible INDEX 00..99 and PREGAP state as exact 75 Hz ticks.
// TRACK order, mandatory INDEX 01, directive uniqueness, timing order and
// cross-track boundaries are validated document-wide. INDEX 100+ is retained
// by patching but intentionally not exposed in the returned timing model.
cue_timing_document parse_cue_timing(const std::string& original);

// Applies atomic token-/line-level INDEX or PREGAP edits. Optional directives
// may be inserted or removed; INDEX 01 is replace-only. The complete result is
// reparsed and must remain a coherent document before it is returned.
cue_patch_result patch_cue_timing(
    const std::string& original,
    const std::vector<cue_timing_edit>& edits);

// Strict user-facing Cue time helpers. Input must be MM:SS:FF with seconds in
// 0..59 and frames in 0..74; output uses at least two minute digits.
unsigned parse_cue_time_75(const std::string& value);
std::string format_cue_time_75(unsigned ticks);

// Returns the number of matching value-bearing directives in the requested
// scope. For a track scope, duplicate TRACK blocks make the result ambiguous
// and throw. This is used by the capability planner before it chooses global
// versus per-track representation.
size_t cue_field_occurrence_count(
    const std::string& original,
    unsigned track_number,
    const std::string& command);

// Returns the decoded value span of a unique existing directive. Missing or
// duplicate directives, duplicate TRACK blocks and unsafe syntax throw.
std::string cue_field_value(
    const std::string& original,
    unsigned track_number,
    const std::string& command);

} // namespace metadata_isolation
