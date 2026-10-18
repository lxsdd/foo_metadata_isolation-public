#include "stdafx.h"
#include "cue_patch.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <map>
#include <set>
#include <tuple>

namespace metadata_isolation {
namespace {

struct located_value {
    size_t start = 0;
    size_t end = 0;
    std::string old_value;
    bool quoted = false;
    size_t line_start = 0;
    size_t line_after_end = 0;
};

struct insertion_point {
    size_t position = 0;
    std::string indentation;
    std::string line_ending;
};

struct located_track_number {
    unsigned value = 0;
    size_t start = 0;
    size_t end = 0;
};

struct located_timing_value {
    unsigned track_number = 0;
    cue_timing_directive directive = cue_timing_directive::index;
    unsigned index_number = 1;
    unsigned ticks = 0;
    size_t time_start = 0;
    size_t time_end = 0;
    size_t line_start = 0;
    size_t line_after_end = 0;
    std::string indentation;
    std::string line_ending;
};

struct located_timing_track {
    unsigned track_number = 0;
    size_t line_start = 0;
    size_t block_end = 0;
};

struct timing_scan {
    std::vector<located_timing_track> tracks;
    std::vector<located_timing_value> values;
};

bool ascii_equal(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) return false;
    for (size_t index = 0; index < left.size(); ++index) {
        if (std::toupper(static_cast<unsigned char>(left[index])) !=
            std::toupper(static_cast<unsigned char>(right[index]))) return false;
    }
    return true;
}

bool is_space(char value) { return value == ' ' || value == '\t'; }
bool is_command_char(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

located_value parse_value_span(
    const std::string& text,
    size_t arguments_start,
    size_t arguments_end) {
    while (arguments_start < arguments_end && is_space(text[arguments_start])) ++arguments_start;
    while (arguments_end > arguments_start && is_space(text[arguments_end - 1])) --arguments_end;
    if (arguments_start == arguments_end) throw pfc::exception("Cue field has an empty value");

    located_value output;
    if (text[arguments_start] == '"') {
        if (arguments_end - arguments_start < 2 || text[arguments_end - 1] != '"') {
            throw pfc::exception("Cue field has ambiguous quoted syntax");
        }
        output.start = arguments_start + 1;
        output.end = arguments_end - 1;
        if (text.find('"', output.start) < output.end) {
            throw pfc::exception("Cue field contains unsupported embedded quote syntax");
        }
        output.quoted = true;
    } else {
        if (text.find('"', arguments_start) < arguments_end) {
            throw pfc::exception("Cue field has ambiguous unquoted syntax");
        }
        output.start = arguments_start;
        output.end = arguments_end;
    }
    output.old_value = text.substr(output.start, output.end - output.start);
    return output;
}

struct field_scan {
    size_t track_count = 0;
    std::vector<located_value> candidates;
    std::vector<insertion_point> insertion_points;
};

std::string line_ending_at(const std::string& text, size_t line_end) {
    if (line_end < text.size() && text[line_end] == '\r') {
        return line_end + 1 < text.size() && text[line_end + 1] == '\n' ? "\r\n" : "\r";
    }
    if (line_end < text.size() && text[line_end] == '\n') return "\n";
    const size_t first = text.find_first_of("\r\n");
    if (first == std::string::npos) return "\n";
    return text[first] == '\r' && first + 1 < text.size() && text[first + 1] == '\n' ? "\r\n" :
        std::string(1, text[first]);
}

field_scan scan_field(
    const std::string& text,
    unsigned wanted_track,
    const std::string& wanted_command) {
    unsigned current_track = 0;
    field_scan output;

    size_t line_start = 0;
    while (line_start < text.size()) {
        size_t line_end = text.find_first_of("\r\n", line_start);
        if (line_end == std::string::npos) line_end = text.size();

        size_t cursor = line_start;
        while (cursor < line_end && is_space(text[cursor])) ++cursor;
        const size_t command_start = cursor;
        while (cursor < line_end && is_command_char(text[cursor])) ++cursor;
        const std::string command = text.substr(command_start, cursor - command_start);
        if (!command.empty() && cursor < line_end && is_space(text[cursor])) {
            while (cursor < line_end && is_space(text[cursor])) ++cursor;
            const size_t arguments_start = cursor;
            if (ascii_equal(command, "TRACK")) {
                size_t number_end = cursor;
                while (number_end < line_end && std::isdigit(static_cast<unsigned char>(text[number_end]))) {
                    ++number_end;
                }
                if (number_end > cursor &&
                    (number_end == line_end || is_space(text[number_end]))) {
                    const unsigned long long parsed =
                        std::stoull(text.substr(cursor, number_end - cursor));
                    if (parsed > std::numeric_limits<unsigned>::max()) {
                        current_track = std::numeric_limits<unsigned>::max();
                    } else {
                        current_track = static_cast<unsigned>(parsed);
                    }
                    if (wanted_track != 0 && current_track == wanted_track) ++output.track_count;
                } else {
                    current_track = UINT_MAX;
                }
            } else if (current_track == wanted_track) {
                const bool insertion_anchor =
                    (wanted_track == 0 && ascii_equal(command, "FILE")) ||
                    (wanted_track != 0 && ascii_equal(command, "INDEX"));
                if (insertion_anchor && output.insertion_points.empty()) {
                    output.insertion_points.push_back({
                        line_start,
                        text.substr(line_start, command_start - line_start),
                        line_ending_at(text, line_end),
                    });
                }
                const size_t qualifier_separator = wanted_command.find(' ');
                if (qualifier_separator == std::string::npos && ascii_equal(command, wanted_command)) {
                    located_value value = parse_value_span(text, arguments_start, line_end);
                    value.line_start = line_start;
                    value.line_after_end = line_end + line_ending_at(text, line_end).size();
                    if (line_end == text.size()) value.line_after_end = line_end;
                    output.candidates.push_back(std::move(value));
                } else if (qualifier_separator != std::string::npos &&
                           ascii_equal(command, wanted_command.substr(0, qualifier_separator))) {
                    const std::string qualifier = wanted_command.substr(qualifier_separator + 1);
                    size_t qualifier_end = arguments_start;
                    while (qualifier_end < line_end && !is_space(text[qualifier_end])) ++qualifier_end;
                    if (ascii_equal(
                            text.substr(arguments_start, qualifier_end - arguments_start), qualifier)) {
                        while (qualifier_end < line_end && is_space(text[qualifier_end])) ++qualifier_end;
                        located_value value = parse_value_span(text, qualifier_end, line_end);
                        value.line_start = line_start;
                        value.line_after_end = line_end + line_ending_at(text, line_end).size();
                        if (line_end == text.size()) value.line_after_end = line_end;
                        output.candidates.push_back(std::move(value));
                    }
                }
            }
        }

        if (line_end == text.size()) break;
        line_start = line_end + 1;
        if (text[line_end] == '\r' && line_start < text.size() && text[line_start] == '\n') ++line_start;
    }

    return output;
}

located_value locate_track_field(
    const std::string& text,
    unsigned wanted_track,
    const std::string& wanted_command) {
    field_scan scan = scan_field(text, wanted_track, wanted_command);
    if (wanted_track != 0 && scan.track_count != 1) {
        pfc::string_formatter message;
        message << "Cue TRACK " << wanted_track << " occurs " << scan.track_count << " times";
        throw pfc::exception(message);
    }
    if (scan.candidates.size() != 1) {
        pfc::string_formatter message;
        message << "Cue " << wanted_command.c_str();
        if (wanted_track == 0) message << " in global scope";
        else message << " on TRACK " << wanted_track;
        message << " occurs " << scan.candidates.size() << " times";
        throw pfc::exception(message);
    }
    return scan.candidates[0];
}

std::string normalize_command(const std::string& source) {
    if (source.empty()) throw pfc::exception("Cue patch request has an invalid command");
    size_t token_length = 0;
    bool qualifier = false;
    for (char character : source) {
        if (character == ' ') {
            if (token_length == 0) throw pfc::exception("Cue command has invalid token spacing");
            token_length = 0;
            qualifier = true;
        } else if (!is_command_char(character) &&
                   !(qualifier && (character == '_' ||
                       std::isdigit(static_cast<unsigned char>(character))))) {
            throw pfc::exception("Cue command contains an invalid token");
        } else {
            ++token_length;
        }
    }
    if (token_length == 0 || std::count(source.begin(), source.end(), ' ') > 1) {
        throw pfc::exception("Cue command must contain one token or REM plus one key");
    }
    std::string output = source;
    std::transform(output.begin(), output.end(), output.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return output;
}

std::string format_inserted_directive(
    const insertion_point& point,
    const std::string& command,
    const std::string& value) {
    const bool unquoted = command == "ISRC" ||
        command.rfind("REM REPLAYGAIN_", 0) == 0;
    std::string output = point.indentation + command + " ";
    if (!unquoted) output += '"';
    output += value;
    if (!unquoted) output += '"';
    output += point.line_ending;
    return output;
}

std::vector<located_track_number> scan_track_numbers(const std::string& text) {
    std::vector<located_track_number> output;
    size_t line_start = 0;
    while (line_start < text.size()) {
        size_t line_end = text.find_first_of("\r\n", line_start);
        if (line_end == std::string::npos) line_end = text.size();
        size_t cursor = line_start;
        while (cursor < line_end && is_space(text[cursor])) ++cursor;
        const size_t command_start = cursor;
        while (cursor < line_end && is_command_char(text[cursor])) ++cursor;
        if (ascii_equal(text.substr(command_start, cursor - command_start), "TRACK") &&
            cursor < line_end && is_space(text[cursor])) {
            while (cursor < line_end && is_space(text[cursor])) ++cursor;
            const size_t number_start = cursor;
            while (cursor < line_end && std::isdigit(static_cast<unsigned char>(text[cursor]))) {
                ++cursor;
            }
            if (number_start == cursor || cursor == line_end || !is_space(text[cursor])) {
                throw pfc::exception("Cue TRACK directive has ambiguous number syntax");
            }
            const unsigned long long parsed = std::stoull(
                text.substr(number_start, cursor - number_start));
            if (parsed < 1 || parsed > 999) {
                throw pfc::exception("Cue TRACK number is outside 1..999");
            }
            output.push_back({ static_cast<unsigned>(parsed), number_start, cursor });
        }
        if (line_end == text.size()) break;
        line_start = line_end + 1;
        if (text[line_end] == '\r' && line_start < text.size() && text[line_start] == '\n') {
            ++line_start;
        }
    }
    if (output.empty()) throw pfc::exception("Cue document contains no TRACK directives");
    for (size_t index = 1; index < output.size(); ++index) {
        if (output[index - 1].value >= output[index].value) {
            throw pfc::exception(
                "Cue TRACK numbers are not unique and strictly increasing in raw block order");
        }
    }
    return output;
}

std::string format_track_number(unsigned value, size_t original_width) {
    const std::string canonical = std::to_string(value);
    if (canonical.size() >= original_width) return canonical;
    return std::string(original_width - canonical.size(), '0') + canonical;
}

timing_scan scan_timing(const std::string& text) {
    timing_scan output;
    unsigned current_track = 0;
    size_t line_start = 0;
    while (line_start < text.size()) {
        size_t line_end = text.find_first_of("\r\n", line_start);
        if (line_end == std::string::npos) line_end = text.size();
        const std::string ending = line_ending_at(text, line_end);
        size_t line_after_end = line_end;
        if (line_end < text.size()) line_after_end += ending.size();

        size_t cursor = line_start;
        while (cursor < line_end && is_space(text[cursor])) ++cursor;
        const size_t command_start = cursor;
        while (cursor < line_end && is_command_char(text[cursor])) ++cursor;
        const std::string command = text.substr(command_start, cursor - command_start);
        if (!command.empty() && cursor < line_end && is_space(text[cursor])) {
            while (cursor < line_end && is_space(text[cursor])) ++cursor;
            if (ascii_equal(command, "TRACK")) {
                const size_t number_start = cursor;
                while (cursor < line_end && std::isdigit(static_cast<unsigned char>(text[cursor]))) {
                    ++cursor;
                }
                if (number_start == cursor || cursor == line_end || !is_space(text[cursor])) {
                    throw pfc::exception("Cue TRACK directive has ambiguous number syntax");
                }
                const unsigned long long parsed = std::stoull(
                    text.substr(number_start, cursor - number_start));
                if (parsed < 1 || parsed > 999) {
                    throw pfc::exception("Cue TRACK number is outside 1..999");
                }
                if (!output.tracks.empty()) output.tracks.back().block_end = line_start;
                current_track = static_cast<unsigned>(parsed);
                output.tracks.push_back({ current_track, line_start, text.size() });
            } else if (current_track != 0 &&
                       (ascii_equal(command, "INDEX") || ascii_equal(command, "PREGAP"))) {
                located_timing_value value;
                value.track_number = current_track;
                value.line_start = line_start;
                value.line_after_end = line_after_end;
                value.indentation = text.substr(line_start, command_start - line_start);
                value.line_ending = ending;
                if (ascii_equal(command, "INDEX")) {
                    value.directive = cue_timing_directive::index;
                    const size_t index_start = cursor;
                    while (cursor < line_end && std::isdigit(static_cast<unsigned char>(text[cursor]))) {
                        ++cursor;
                    }
                    if (index_start == cursor || cursor == line_end || !is_space(text[cursor])) {
                        throw pfc::exception("Cue INDEX directive has ambiguous number syntax");
                    }
                    const unsigned long long parsed = std::stoull(
                        text.substr(index_start, cursor - index_start));
                    if (parsed > 999) throw pfc::exception("Cue INDEX number is outside 0..999");
                    value.index_number = static_cast<unsigned>(parsed);
                    while (cursor < line_end && is_space(text[cursor])) ++cursor;
                } else {
                    value.directive = cue_timing_directive::pregap;
                    value.index_number = 0;
                }
                value.time_start = cursor;
                while (cursor < line_end && !is_space(text[cursor])) ++cursor;
                value.time_end = cursor;
                while (cursor < line_end && is_space(text[cursor])) ++cursor;
                if (value.time_start == value.time_end || cursor != line_end) {
                    throw pfc::exception("Cue timing directive has ambiguous time syntax");
                }
                value.ticks = parse_cue_time_75(
                    text.substr(value.time_start, value.time_end - value.time_start));
                output.values.push_back(std::move(value));
            }
        }

        if (line_end == text.size()) break;
        line_start = line_after_end;
    }
    if (output.tracks.empty()) throw pfc::exception("Cue document contains no TRACK directives");
    return output;
}

const located_timing_value* find_timing_value(
    const timing_scan& scan,
    unsigned track_number,
    cue_timing_directive directive,
    unsigned index_number) {
    const located_timing_value* output = nullptr;
    for (const auto& value : scan.values) {
        if (value.track_number != track_number || value.directive != directive ||
            (directive == cue_timing_directive::index && value.index_number != index_number)) {
            continue;
        }
        if (output != nullptr) throw pfc::exception("Duplicate Cue timing directive");
        output = &value;
    }
    return output;
}

cue_timing_document make_timing_document(const timing_scan& scan) {
    cue_timing_document output;
    unsigned previous_track = 0;
    for (const auto& located_track : scan.tracks) {
        if (located_track.track_number <= previous_track) {
            throw pfc::exception("Cue TRACK numbers are not strictly increasing");
        }
        previous_track = located_track.track_number;
        cue_track_timing track;
        track.track_number = located_track.track_number;
        std::set<std::pair<cue_timing_directive, unsigned>> unique;
        for (const auto& value : scan.values) {
            if (value.track_number != track.track_number) continue;
            if (value.directive == cue_timing_directive::index && value.index_number > 99) continue;
            const auto key = std::make_pair(value.directive, value.index_number);
            if (!unique.insert(key).second) throw pfc::exception("Duplicate Cue timing directive");
            track.values.push_back({ value.directive, value.index_number, value.ticks });
        }
        output.tracks.push_back(std::move(track));
    }

    const auto lookup = [](const cue_track_timing& track,
                           cue_timing_directive directive,
                           unsigned index_number) -> const cue_timing_value* {
        const cue_timing_value* found = nullptr;
        for (const auto& value : track.values) {
            if (value.directive == directive &&
                (directive == cue_timing_directive::pregap || value.index_number == index_number)) {
                if (found != nullptr) throw pfc::exception("Duplicate Cue timing directive");
                found = &value;
            }
        }
        return found;
    };

    for (size_t track_index = 0; track_index < output.tracks.size(); ++track_index) {
        const cue_track_timing& track = output.tracks[track_index];
        const cue_timing_value* index01 = lookup(track, cue_timing_directive::index, 1);
        if (index01 == nullptr) throw pfc::exception("Cue TRACK has no unique INDEX 01");
        const cue_timing_value* index00 = lookup(track, cue_timing_directive::index, 0);
        const cue_timing_value* pregap = lookup(track, cue_timing_directive::pregap, 0);
        if (index00 != nullptr && pregap != nullptr) {
            throw pfc::exception("Cue TRACK contains both INDEX 00 and PREGAP");
        }
        unsigned effective_index00 = index01->ticks;
        if (index00 != nullptr) {
            if (index00->ticks > index01->ticks) {
                throw pfc::exception("Cue INDEX 00 is after INDEX 01");
            }
            effective_index00 = index00->ticks;
        } else if (pregap != nullptr) {
            if (pregap->ticks > index01->ticks) {
                throw pfc::exception("Cue PREGAP begins before the physical timeline");
            }
            effective_index00 = index01->ticks - pregap->ticks;
        }
        if (track_index > 0) {
            const cue_timing_value* previous_index01 = lookup(
                output.tracks[track_index - 1], cue_timing_directive::index, 1);
            if (previous_index01 == nullptr || index01->ticks <= previous_index01->ticks) {
                throw pfc::exception("Cue INDEX 01 values are not strictly increasing");
            }
            if (effective_index00 < previous_index01->ticks) {
                throw pfc::exception("Cue pregap overlaps the previous track start");
            }
        }

        unsigned previous_index = index01->ticks;
        bool saw_additional = false;
        unsigned expected_number = 2;
        for (;;) {
            const cue_timing_value* additional = lookup(
                track, cue_timing_directive::index, expected_number);
            if (additional == nullptr) break;
            saw_additional = true;
            if (additional->ticks <= previous_index) {
                throw pfc::exception("Cue additional INDEX values are not strictly increasing");
            }
            if (track_index + 1 < output.tracks.size()) {
                const cue_timing_value* next_index01 = lookup(
                    output.tracks[track_index + 1], cue_timing_directive::index, 1);
                if (next_index01 == nullptr || additional->ticks >= next_index01->ticks) {
                    throw pfc::exception("Cue additional INDEX reaches the next track");
                }
            }
            previous_index = additional->ticks;
            ++expected_number;
        }
        for (const auto& value : track.values) {
            if (value.directive == cue_timing_directive::index && value.index_number >= 2 &&
                value.index_number != 1 && value.index_number >= expected_number) {
                if (!saw_additional || value.index_number != expected_number) {
                    throw pfc::exception("Cue additional INDEX numbers are not contiguous from 02");
                }
            }
        }
    }
    return output;
}

} // namespace

unsigned parse_cue_time_75(const std::string& value) {
    const size_t first = value.find(':');
    const size_t second = first == std::string::npos ? std::string::npos : value.find(':', first + 1);
    if (first < 2 || second == std::string::npos || value.find(':', second + 1) != std::string::npos ||
        second - first != 3 || value.size() - second != 3) {
        throw pfc::exception("Cue time must use MM:SS:FF syntax");
    }
    if (std::any_of(value.begin(), value.end(), [](unsigned char character) {
            return character != ':' && (character < '0' || character > '9');
        })) {
        throw pfc::exception("Cue time contains a non-numeric character");
    }
    const unsigned long long minutes = std::stoull(value.substr(0, first));
    const unsigned seconds = static_cast<unsigned>(std::stoul(value.substr(first + 1, 2)));
    const unsigned frames = static_cast<unsigned>(std::stoul(value.substr(second + 1, 2)));
    if (seconds >= 60 || frames >= 75) {
        throw pfc::exception("Cue time seconds or frames are outside their valid range");
    }
    constexpr unsigned long long ticks_per_minute = 60ULL * 75ULL;
    if (minutes > std::numeric_limits<unsigned>::max() / ticks_per_minute) {
        throw pfc::exception("Cue time is outside foobar2000's supported range");
    }
    const unsigned long long ticks = minutes * ticks_per_minute + seconds * 75ULL + frames;
    if (ticks > std::numeric_limits<unsigned>::max()) {
        throw pfc::exception("Cue time is outside foobar2000's supported range");
    }
    return static_cast<unsigned>(ticks);
}

std::string format_cue_time_75(unsigned ticks) {
    const unsigned frames = ticks % 75;
    const unsigned total_seconds = ticks / 75;
    const unsigned seconds = total_seconds % 60;
    const unsigned minutes = total_seconds / 60;
    char buffer[64] = {};
    sprintf_s(buffer, "%02u:%02u:%02u", minutes, seconds, frames);
    return buffer;
}

cue_patch_result patch_cue_track_fields(
    const std::string& original,
    const std::vector<cue_field_edit>& edits) {
    struct replacement {
        size_t start = 0;
        size_t end = 0;
        std::string old_text;
        std::string new_text;
        size_t request_index = 0;
    };
    std::vector<replacement> replacements;
    std::set<std::pair<unsigned, std::string>> unique_requests;

    for (size_t request_index = 0; request_index < edits.size(); ++request_index) {
        const auto& edit = edits[request_index];
        const std::string command = normalize_command(edit.command);
        if (!unique_requests.emplace(edit.track_number, command).second) {
            throw pfc::exception("Duplicate Cue patch request");
        }
        if (edit.kind != cue_field_edit_kind::remove &&
            (edit.new_value.empty() || edit.new_value.find_first_of("\"\r\n") != std::string::npos)) {
            throw pfc::exception("Cue value is empty or contains quote/newline");
        }
        if (edit.kind == cue_field_edit_kind::insert) {
            const field_scan scan = scan_field(original, edit.track_number, command);
            if (edit.track_number != 0 && scan.track_count != 1) {
                throw pfc::exception("Cue insertion requires one unique TRACK block");
            }
            if (!scan.candidates.empty()) throw pfc::exception("Cue insertion target already exists");
            if (scan.insertion_points.size() != 1) {
                throw pfc::exception("Cue insertion anchor is missing or ambiguous");
            }
            if (command == "ISRC" && std::any_of(
                    edit.new_value.begin(), edit.new_value.end(), [](char value) { return is_space(value); })) {
                throw pfc::exception("Inserted ISRC cannot contain whitespace");
            }
            const insertion_point& point = scan.insertion_points[0];
            replacements.push_back({
                point.position,
                point.position,
                {},
                format_inserted_directive(point, command, edit.new_value),
                request_index,
            });
            continue;
        }
        const located_value location = locate_track_field(original, edit.track_number, command);
        if (edit.kind == cue_field_edit_kind::remove) {
            replacements.push_back({
                location.line_start,
                location.line_after_end,
                original.substr(location.line_start, location.line_after_end - location.line_start),
                {},
                request_index,
            });
            continue;
        }
        const bool rem_value = command.rfind("REM ", 0) == 0;
        if (!location.quoted && !rem_value && std::any_of(edit.new_value.begin(), edit.new_value.end(), [](char value) {
                return is_space(value);
            })) {
            throw pfc::exception("Unquoted Cue field cannot safely receive whitespace");
        }
        replacements.push_back({
            location.start,
            location.end,
            location.old_value,
            edit.new_value,
            request_index,
        });
    }

    std::sort(replacements.begin(), replacements.end(), [](const replacement& left, const replacement& right) {
        if (left.start != right.start) return left.start > right.start;
        return left.request_index > right.request_index;
    });
    for (size_t index = 1; index < replacements.size(); ++index) {
        if (replacements[index - 1].start < replacements[index].end) {
            throw pfc::exception("Overlapping Cue patch requests");
        }
    }

    cue_patch_result output;
    output.text = original;
    for (const auto& item : replacements) {
        if (output.text.compare(item.start, item.end - item.start, item.old_text) != 0) {
            throw pfc::exception("Cue source value changed while preparing patch");
        }
        output.text.replace(item.start, item.end - item.start, item.new_text);
        ++output.replacement_count;
    }
    return output;
}

cue_patch_result patch_cue_track_numbers(
    const std::string& original,
    const std::vector<cue_track_number_edit>& edits) {
    std::vector<located_track_number> tracks = scan_track_numbers(original);
    std::map<unsigned, unsigned> requested;
    for (const auto& edit : edits) {
        if (edit.before < 1 || edit.before > 999 || edit.after < 1 || edit.after > 999) {
            throw pfc::exception("Cue TRACK renumber request is outside 1..999");
        }
        if (!requested.emplace(edit.before, edit.after).second) {
            throw pfc::exception("Duplicate Cue TRACK renumber request");
        }
    }
    for (const auto& request : requested) {
        if (std::none_of(tracks.begin(), tracks.end(), [&request](const located_track_number& item) {
                return item.value == request.first;
            })) {
            throw pfc::exception("Cue TRACK renumber source is missing");
        }
    }
    std::vector<unsigned> final_numbers;
    final_numbers.reserve(tracks.size());
    for (const auto& track : tracks) {
        const auto found = requested.find(track.value);
        final_numbers.push_back(found == requested.end() ? track.value : found->second);
    }
    for (size_t index = 1; index < final_numbers.size(); ++index) {
        if (final_numbers[index - 1] >= final_numbers[index]) {
            throw pfc::exception(
                "Renumbered Cue TRACK values would conflict or reorder track identity");
        }
    }

    cue_patch_result output;
    output.text = original;
    for (size_t index = tracks.size(); index-- > 0;) {
        const auto found = requested.find(tracks[index].value);
        if (found == requested.end() || found->second == tracks[index].value) continue;
        const size_t width = tracks[index].end - tracks[index].start;
        output.text.replace(
            tracks[index].start,
            width,
            format_track_number(found->second, width));
        ++output.replacement_count;
    }
    return output;
}

cue_timing_document parse_cue_timing(const std::string& original) {
    return make_timing_document(scan_timing(original));
}

cue_patch_result patch_cue_timing(
    const std::string& original,
    const std::vector<cue_timing_edit>& edits) {
    struct replacement {
        size_t start = 0;
        size_t end = 0;
        std::string old_text;
        std::string new_text;
        size_t request_index = 0;
    };

    const timing_scan scan = scan_timing(original);
    (void)make_timing_document(scan);
    std::vector<replacement> replacements;
    std::set<std::tuple<unsigned, cue_timing_directive, unsigned>> unique;

    for (size_t request_index = 0; request_index < edits.size(); ++request_index) {
        const cue_timing_edit& edit = edits[request_index];
        const unsigned index_number = edit.directive == cue_timing_directive::pregap ? 0 :
            edit.index_number;
        if (edit.track_number < 1 || edit.track_number > 999) {
            throw pfc::exception("Cue timing edit has an invalid TRACK number");
        }
        if (edit.directive == cue_timing_directive::index && index_number > 99) {
            throw pfc::exception("INDEX 100+ is preserve-only");
        }
        if (index_number == 1 && edit.kind != cue_timing_edit_kind::replace) {
            throw pfc::exception("INDEX 01 is mandatory and replace-only");
        }
        if (!unique.emplace(edit.track_number, edit.directive, index_number).second) {
            throw pfc::exception("Duplicate Cue timing edit");
        }
        const auto track = std::find_if(
            scan.tracks.begin(), scan.tracks.end(), [&edit](const located_timing_track& item) {
                return item.track_number == edit.track_number;
            });
        if (track == scan.tracks.end()) throw pfc::exception("Cue timing TRACK is missing");
        const located_timing_value* existing = find_timing_value(
            scan, edit.track_number, edit.directive, index_number);

        if (edit.kind == cue_timing_edit_kind::insert) {
            if (existing != nullptr) throw pfc::exception("Cue timing insertion target already exists");
            const located_timing_value* index01 = find_timing_value(
                scan, edit.track_number, cue_timing_directive::index, 1);
            if (index01 == nullptr) throw pfc::exception("Cue timing insertion has no INDEX 01 anchor");
            size_t position = index01->line_start;
            std::string indentation = index01->indentation;
            std::string ending = index01->line_ending;
            bool append_after_anchor = false;
            if (edit.directive == cue_timing_directive::index && index_number >= 2) {
                const located_timing_value* anchor = index01;
                for (const auto& candidate : scan.values) {
                    if (candidate.track_number == edit.track_number &&
                        candidate.directive == cue_timing_directive::index &&
                        candidate.index_number < index_number &&
                        candidate.index_number > anchor->index_number) {
                        anchor = &candidate;
                    }
                }
                position = anchor->line_after_end;
                indentation = anchor->indentation;
                ending = anchor->line_ending;
                append_after_anchor = anchor->line_after_end == original.size() &&
                    !original.empty() && original.back() != '\r' && original.back() != '\n';
            }
            if (ending.empty()) ending = line_ending_at(original, original.size());
            pfc::string_formatter inserted;
            if (append_after_anchor) inserted << ending.c_str();
            inserted << indentation.c_str();
            if (edit.directive == cue_timing_directive::pregap) {
                inserted << "PREGAP " << format_cue_time_75(edit.after_ticks).c_str();
            } else {
                inserted << "INDEX " << pfc::format_uint(index_number, 2) << " "
                         << format_cue_time_75(edit.after_ticks).c_str();
            }
            if (!append_after_anchor) inserted << ending.c_str();
            replacements.push_back({ position, position, {}, inserted.c_str(), request_index });
            continue;
        }

        if (existing == nullptr) throw pfc::exception("Cue timing edit target is missing");
        if (existing->ticks != edit.before_ticks) {
            throw pfc::exception("Cue timing preimage does not match the raw directive");
        }
        if (edit.kind == cue_timing_edit_kind::remove) {
            replacements.push_back({
                existing->line_start,
                existing->line_after_end,
                original.substr(
                    existing->line_start,
                    existing->line_after_end - existing->line_start),
                {},
                request_index,
            });
        } else {
            replacements.push_back({
                existing->time_start,
                existing->time_end,
                original.substr(existing->time_start, existing->time_end - existing->time_start),
                format_cue_time_75(edit.after_ticks),
                request_index,
            });
        }
    }

    std::sort(replacements.begin(), replacements.end(), [](const replacement& left, const replacement& right) {
        if (left.start != right.start) return left.start > right.start;
        return left.request_index > right.request_index;
    });
    for (size_t index = 1; index < replacements.size(); ++index) {
        if (replacements[index - 1].start < replacements[index].end) {
            throw pfc::exception("Overlapping Cue timing edits");
        }
    }

    cue_patch_result output;
    output.text = original;
    for (const auto& item : replacements) {
        if (output.text.compare(item.start, item.end - item.start, item.old_text) != 0) {
            throw pfc::exception("Cue timing source changed while preparing patch");
        }
        output.text.replace(item.start, item.end - item.start, item.new_text);
        ++output.replacement_count;
    }

    const timing_scan after_scan = scan_timing(output.text);
    (void)make_timing_document(after_scan);
    for (const cue_timing_edit& edit : edits) {
        const unsigned index_number = edit.directive == cue_timing_directive::pregap ? 0 :
            edit.index_number;
        const located_timing_value* after = find_timing_value(
            after_scan, edit.track_number, edit.directive, index_number);
        if (edit.kind == cue_timing_edit_kind::remove) {
            if (after != nullptr) throw pfc::exception("Removed Cue timing directive remains present");
        } else if (after == nullptr || after->ticks != edit.after_ticks) {
            throw pfc::exception("Cue timing postimage does not match the requested value");
        }
    }
    return output;
}

size_t cue_field_occurrence_count(
    const std::string& original,
    unsigned track_number,
    const std::string& command) {
    const field_scan scan = scan_field(original, track_number, command);
    if (track_number != 0 && scan.track_count != 1) {
        pfc::string_formatter message;
        message << "Cue TRACK " << track_number << " occurs " << scan.track_count << " times";
        throw pfc::exception(message);
    }
    return scan.candidates.size();
}

std::string cue_field_value(
    const std::string& original,
    unsigned track_number,
    const std::string& command) {
    return locate_track_field(original, track_number, normalize_command(command)).old_value;
}

} // namespace metadata_isolation
