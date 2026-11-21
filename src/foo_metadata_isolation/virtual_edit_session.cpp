#include "stdafx.h"
#include "cue_patch.h"
#include "metadata_diff.h"
#include "operation_log.h"
#include "preferences.h"
#include "raw_cuesheet_patch.h"
#include "resource.h"
#include "ui_tooltips.h"
#include "virtual_edit_session.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace metadata_isolation {
namespace {

constexpr double k_open_timeout_seconds = 2.0;

using runtime_clock = std::chrono::steady_clock;

uint64_t runtime_elapsed_us(runtime_clock::time_point begin) {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        runtime_clock::now() - begin).count());
}

struct runtime_timing_metrics {
    uint64_t prepare_total_us = 0;
    uint64_t source_analysis_us = 0;
    uint64_t source_hash_us = 0;
    uint64_t virtual_inventory_us = 0;
    uint64_t storage_inspection_us = 0;
    uint64_t shadow_copy_us = 0;
    uint64_t shadow_projection_write_us = 0;
    uint64_t shadow_baseline_read_us = 0;
    uint64_t command_us = 0;
    uint64_t diff_classification_us = 0;
    uint64_t planning_us = 0;
    uint64_t accepted_postimage_us = 0;
    uint64_t stale_check_us = 0;
    uint64_t backup_us = 0;
    uint64_t staging_us = 0;
    uint64_t install_us = 0;
    uint64_t postvalidation_us = 0;
    uint64_t rollback_us = 0;
    uint64_t commit_total_us = 0;
    uint64_t cleanup_us = 0;
};

#if defined(FB2K_ENABLE_TEST_PROBES)
using benchmark_clock = std::chrono::steady_clock;

uint64_t elapsed_microseconds(benchmark_clock::time_point begin) {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        benchmark_clock::now() - begin).count());
}

struct projection_benchmark_metrics {
    uint64_t prepare_total_us = 0;
    uint64_t source_snapshot_us = 0;
    uint64_t source_hash_us = 0;
    uint64_t virtual_inventory_us = 0;
    uint64_t storage_inspection_us = 0;
    uint64_t shadow_copy_us = 0;
    uint64_t shadow_projection_write_us = 0;
    uint64_t shadow_baseline_read_us = 0;
    uint64_t shadow_logical_bytes = 0;
    uint64_t shadow_allocated_bytes = 0;
    uint64_t analyze_diff_us = 0;
    uint64_t analyze_planning_us = 0;
    uint64_t analyze_postimage_us = 0;
    uint64_t commit_lock_stale_us = 0;
    uint64_t commit_backup_us = 0;
    uint64_t commit_staging_us = 0;
    uint64_t commit_install_us = 0;
    uint64_t commit_postvalidation_us = 0;
    uint64_t commit_total_us = 0;
};
#endif

struct source_document {
    std::string path;
    std::string native_path;
    t_filestats stats;
    hasher_md5_result fingerprint = hasher_md5_result::null();
    file_info_impl physical_before;
    std::vector<unsigned> virtual_subsongs;
    std::map<unsigned, file_info_impl> virtual_before;
    std::map<unsigned, file_info_impl> virtual_expected_after;
    std::set<unsigned> selected_subsongs;
    std::string cue_before;
    std::string cue_after;
    std::string shadow_path;
    std::string backup_path;
    std::string backup_native_path;
    std::map<unsigned, unsigned> track_number_after;
    raw_cuesheet_storage_kind storage = raw_cuesheet_storage_kind::id3v2;
    bool changed = false;
    bool rechaptered = false;
};

std::mutex g_virtual_shadow_registry_mutex;
struct registered_virtual_shadow {
    std::map<unsigned, file_info_impl> captured_postimages;
};
std::map<std::string, registered_virtual_shadow> g_virtual_shadow_registry;

void register_virtual_shadow_path(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_virtual_shadow_registry_mutex);
    // Session directories are unique. Replacing, rather than retaining, a
    // theoretically stale same-path entry also guarantees that no captured
    // postimage can leak into a newly prepared session.
    g_virtual_shadow_registry.insert_or_assign(path, registered_virtual_shadow{});
}

void unregister_virtual_shadow_path(const std::string& path) noexcept {
    std::lock_guard<std::mutex> lock(g_virtual_shadow_registry_mutex);
    g_virtual_shadow_registry.erase(path);
}

bool captured_virtual_shadow_postimage(
    const char* path,
    unsigned subsong,
    file_info_impl& output) {
    std::lock_guard<std::mutex> lock(g_virtual_shadow_registry_mutex);
    for (const auto& registered : g_virtual_shadow_registry) {
        if (metadb::path_compare(registered.first.c_str(), path) != 0) continue;
        const auto found = registered.second.captured_postimages.find(subsong);
        if (found == registered.second.captured_postimages.end()) return false;
        output = found->second;
        return true;
    }
    return false;
}

struct projected_shadow {
    size_t document_index = 0;
    unsigned track_number = 0;
    std::string path;
    file_info_impl before;
    file_info_impl after;
    metadata_diff_result diff;
};

bool same_path(const char* left, const char* right) {
    int comparison = 0;
    if (filesystem::g_compare_paths(left, right, comparison)) return comparison == 0;
    return metadb::path_compare(left, right) == 0;
}

bool qualified_timing_path(const char* path) {
    const char* extension = filesystem::g_get_extension(path);
    return pfc::stricmp_ascii(extension, "mp3") == 0 ||
        pfc::stricmp_ascii(extension, "flac") == 0 ||
        pfc::stricmp_ascii(extension, "wv") == 0;
}

file_info_impl read_info(const char* path, unsigned subsong, abort_callback& abort) {
    input_info_reader::ptr reader;
    input_entry::g_open_for_info_read(reader, nullptr, path, abort);
    file_info_impl output;
    reader->get_info(subsong, output, abort);
    return output;
}

struct file_snapshot {
    file_info_impl info;
    t_filestats stats;
    std::vector<unsigned> virtual_subsongs;
};

file_snapshot read_physical_snapshot(
    const char* path,
    abort_callback& abort,
    input_info_reader::ptr* retained_reader = nullptr) {
    input_info_reader::ptr reader;
    input_entry::g_open_for_info_read(reader, nullptr, path, abort);
    file_snapshot output;
    reader->get_info(0, output.info, abort);
    output.stats = reader->get_file_stats(abort);
    for (unsigned index = 0; index < reader->get_subsong_count(); ++index) {
        const unsigned subsong = reader->get_subsong(index);
        if (subsong != 0) output.virtual_subsongs.push_back(subsong);
    }
    if (retained_reader != nullptr) *retained_reader = reader;
    return output;
}

hasher_md5_result fingerprint_file(const char* path, abort_callback& abort) {
    file::ptr input;
    filesystem::g_open_read(input, path, abort);
    hasher_md5::ptr hasher = hasher_md5::get();
    hasher_md5_state state;
    hasher->initialize(state);
    std::vector<unsigned char> buffer(1024 * 1024);
    for (;;) {
        const size_t count = input->read(buffer.data(), buffer.size(), abort);
        if (count == 0) break;
        hasher->process(state, buffer.data(), count);
    }
    return hasher->get_result(state);
}

std::string make_session_directory() {
    wchar_t temp_path[MAX_PATH + 1] = {};
    const DWORD length = GetTempPathW(MAX_PATH, temp_path);
    if (length == 0 || length > MAX_PATH) throw pfc::exception("Windows temporary path is unavailable");

    static std::atomic<unsigned long> sequence = 0;
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        wchar_t directory[MAX_PATH + 1] = {};
        _snwprintf_s(
            directory,
            _countof(directory),
            _TRUNCATE,
            L"%sfoo_metadata_isolation-%lu-%llu-%lu",
            temp_path,
            static_cast<unsigned long>(GetCurrentProcessId()),
            static_cast<unsigned long long>(GetTickCount64()),
            ++sequence);
        if (CreateDirectoryW(directory, nullptr)) {
            pfc::stringcvt::string_utf8_from_wide utf8(directory);
            pfc::string8 canonical;
            filesystem::g_get_canonical_path(utf8, canonical);
            return canonical.c_str();
        }
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            throw exception_win32(GetLastError());
        }
    }
    throw pfc::exception("Could not allocate a unique Metadata Isolation temporary directory");
}

std::string make_session_file_path(const std::string& directory, const char* filename) {
    pfc::string8 native_directory;
    if (!filesystem::g_get_native_path(directory.c_str(), native_directory)) {
        throw pfc::exception("Temporary session directory has no native path");
    }
    pfc::string_formatter native_path;
    native_path << native_directory << "\\" << filename;
    pfc::string8 canonical;
    filesystem::g_get_canonical_path(native_path, canonical);
    return canonical.c_str();
}

#if defined(FB2K_ENABLE_TEST_PROBES)
struct native_storage_usage {
    uint64_t logical = 0;
    uint64_t allocated = 0;
};

native_storage_usage query_native_storage_usage(const char* canonical) {
    pfc::string8 native;
    if (!filesystem::g_get_native_path(canonical, native)) {
        throw pfc::exception("Benchmark file has no native path");
    }
    pfc::stringcvt::string_wide_from_utf8 path(native);
    WIN32_FILE_ATTRIBUTE_DATA attributes = {};
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attributes)) {
        throw exception_win32(GetLastError());
    }
    native_storage_usage output;
    output.logical = (static_cast<uint64_t>(attributes.nFileSizeHigh) << 32) |
        attributes.nFileSizeLow;
    SetLastError(NO_ERROR);
    DWORD allocation_high = 0;
    const DWORD allocation_low = GetCompressedFileSizeW(path, &allocation_high);
    const DWORD allocation_error = GetLastError();
    if (allocation_low == INVALID_FILE_SIZE && allocation_error != NO_ERROR) {
        throw exception_win32(allocation_error);
    }
    output.allocated = (static_cast<uint64_t>(allocation_high) << 32) | allocation_low;
    return output;
}
#endif

void copy_file_overwrite_native(const std::string& from, const std::string& to) {
    pfc::stringcvt::string_wide_from_utf8 from_wide(from.c_str());
    pfc::stringcvt::string_wide_from_utf8 to_wide(to.c_str());
    if (!CopyFileW(from_wide, to_wide, FALSE)) {
        throw exception_win32(GetLastError());
    }
}

void append_value_list(
    pfc::string_base& output,
    const char* label,
    const std::vector<std::string>& values) {
    output << "  " << label << ":\n";
    if (values.empty()) {
        output << "    <missing>\n";
        return;
    }
    for (const auto& value : values) {
        output << "    - " << (value.empty() ? "<empty>" : value.c_str()) << "\n";
    }
}

void append_replaygain_value(
    pfc::string_base& output,
    const char* label,
    replaygain_component component,
    const replaygain_value& value) {
    output << "  " << label << ":\n    ";
    if (!value.present) {
        output << "<missing>\n";
        return;
    }
    replaygain_info::t_text_buffer formatted = {};
    const bool gain = component == replaygain_component::track_gain ||
        component == replaygain_component::album_gain;
    const bool success = gain ? replaygain_info::g_format_gain(value.value, formatted) :
        replaygain_info::g_format_peak(value.value, formatted);
    output << (success ? formatted : "<invalid>") << "\n";
}

std::string format_replaygain_value_exact(
    replaygain_component component,
    const replaygain_value& value) {
    if (!value.present) return {};
    if (!std::isfinite(value.value)) {
        throw pfc::exception("ReplayGain value is not finite");
    }
    const bool gain = component == replaygain_component::track_gain ||
        component == replaygain_component::album_gain;
    if (!gain && value.value < 0) {
        throw pfc::exception("ReplayGain peak is negative");
    }
    replaygain_info::t_text_buffer formatted = {};
    if (!(gain ? replaygain_info::g_format_gain(value.value, formatted) :
          replaygain_info::g_format_peak(value.value, formatted))) {
        throw pfc::exception("ReplayGain value cannot be formatted");
    }
    replaygain_info parsed;
    switch (component) {
    case replaygain_component::track_gain: parsed.set_track_gain_text(formatted); break;
    case replaygain_component::track_peak: parsed.set_track_peak_text(formatted); break;
    case replaygain_component::album_gain: parsed.set_album_gain_text(formatted); break;
    case replaygain_component::album_peak: parsed.set_album_peak_text(formatted); break;
    }
    if (replaygain_component_value(parsed, component) != value) {
        throw pfc::exception(
            "ReplayGain value loses precision in the native Cue text representation");
    }
    return formatted;
}

std::vector<std::string> metadata_values(const file_info& info, const std::string& name) {
    std::vector<std::string> output;
    const size_t field = info.meta_find(name.c_str());
    if (field == SIZE_MAX) return output;
    for (size_t index = 0; index < info.meta_enum_value_count(field); ++index) {
        output.emplace_back(info.meta_enum_value(field, index));
    }
    return output;
}

void replace_metadata_values(
    file_info_impl& info,
    const std::string& name,
    const std::vector<std::string>& values) {
    info.meta_remove_field(name.c_str());
    if (values.empty()) return;
    const size_t field = info.meta_set(name.c_str(), values[0].c_str());
    for (size_t index = 1; index < values.size(); ++index) {
        info.meta_add_value(field, values[index].c_str());
    }
}

unsigned parse_canonical_track_number(const std::string& value) {
    if (value.empty() || value.size() > 3 ||
        std::any_of(value.begin(), value.end(), [](unsigned char character) {
            return character < '0' || character > '9';
        })) {
        throw pfc::exception("TRACKNUMBER must be canonical decimal 1..999");
    }
    if (value.size() > 1 && value[0] == '0') {
        throw pfc::exception("TRACKNUMBER with leading zeroes cannot roundtrip exactly");
    }
    const unsigned parsed = static_cast<unsigned>(std::stoul(value));
    if (parsed < 1 || parsed > 999 || std::to_string(parsed) != value) {
        throw pfc::exception("TRACKNUMBER must be canonical decimal 1..999");
    }
    return parsed;
}

const cue_track_timing& timing_track(
    const cue_timing_document& document,
    unsigned track_number) {
    const auto found = std::find_if(
        document.tracks.begin(), document.tracks.end(), [track_number](const cue_track_timing& item) {
            return item.track_number == track_number;
        });
    if (found == document.tracks.end()) throw pfc::exception("Cue timing TRACK is missing");
    return *found;
}

const cue_timing_value* timing_value(
    const cue_track_timing& track,
    cue_timing_directive directive,
    unsigned index_number = 0) {
    const cue_timing_value* found = nullptr;
    for (const auto& value : track.values) {
        if (value.directive != directive ||
            (directive == cue_timing_directive::index && value.index_number != index_number)) {
            continue;
        }
        if (found != nullptr) throw pfc::exception("Duplicate Cue timing value");
        found = &value;
    }
    return found;
}

file_info_impl without_timing_info(const file_info& source) {
    file_info_impl output(source);
    output.info_remove("referenced_offset");
    output.info_remove("pregap");
    for (unsigned index = 0; index < 100; ++index) {
        pfc::string_formatter name;
        name << "index " << pfc::format_uint(index, 2);
        output.info_remove(name.c_str());
    }
    return output;
}

void validate_exposed_timing(
    const cue_track_timing& planned,
    const file_info& actual) {
    const cue_timing_value* index01 = timing_value(
        planned, cue_timing_directive::index, 1);
    if (index01 == nullptr) throw pfc::exception("Planned timing has no INDEX 01");
    const char* referenced = actual.info_get("referenced_offset");
    if (index01->ticks == 0) {
        if (referenced != nullptr) throw pfc::exception("Unexpected zero referenced_offset");
    } else if (referenced == nullptr || format_cue_time_75(index01->ticks) != referenced) {
        throw pfc::exception("INDEX 01 did not roundtrip as referenced_offset");
    }

    unsigned pregap_ticks = 0;
    if (const cue_timing_value* index00 = timing_value(
            planned, cue_timing_directive::index, 0)) {
        pregap_ticks = index01->ticks - index00->ticks;
    } else if (const cue_timing_value* pregap = timing_value(
                   planned, cue_timing_directive::pregap)) {
        pregap_ticks = pregap->ticks;
    }
    const char* pregap = actual.info_get("pregap");
    if (pregap_ticks == 0) {
        if (pregap != nullptr) throw pfc::exception("Unexpected zero pregap technical state");
    } else if (pregap == nullptr || format_cue_time_75(pregap_ticks) != pregap) {
        throw pfc::exception("INDEX 00/PREGAP did not roundtrip as pregap state");
    }

    for (unsigned index = 2; index < 100; ++index) {
        const cue_timing_value* planned_index = timing_value(
            planned, cue_timing_directive::index, index);
        pfc::string_formatter name;
        name << "index " << pfc::format_uint(index, 2);
        const char* value = actual.info_get(name.c_str());
        if (planned_index == nullptr) {
            if (value != nullptr) throw pfc::exception("Unexpected additional INDEX technical state");
        } else {
            const unsigned relative = planned_index->ticks - index01->ticks;
            if (value == nullptr || format_cue_time_75(relative) != value) {
                throw pfc::exception("Additional INDEX did not roundtrip exactly");
            }
        }
    }
}

class virtual_edit_session {
public:
    explicit virtual_edit_session(
        metadb_handle_list_cref selection,
        const char* operation = "projected_virtual")
        : m_selection(selection),
          m_log(operation, "virtual"),
          m_apply_supported_changes(cfg_apply_supported_changes) {
        m_log.standard(
            "session.start",
            {log_count("selected_tracks", m_selection.get_count())});
    }

    ~virtual_edit_session() { cleanup_noexcept(); }

    operation_logger& logger() { return m_log; }
    const operation_logger& logger() const { return m_log; }

    void record_command_duration(uint64_t duration_us, const char* result) {
        m_timing.command_us += duration_us;
        m_log.verbose(
            "command.complete",
            {log_token("result", result), log_duration("duration_us", duration_us)});
    }

    void finish_logging(const char* outcome, const char* reason = nullptr) {
        m_log.verbose(
            "timing.summary",
            {
                log_duration("source_analysis_us", m_timing.source_analysis_us),
                log_duration("source_hash_us", m_timing.source_hash_us),
                log_duration("virtual_inventory_us", m_timing.virtual_inventory_us),
                log_duration("storage_inspection_us", m_timing.storage_inspection_us),
                log_duration("projection_copy_us", m_timing.shadow_copy_us),
                log_duration("projection_write_us", m_timing.shadow_projection_write_us),
                log_duration("projection_read_us", m_timing.shadow_baseline_read_us),
                log_duration("projection_total_us", m_timing.prepare_total_us),
                log_duration("command_us", m_timing.command_us),
                log_duration("diff_classification_us", m_timing.diff_classification_us),
                log_duration("planning_us", m_timing.planning_us),
                log_duration("accepted_postimage_us", m_timing.accepted_postimage_us),
                log_duration("stale_check_us", m_timing.stale_check_us),
                log_duration("backup_us", m_timing.backup_us),
                log_duration("staging_us", m_timing.staging_us),
                log_duration("install_us", m_timing.install_us),
                log_duration("postvalidation_us", m_timing.postvalidation_us),
                log_duration("rollback_us", m_timing.rollback_us),
                log_duration("commit_total_us", m_timing.commit_total_us),
                log_duration("cleanup_us", m_timing.cleanup_us),
            });
        m_log.finish(
            outcome,
            reason,
            {
                log_count("selected_tracks", m_shadows.empty() ? m_selection.get_count() : m_shadows.size()),
                log_count("source_documents", m_documents.size()),
                log_count("accepted", m_accepted_change_count),
                log_count("skipped", m_skipped_change_count),
                log_count("blocking", m_blocking_change_count),
            });
    }

    void cleanup_for_completion() noexcept { cleanup_noexcept(); }

#if defined(FB2K_ENABLE_TEST_PROBES)
    void diagnostic_fail_after_first_install(bool value) {
        m_diagnostic_fail_after_first_install = value;
    }

    void diagnostic_apply_supported_changes(bool value) {
        m_apply_supported_changes = value;
    }

    void append_benchmark_metrics(pfc::string_base& output) const {
        uint64_t source_logical_bytes = 0;
        uint64_t source_allocated_bytes = 0;
        for (const auto& document : m_documents) {
            const native_storage_usage usage = query_native_storage_usage(document.path.c_str());
            source_logical_bytes += usage.logical;
            source_allocated_bytes += usage.allocated;
        }
        output << "benchmark_source_document_count=" << m_documents.size() << "\n"
               << "benchmark_virtual_inventory_count="
               << (m_documents.empty() ? 0 : m_documents[0].virtual_subsongs.size()) << "\n"
               << "benchmark_shadow_count=" << m_shadows.size() << "\n"
               << "benchmark_shadow_backing_file_count=" << m_documents.size() << "\n"
               << "benchmark_source_logical_bytes=" << source_logical_bytes << "\n"
               << "benchmark_source_allocated_bytes=" << source_allocated_bytes << "\n"
               << "benchmark_shadow_logical_bytes=" << m_benchmark.shadow_logical_bytes << "\n"
               << "benchmark_shadow_allocated_bytes=" << m_benchmark.shadow_allocated_bytes << "\n"
               << "benchmark_prepare_total_us=" << m_benchmark.prepare_total_us << "\n"
               << "benchmark_source_snapshot_cue_parse_us=" << m_benchmark.source_snapshot_us << "\n"
               << "benchmark_source_hash_us=" << m_benchmark.source_hash_us << "\n"
               << "benchmark_virtual_inventory_load_us=" << m_benchmark.virtual_inventory_us << "\n"
               << "benchmark_storage_inspection_us=" << m_benchmark.storage_inspection_us << "\n"
               << "benchmark_shadow_copy_us=" << m_benchmark.shadow_copy_us << "\n"
               << "benchmark_shadow_projection_write_us="
               << m_benchmark.shadow_projection_write_us << "\n"
               << "benchmark_shadow_baseline_read_us="
               << m_benchmark.shadow_baseline_read_us << "\n"
               << "benchmark_diff_us=" << m_benchmark.analyze_diff_us << "\n"
               << "benchmark_capability_planning_us=" << m_benchmark.analyze_planning_us << "\n"
               << "benchmark_review_staging_postimage_us="
               << m_benchmark.analyze_postimage_us << "\n"
               << "benchmark_commit_lock_stale_us=" << m_benchmark.commit_lock_stale_us << "\n"
               << "benchmark_commit_backup_us=" << m_benchmark.commit_backup_us << "\n"
               << "benchmark_commit_staging_us=" << m_benchmark.commit_staging_us << "\n"
               << "benchmark_commit_install_us=" << m_benchmark.commit_install_us << "\n"
               << "benchmark_commit_postvalidation_us="
               << m_benchmark.commit_postvalidation_us << "\n"
               << "benchmark_commit_total_us=" << m_benchmark.commit_total_us << "\n";
    }
#endif

    void prepare(threaded_process_status& status, abort_callback& abort) {
        const auto runtime_prepare_begin = runtime_clock::now();
#if defined(FB2K_ENABLE_TEST_PROBES)
        const auto prepare_begin = benchmark_clock::now();
#endif
        std::string stage = "validating selection";
        try {
            if (m_selection.get_count() == 0) throw pfc::exception("Selection is empty");
            stage = "creating temporary session directory";
            m_directory = make_session_directory();
            status.set_progress(0, m_selection.get_count());

            size_t output_index = 0;
            for (const auto& selected : m_selection) {
                abort.check();
                status.set_item(selected->get_path());
                status.set_progress(output_index, m_selection.get_count());

                if (selected->get_subsong_index() == 0) {
                    throw pfc::exception("Safe virtual routing requires embedded-Cue subsongs, not subsong 0");
                }
                const char* selected_extension = filesystem::g_get_extension(selected->get_path());
                if (pfc::stricmp_ascii(selected_extension, "mp3") != 0 &&
                    pfc::stricmp_ascii(selected_extension, "flac") != 0 &&
                    pfc::stricmp_ascii(selected_extension, "wv") != 0) {
                    throw pfc::exception(
                        "Safe virtual routing is release-scoped to qualified local MP3, FLAC and WavPack files");
                }
                stage = "canonicalizing source path";
                pfc::string8 canonical;
                filesystem::g_get_canonical_path(selected->get_path(), canonical);
                if (filesystem::g_is_remote(canonical)) {
                    throw pfc::exception("Remote sources cannot be projected safely");
                }

                bool duplicate = false;
                for (const auto& existing : m_shadows) {
                    if (existing.track_number == selected->get_subsong_index() &&
                        same_path(m_documents[existing.document_index].path.c_str(), canonical)) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) continue;

                stage = "snapshotting original source";
                size_t document_index = find_or_add_document(canonical, abort);
                m_documents[document_index].selected_subsongs.insert(
                    selected->get_subsong_index());
                stage = "reading virtual track metadata";
                const auto virtual_before = m_documents[document_index].virtual_before.find(
                    selected->get_subsong_index());
                if (virtual_before == m_documents[document_index].virtual_before.end()) {
                    throw pfc::exception(
                        "Selected virtual track is not present in the current Cue document");
                }
                file_info_impl virtual_info(virtual_before->second);
                source_document& document = m_documents[document_index];

                if (document.shadow_path.empty()) {
                    pfc::string_formatter shadow_name;
                    shadow_name << "source-" << (document_index + 1) << "."
                                << raw_cuesheet_storage_extension(document.storage);
                    document.shadow_path = make_session_file_path(
                        m_directory, shadow_name.c_str());
                    stage = "copying original to shared document shadow";
                    const auto runtime_phase_begin = runtime_clock::now();
#if defined(FB2K_ENABLE_TEST_PROBES)
                    const auto benchmark_begin = benchmark_clock::now();
#endif
                    filesystem::g_copy(canonical, document.shadow_path.c_str(), abort);
                    register_virtual_shadow_path(document.shadow_path);
                    m_timing.shadow_copy_us += runtime_elapsed_us(runtime_phase_begin);
#if defined(FB2K_ENABLE_TEST_PROBES)
                    m_benchmark.shadow_copy_us += elapsed_microseconds(benchmark_begin);
                    const native_storage_usage usage = query_native_storage_usage(
                        document.shadow_path.c_str());
                    m_benchmark.shadow_logical_bytes += usage.logical;
                    m_benchmark.shadow_allocated_bytes += usage.allocated;
#endif
                }

                projected_shadow shadow;
                shadow.document_index = document_index;
                shadow.track_number = selected->get_subsong_index();
                shadow.path = document.shadow_path;
                shadow.before = std::move(virtual_info);
                m_shadows.push_back(std::move(shadow));
                ++output_index;
            }
            if (m_shadows.empty()) throw pfc::exception("Selection produced no unique virtual tracks");
            status.set_progress(m_selection.get_count(), m_selection.get_count());
            m_prepared = true;
            m_timing.prepare_total_us = runtime_elapsed_us(runtime_prepare_begin);
            m_log.standard(
                "projection.complete",
                {
                    log_count("selected_tracks", m_shadows.size()),
                    log_count("source_documents", m_documents.size()),
                    log_count("backing_files", m_documents.size()),
                    log_token("strategy", "SHARED_DOCUMENT_SHADOW"),
                });
            m_log.verbose(
                "mapping.complete",
                {
                    log_count("selected_handles", m_selection.get_count()),
                    log_count("deduplicated_virtual_tracks", m_shadows.size()),
                    log_count("source_documents", m_documents.size()),
                    log_count("backing_files", m_documents.size()),
                    log_duration("duration_us", m_timing.prepare_total_us),
                });
#if defined(FB2K_ENABLE_TEST_PROBES)
            m_benchmark.prepare_total_us = elapsed_microseconds(prepare_begin);
#endif
        } catch (const exception_aborted&) {
            m_timing.prepare_total_us = runtime_elapsed_us(runtime_prepare_begin);
            m_log.standard(
                "projection.cancelled",
                {log_token("reason", "COMMAND_CANCELLED")});
#if defined(FB2K_ENABLE_TEST_PROBES)
            m_benchmark.prepare_total_us = elapsed_microseconds(prepare_begin);
#endif
            throw;
        } catch (const std::exception& error) {
            m_error = stage + ": " + error.what();
            m_timing.prepare_total_us = runtime_elapsed_us(runtime_prepare_begin);
            note_blocking("PREPARATION_FAILED");
            m_log.standard(
                "projection.failed",
                {log_token("reason", "PREPARATION_FAILED"), log_token("phase", stage.c_str())});
#if defined(FB2K_ENABLE_TEST_PROBES)
            m_benchmark.prepare_total_us = elapsed_microseconds(prepare_begin);
#endif
        }
    }

    void prepare_timing(
        const std::vector<virtual_timing_edit_request>& requests,
        threaded_process_status& status,
        abort_callback& abort) {
        const auto runtime_prepare_begin = runtime_clock::now();
        std::string stage = "validating structural timing request";
        try {
            if (requests.empty()) throw pfc::exception("Timing request is empty");
            m_directory = make_session_directory();
            std::vector<std::vector<cue_timing_edit>> edits;
            status.set_progress(0, requests.size());
            for (size_t request_index = 0; request_index < requests.size(); ++request_index) {
                abort.check();
                const auto& request = requests[request_index];
                if (!request.target.is_valid() || request.target->get_subsong_index() == 0 ||
                    request.edit.track_number != request.target->get_subsong_index()) {
                    throw pfc::exception("Timing request does not match a virtual target handle");
                }
                if (!qualified_timing_path(request.target->get_path())) {
                    throw pfc::exception(
                        "Structural timing is release-scoped to qualified local MP3/FLAC/WavPack files");
                }
                pfc::string8 canonical;
                filesystem::g_get_canonical_path(request.target->get_path(), canonical);
                if (filesystem::g_is_remote(canonical)) {
                    throw pfc::exception("Remote structural timing targets are unsupported");
                }
                status.set_item(canonical);
                const size_t document_index = find_or_add_document(canonical, abort);
                if (edits.size() < m_documents.size()) edits.resize(m_documents.size());
                if (std::find(
                        m_documents[document_index].virtual_subsongs.begin(),
                        m_documents[document_index].virtual_subsongs.end(),
                        request.edit.track_number) ==
                    m_documents[document_index].virtual_subsongs.end()) {
                    throw pfc::exception("Timing request refers to a missing virtual subsong");
                }
                m_documents[document_index].selected_subsongs.insert(request.edit.track_number);
                edits[document_index].push_back(request.edit);
                status.set_progress(request_index + 1, requests.size());
            }

            pfc::string_formatter summary;
            summary << "VIRTUAL STRUCTURE: exact 75 Hz Cue timing transaction\n\n";
            for (size_t document_index = 0; document_index < m_documents.size(); ++document_index) {
                source_document& document = m_documents[document_index];
                document.track_number_after.clear();
                document.virtual_expected_after.clear();
                document.rechaptered = false;
                for (unsigned subsong : document.virtual_subsongs) {
                    document.track_number_after.emplace(subsong, subsong);
                }
                if (edits[document_index].empty()) continue;
                stage = "planning exact raw-Cue timing patch";
                const cue_timing_document before_model = parse_cue_timing(document.cue_before);
                const cue_patch_result patched = patch_cue_timing(
                    document.cue_before, edits[document_index]);
                const cue_timing_document after_model = parse_cue_timing(patched.text);
                const double source_length = document.physical_before.get_length();
                if (!(source_length > 0)) {
                    throw pfc::exception("Physical source has no positive audio duration");
                }
                for (const auto& track : after_model.tracks) {
                    for (const auto& value : track.values) {
                        if (value.directive == cue_timing_directive::index &&
                            static_cast<double>(value.ticks) / 75.0 > source_length) {
                            throw pfc::exception("Cue INDEX exceeds the physical audio duration");
                        }
                    }
                }
                document.cue_after = patched.text;
                document.changed = document.cue_after != document.cue_before;
                if (!document.changed) throw pfc::exception("Timing request produces no raw Cue change");

                std::set<unsigned> length_impact;
                std::map<unsigned, double> length_postimage;
                summary << "Source: " << document.path.c_str() << "\n";
                for (const cue_timing_edit& edit : edits[document_index]) {
                    summary << "  TRACK " << edit.track_number << " ";
                    if (edit.directive == cue_timing_directive::pregap) {
                        summary << "PREGAP";
                    } else {
                        summary << "INDEX " << pfc::format_uint(edit.index_number, 2);
                    }
                    summary << "\n    Before: ";
                    if (edit.kind == cue_timing_edit_kind::insert) summary << "<missing>";
                    else summary << format_cue_time_75(edit.before_ticks).c_str();
                    summary << "\n    After: ";
                    if (edit.kind == cue_timing_edit_kind::remove) summary << "<removed>";
                    else summary << format_cue_time_75(edit.after_ticks).c_str();
                    summary << "\n";
                    if (edit.directive == cue_timing_directive::index && edit.index_number == 1) {
                        length_impact.insert(edit.track_number);
                        const auto position = std::find(
                            document.virtual_subsongs.begin(),
                            document.virtual_subsongs.end(),
                            edit.track_number);
                        if (position != document.virtual_subsongs.begin()) {
                            length_impact.insert(*(position - 1));
                        }
                    }
                }
                if (!length_impact.empty()) {
                    summary << "  Affected track durations:";
                    for (unsigned track : length_impact) summary << " " << track;
                    summary << "\n";
                }

                pfc::string_formatter staged_name;
                staged_name << "timing-review-" << (document_index + 1) << "."
                            << raw_cuesheet_storage_extension(document.storage);
                const std::string staged_path = make_session_file_path(
                    m_directory, staged_name.c_str());
                stage = "staging exact raw-Cue timing patch";
                write_raw_cuesheet_patch(
                    document.storage,
                    document.path.c_str(),
                    staged_path.c_str(),
                    document.cue_before.c_str(),
                    document.cue_after.c_str(),
                    abort);
                file_info_impl expected_physical(document.physical_before);
                expected_physical.meta_set("CUESHEET", document.cue_after.c_str());
                input_info_reader::ptr staged_reader;
                const file_snapshot staged = read_physical_snapshot(
                    staged_path.c_str(), abort, &staged_reader);
                if (!file_info::g_is_meta_equal(expected_physical, staged.info) ||
                    expected_physical.get_replaygain() != staged.info.get_replaygain() ||
                    staged.virtual_subsongs != document.virtual_subsongs) {
                    throw pfc::exception("Staged timing patch changed physical or track inventory state");
                }
                for (unsigned subsong : document.virtual_subsongs) {
                    file_info_impl actual;
                    staged_reader->get_info(subsong, actual, abort);
                    const file_info_impl& before = document.virtual_before.at(subsong);
                    if (!file_info::g_is_meta_equal(before, actual) ||
                        before.get_replaygain() != actual.get_replaygain() ||
                        !file_info::g_is_info_equal(
                            without_timing_info(before), without_timing_info(actual))) {
                        throw pfc::exception("Timing patch changed unrelated virtual state");
                    }
                    validate_exposed_timing(timing_track(after_model, subsong), actual);
                    if (length_impact.count(subsong) == 0 &&
                        before.get_length() != actual.get_length()) {
                        throw pfc::exception("Timing patch changed an unrelated track duration");
                    }
                    if (actual.get_length() <= 0) {
                        throw pfc::exception("Timing patch creates a non-positive track duration");
                    }
                    if (length_impact.count(subsong) != 0) {
                        length_postimage.emplace(subsong, actual.get_length());
                    }
                    document.virtual_expected_after.emplace(subsong, actual);
                }
                if (!length_postimage.empty()) {
                    summary << "  Duration postimage:\n";
                    for (const auto& item : length_postimage) {
                        summary << "    TRACK " << item.first << ": "
                                << pfc::format_float(
                                       document.virtual_before.at(item.first).get_length(), 0, 15)
                                << " s -> " << pfc::format_float(item.second, 0, 15) << " s\n";
                    }
                }
                for (unsigned subsong : document.virtual_subsongs) {
                    (void)timing_track(before_model, subsong);
                    (void)timing_track(after_model, subsong);
                }
            }
            summary << "\nPOSTIMAGE: PASS for complete metadata, ReplayGain, timing and affected durations\n"
                    << "RESULT: COMMITTABLE\n\nChoose Commit to install the minimal timing patch.";
            m_has_changes = true;
            m_committable = true;
            m_prepared = true;
            m_timing_prepared = true;
            m_summary = summary.c_str();
            m_timing.prepare_total_us = runtime_elapsed_us(runtime_prepare_begin);
            m_log.standard(
                "structure.prepare",
                {
                    log_token("result", "PASS"),
                    log_count("selected_tracks", m_selection.get_count()),
                    log_count("source_documents", m_documents.size()),
                });
        } catch (const exception_aborted&) {
            m_log.standard(
                "structure.cancelled",
                {log_token("reason", "COMMAND_CANCELLED")});
            throw;
        } catch (const std::exception& error) {
            m_error = stage + ": " + error.what();
            note_blocking("STRUCTURAL_CONFLICT");
            m_log.standard(
                "structure.failed",
                {log_token("reason", "STRUCTURAL_CONFLICT"), log_token("phase", stage.c_str())});
        }
    }

    bool prepared() const { return m_prepared; }
    bool timing_prepared() const { return m_timing_prepared; }
    const std::string& error() const { return m_error; }

    metadb_handle_list shadow_handles() const {
        metadb_handle_list output;
        for (const auto& shadow : m_shadows) {
            output.add_item(metadb::get()->handle_create(
                shadow.path.c_str(), shadow.track_number));
        }
        return output;
    }

    void analyze(threaded_process_status& status, abort_callback& abort) {
        const auto runtime_analyze_begin = runtime_clock::now();
        m_log.verbose(
            "command.state_collect",
            {log_count("projected_tracks", m_shadows.size())});
#if defined(FB2K_ENABLE_TEST_PROBES)
        const auto analyze_begin = benchmark_clock::now();
#endif
        m_error.clear();
        m_summary.clear();
        m_committable = false;
        m_has_changes = false;
        m_accepted_change_count = 0;
        m_skipped_change_count = 0;
        m_skipped_fields.clear();
        m_blocking_change_count = 0;
        m_primary_reason.clear();
        try {
            // The public context-menu API has no generic writer-completion
            // callback.  The user explicitly declares completion in the UX;
            // acquiring foobar write locks here additionally waits out any
            // still-active SDK writer before a Shadow postimage is read.
            std::vector<size_t> shadow_order(m_shadows.size());
            for (size_t index = 0; index < shadow_order.size(); ++index) {
                shadow_order[index] = index;
            }
            std::sort(shadow_order.begin(), shadow_order.end(), [this](size_t left, size_t right) {
                return metadb::path_compare(
                    m_shadows[left].path.c_str(), m_shadows[right].path.c_str()) < 0;
            });
            std::vector<file_lock_ptr> shadow_locks;
            shadow_locks.reserve(m_documents.size());
            status.set_item("Waiting for the external editor to release isolated files");
            const char* previous_path = nullptr;
            for (size_t index : shadow_order) {
                abort.check();
                const char* current_path = m_shadows[index].path.c_str();
                if (previous_path != nullptr && same_path(previous_path, current_path)) continue;
                shadow_locks.push_back(file_lock_manager::get()->acquire_write(
                    current_path, abort));
                previous_path = current_path;
            }

            for (auto& document : m_documents) {
                document.changed = false;
                document.rechaptered = false;
                document.cue_after.clear();
                document.virtual_expected_after.clear();
                document.track_number_after.clear();
                for (unsigned subsong : document.virtual_subsongs) {
                    document.track_number_after.emplace(subsong, subsong);
                }
            }
            std::vector<std::vector<cue_field_edit>> edits(m_documents.size());
            std::vector<std::vector<cue_track_number_edit>> track_number_edits(
                m_documents.size());
            struct change_reference {
                size_t shadow_index = 0;
                size_t change_index = 0;
            };
            std::map<size_t, std::map<std::string, std::vector<change_reference>>> groups;
            std::map<size_t, std::vector<change_reference>> track_number_groups;
            std::map<size_t, std::map<replaygain_component, std::vector<size_t>>>
                replaygain_groups;
            pfc::string_formatter summary;
            summary << "Projected-shadow review\n\n"
                    << "Virtual tracks: " << m_shadows.size() << "\n"
                    << "Cue documents: " << m_documents.size() << "\n\n";
            bool safe = true;
            size_t change_count = 0;
            const auto flac_metadata_operation_supported = [](const metadata_field_change& change) {
                const bool supported_track_field =
                    change.name == "TITLE" || change.name == "ARTIST" ||
                    change.name == "SONGWRITER" || change.name == "ISRC" ||
                    change.name == "DATE" || change.name == "COMMENT";
                const bool supported_global_operation =
                    (change.name == "ALBUM" || change.name == "ALBUM ARTIST" ||
                     change.name == "GENRE" || change.name == "DISCNUMBER" ||
                     change.name == "TOTALDISCS") &&
                    change.status == virtual_field_status::candidate_global;
                const bool supported_scope =
                    (supported_track_field &&
                     (change.status == virtual_field_status::safe_direct ||
                      change.status == virtual_field_status::safe_transformed)) ||
                    supported_global_operation;
                return supported_scope &&
                    change.multivalue_operation == metadata_multivalue_operation::none &&
                    change.before.size() <= 1 && change.after.size() <= 1 &&
                    (change.before.empty() || !change.before[0].empty()) &&
                    (change.after.empty() || !change.after[0].empty());
            };
            status.set_progress(0, m_shadows.size());
            std::vector<input_info_reader::ptr> shadow_readers(m_documents.size());
            for (size_t document_index = 0; document_index < m_documents.size(); ++document_index) {
                input_entry::g_open_for_info_read(
                    shadow_readers[document_index],
                    nullptr,
                    m_documents[document_index].shadow_path.c_str(),
                    abort);
            }

            for (size_t index = 0; index < m_shadows.size(); ++index) {
                abort.check();
                projected_shadow& shadow = m_shadows[index];
                status.set_item(shadow.path.c_str());
                if (!captured_virtual_shadow_postimage(
                        shadow.path.c_str(), shadow.track_number, shadow.after)) {
                    shadow_readers[shadow.document_index]->get_info(
                        shadow.track_number, shadow.after, abort);
                }
                shadow.diff = diff_shadow_metadata(shadow.before, shadow.after);
                for (const replaygain_change& change : shadow.diff.replaygain_changes) {
                    ++change_count;
                    m_log.verbose(
                        "diff.classified",
                        {
                            log_count("document", shadow.document_index + 1),
                            log_number("track", shadow.track_number),
                            log_token("field", replaygain_component_name(change.component)),
                            log_token("change", metadata_change_kind_name(change.kind)),
                            log_token("scope", "REPLAYGAIN"),
                            log_token("classification", "COMMITTABLE_CANDIDATE"),
                        });
                    summary << "TRACK " << shadow.track_number << "  ReplayGain "
                            << replaygain_component_name(change.component) << "  "
                            << metadata_change_kind_name(change.kind) << "  CANDIDATE_CUE_STATE\n";
                    append_replaygain_value(
                        summary, "Before", change.component, change.before);
                    append_replaygain_value(
                        summary, "After", change.component, change.after);
                    replaygain_groups[shadow.document_index][change.component].push_back(index);
                }
                for (size_t change_index = 0; change_index < shadow.diff.changes.size(); ++change_index) {
                    const auto& change = shadow.diff.changes[change_index];
                    ++change_count;
                    m_log.verbose(
                        "diff.classified",
                        {
                            log_count("document", shadow.document_index + 1),
                            log_number("track", shadow.track_number),
                            log_token("field", change.name.c_str()),
                            log_token("change", metadata_change_kind_name(change.kind)),
                            log_count("before_values", change.before.size()),
                            log_count("after_values", change.after.size()),
                            log_token("scope", virtual_field_status_name(change.status)),
                            log_token("classification", virtual_diff_class_name(change.diff_class)),
                            log_token(
                                "reason",
                                virtual_reject_reason_name(change.reject_reason)),
                        });
                    summary << "TRACK " << shadow.track_number << "  " << change.name.c_str()
                            << "  " << metadata_change_kind_name(change.kind) << "  "
                            << virtual_field_status_name(change.status) << "  "
                            << virtual_diff_class_name(change.diff_class);
                    if (change.multivalue_operation != metadata_multivalue_operation::none) {
                        summary << "  "
                                << metadata_multivalue_operation_name(
                                    change.multivalue_operation);
                        if (change.multivalue_duplicates) summary << "  DUPLICATES";
                    }
                    summary << "\n";
                    append_value_list(summary, "Before", change.before);
                    append_value_list(summary, "After", change.after);
                    const bool track_number_candidate =
                        change.name == "TRACKNUMBER" &&
                        change.kind == metadata_change_kind::change &&
                        change.multivalue_operation == metadata_multivalue_operation::none &&
                        change.before.size() == 1 && change.after.size() == 1 &&
                        !change.before[0].empty() && !change.after[0].empty();
                    const bool field_safe = track_number_candidate ||
                        (change.status == virtual_field_status::safe_direct ||
                         change.status == virtual_field_status::safe_transformed ||
                         change.status == virtual_field_status::candidate_global) &&
                        change.before.size() <= 1 && change.after.size() <= 1 &&
                        (change.before.empty() || !change.before[0].empty()) &&
                        (change.after.empty() || !change.after[0].empty());
                    const bool storage_safe =
                        m_documents[shadow.document_index].storage !=
                            raw_cuesheet_storage_kind::flac_vorbis_comment ||
                        track_number_candidate ||
                        flac_metadata_operation_supported(change);
                    if (change.diff_class == virtual_diff_class::known_skippable) {
                        ++m_skipped_change_count;
                        ++m_skipped_fields[change.name];
                        if (m_apply_supported_changes) {
                            summary << "  skip KNOWN_UNREPRESENTABLE: "
                                    << change.reason.c_str()
                                    << " The original field value will be retained.\n";
                        } else {
                            safe = false;
                            note_blocking("STRICT_ALL_OR_NOTHING");
                            summary << "  reject STRICT_ALL_OR_NOTHING: this proven, independently "
                                       "skippable field changed while strict policy is active.\n";
                        }
                    } else if (!field_safe ||
                               change.diff_class == virtual_diff_class::blocking) {
                        safe = false;
                        note_blocking(change.reject_reason);
                        summary << "  reject "
                                << virtual_reject_reason_name(change.reject_reason) << ": "
                                << change.reason.c_str() << "\n";
                    } else if (!storage_safe) {
                        safe = false;
                        note_blocking("FLAC_STORAGE_CAPABILITY_UNSUPPORTED");
                        summary << "  reject FLAC_STORAGE_CAPABILITY_UNSUPPORTED: "
                                   "the current FLAC slice permits only qualified single-value "
                                   "metadata operations and scopes.\n";
                    } else if (track_number_candidate) {
                        ++m_accepted_change_count;
                        track_number_groups[shadow.document_index].push_back({ index, change_index });
                    } else {
                        ++m_accepted_change_count;
                        groups[shadow.document_index][change.name].push_back({ index, change_index });
                    }
                }
                status.set_progress(index + 1, m_shadows.size());
            }
            for (const auto& document_group : replaygain_groups) {
                for (const auto& component_group : document_group.second) {
                    m_accepted_change_count += component_group.second.size();
                }
            }
            m_timing.diff_classification_us = runtime_elapsed_us(runtime_analyze_begin);
            const auto runtime_planning_begin = runtime_clock::now();
#if defined(FB2K_ENABLE_TEST_PROBES)
            m_benchmark.analyze_diff_us = elapsed_microseconds(analyze_begin);
            const auto planning_begin = benchmark_clock::now();
#endif

            for (const auto& document_group : track_number_groups) {
                const size_t document_index = document_group.first;
                source_document& document = m_documents[document_index];
                try {
                    for (const auto& reference : document_group.second) {
                        const projected_shadow& shadow = m_shadows[reference.shadow_index];
                        const metadata_field_change& change =
                            shadow.diff.changes[reference.change_index];
                        if (change.before[0] != std::to_string(shadow.track_number)) {
                            throw pfc::exception(
                                "TRACKNUMBER preimage does not match the original Cue subsong identity");
                        }
                        const unsigned after = parse_canonical_track_number(change.after[0]);
                        document.track_number_after.at(shadow.track_number) = after;
                        if (after != shadow.track_number) {
                            track_number_edits[document_index].push_back({
                                shadow.track_number,
                                after,
                            });
                        }
                    }
                    unsigned previous = 0;
                    for (unsigned original : document.virtual_subsongs) {
                        const unsigned after = document.track_number_after.at(original);
                        if (after <= previous) {
                            throw pfc::exception(
                                "renumbered TRACK values conflict or change raw track order");
                        }
                        previous = after;
                    }
                    // This also proves that raw TRACK block order, uniqueness and
                    // source numbers match the requested structural identities.
                    (void)patch_cue_track_numbers(
                        document.cue_before,
                        track_number_edits[document_index]);
                    document.rechaptered = !track_number_edits[document_index].empty();
                    summary << "  resolve TRACKNUMBER: structural TRACK change; "
                            << "identity remains raw block ordinal and INDEX 01\n";
                    m_log.verbose(
                        "scope.resolved",
                        {
                            log_count("document", document_index + 1),
                            log_token("field", "TRACKNUMBER"),
                            log_token("scope", "STRUCTURAL_TRACK"),
                            log_count("impact_tracks", document_group.second.size()),
                        });
                } catch (const std::exception& error) {
                    safe = false;
                    note_blocking("STRUCTURAL_CHANGE");
                    summary << "  reject TRACKNUMBER: STRUCTURAL_CHANGE: "
                            << error.what() << "\n";
                }
            }

            if (change_count == 0) {
                m_timing.planning_us = runtime_elapsed_us(runtime_planning_begin);
                m_log.standard(
                    "diff.classified",
                    {
                        log_count("accepted", 0),
                        log_count("skipped", 0),
                        log_count("blocking", 0),
                    });
#if defined(FB2K_ENABLE_TEST_PROBES)
                m_benchmark.analyze_planning_us = elapsed_microseconds(planning_begin);
#endif
                summary << "No metadata changes were found. Nothing can be committed.";
                m_summary = summary.c_str();
                return;
            }
            m_has_changes = true;

            for (const auto& document_groups : groups) {
                const size_t document_index = document_groups.first;
                const source_document& document = m_documents[document_index];
                for (const auto& named_group : document_groups.second) {
                    const std::string& field_name = named_group.first;
                    const auto& references = named_group.second;
                    const metadata_field_change& first_change =
                        m_shadows[references[0].shadow_index].diff.changes[references[0].change_index];

                    bool track_resolved = first_change.status != virtual_field_status::candidate_global;
                    std::vector<cue_field_edit> track_edits;
                    std::string track_reject_reason;
                    if (track_resolved) {
                        try {
                            for (const auto& reference : references) {
                                const projected_shadow& shadow = m_shadows[reference.shadow_index];
                                const metadata_field_change& change =
                                    shadow.diff.changes[reference.change_index];
                                const size_t occurrence = cue_field_occurrence_count(
                                    document.cue_before, shadow.track_number, change.cue_command);
                                if (occurrence > 1) {
                                    track_reject_reason = "MALFORMED_CUE: track directive is duplicated.";
                                    track_resolved = false;
                                    break;
                                }
                                if (occurrence == 0) {
                                    // A missing raw token with an existing effective value is inherited.
                                    // Keep the established full-document rule instead of silently
                                    // converting a partial inherited edit into an override.
                                    if (!change.before.empty()) {
                                        track_resolved = false;
                                        break;
                                    }
                                    if (change.after.empty()) {
                                        track_reject_reason =
                                            "DELETE_SEMANTICS_UNSAFE: no explicit track directive exists to remove.";
                                        track_resolved = false;
                                        break;
                                    }
                                    track_edits.push_back({
                                        shadow.track_number,
                                        change.cue_command,
                                        change.after[0],
                                        cue_field_edit_kind::insert,
                                    });
                                    continue;
                                }

                                if (change.after.empty()) {
                                    std::string inherited_command;
                                    if (field_name != "TITLE" &&
                                        global_cue_command_for_field(field_name, inherited_command) &&
                                        cue_field_occurrence_count(
                                            document.cue_before, 0, inherited_command) == 1) {
                                        track_reject_reason =
                                            "INHERITANCE_CONFLICT: removing this explicit directive would expose a global value, not the requested missing value.";
                                        track_resolved = false;
                                        break;
                                    }
                                    track_edits.push_back({
                                        shadow.track_number,
                                        change.cue_command,
                                        {},
                                        cue_field_edit_kind::remove,
                                    });
                                    continue;
                                }

                                bool remove_to_inherit = false;
                                std::string inherited_command;
                                if (field_name != "TITLE" &&
                                    global_cue_command_for_field(field_name, inherited_command) &&
                                    cue_field_occurrence_count(
                                        document.cue_before, 0, inherited_command) == 1 &&
                                    cue_field_value(document.cue_before, 0, inherited_command) == change.after[0]) {
                                    remove_to_inherit = true;
                                }
                                track_edits.push_back({
                                    shadow.track_number,
                                    change.cue_command,
                                    remove_to_inherit ? std::string() : change.after[0],
                                    remove_to_inherit ? cue_field_edit_kind::remove : cue_field_edit_kind::replace,
                                });
                            }
                        } catch (const std::exception& error) {
                            track_reject_reason = std::string("MALFORMED_CUE: ") + error.what();
                            track_resolved = false;
                        }
                    }

                    if (track_resolved) {
                        edits[document_index].insert(
                            edits[document_index].end(), track_edits.begin(), track_edits.end());
                        summary << "  resolve " << field_name.c_str() << ": track scope (";
                        bool first = true;
                        size_t inheritance_resolutions = 0;
                        for (size_t edit_index = 0; edit_index < track_edits.size(); ++edit_index) {
                            const auto& edit = track_edits[edit_index];
                            const auto& change = m_shadows[references[edit_index].shadow_index]
                                .diff.changes[references[edit_index].change_index];
                            if (!first) summary << ", ";
                            first = false;
                            summary << (edit.kind == cue_field_edit_kind::insert ? "insert" :
                                edit.kind == cue_field_edit_kind::remove && !change.after.empty() ?
                                    (++inheritance_resolutions, "remove override -> inherited") :
                                edit.kind == cue_field_edit_kind::remove ? "remove" : "replace");
                        }
                        summary << ")\n";
                        m_log.verbose(
                            "scope.resolved",
                            {
                                log_count("document", document_index + 1),
                                log_token("field", field_name.c_str()),
                                log_token("scope", "TRACK"),
                                log_count("track_edits", track_edits.size()),
                                log_count("inheritance_resolutions", inheritance_resolutions),
                            });
                        continue;
                    }

                    std::string global_command;
                    std::string reject_reason = track_reject_reason;
                    if (!global_cue_command_for_field(field_name, global_command)) {
                        if (reject_reason.empty()) {
                            reject_reason = "UNSUPPORTED_FIELD: no proven global fallback exists for this track field.";
                        }
                    }

                    std::vector<const projected_shadow*> document_shadows;
                    if (reject_reason.empty()) {
                        for (const auto& shadow : m_shadows) {
                            if (shadow.document_index == document_index) document_shadows.push_back(&shadow);
                        }
                        if (document_shadows.size() != document.virtual_subsongs.size()) {
                            reject_reason = "PARTIAL_GLOBAL_SELECTION: global edit requires every virtual subsong of the source document.";
                        }
                    }
                    if (reject_reason.empty()) {
                        for (unsigned subsong : document.virtual_subsongs) {
                            if (std::none_of(
                                    document_shadows.begin(),
                                    document_shadows.end(),
                                    [subsong](const projected_shadow* shadow) {
                                        return shadow->track_number == subsong;
                                    })) {
                                reject_reason = "PARTIAL_GLOBAL_SELECTION: selection does not exactly cover the Cue document.";
                                break;
                            }
                        }
                    }

                    std::vector<std::string> global_values;
                    bool global_state_initialized = false;
                    if (reject_reason.empty()) {
                        for (const projected_shadow* shadow : document_shadows) {
                            const std::vector<std::string> values =
                                metadata_values(shadow->after, field_name);
                            if (values.size() > 1 || (!values.empty() && values[0].empty())) {
                                reject_reason = "MULTIVALUE_UNSUPPORTED: global edit requires zero or one non-empty final value.";
                                break;
                            }
                            if (!global_state_initialized) {
                                global_values = values;
                                global_state_initialized = true;
                            } else if (values != global_values) {
                                reject_reason = "AMBIGUOUS_SCOPE: global edit requires the same final value on every virtual track.";
                                break;
                            }
                        }
                    }

                    if (reject_reason.empty()) {
                        try {
                            const size_t global_occurrence = cue_field_occurrence_count(
                                document.cue_before, 0, global_command);
                            if (global_occurrence > 1 || (global_occurrence == 0 && global_values.empty())) {
                                reject_reason =
                                    "DELETE_SEMANTICS_UNSAFE: global Cue representation is missing or ambiguous.";
                            }
                            if (reject_reason.empty() && global_command == "PERFORMER") {
                                size_t track_performer_count = 0;
                                for (unsigned subsong : document.virtual_subsongs) {
                                    if (cue_field_occurrence_count(
                                            document.cue_before, subsong, "PERFORMER") == 1) {
                                        ++track_performer_count;
                                    }
                                }
                                if (field_name == "ALBUM ARTIST" &&
                                    track_performer_count != document.virtual_subsongs.size()) {
                                    reject_reason =
                                        "AMBIGUOUS_SCOPE: global PERFORMER maps to ALBUM ARTIST only when every track has its own PERFORMER.";
                                } else if (field_name == "ARTIST" && track_performer_count != 0) {
                                    reject_reason =
                                        "INHERITANCE_CONFLICT: global PERFORMER maps to inherited ARTIST only when no track has its own PERFORMER.";
                                }
                            }
                            // Track TITLE/PERFORMER are different Cue semantics
                            // from global ALBUM/ALBUM ARTIST despite sharing the
                            // directive spelling. Other globally resolved fields
                            // can be genuinely overridden at track scope.
                            if (field_name != "ALBUM" && field_name != "ALBUM ARTIST") {
                                for (unsigned subsong : document.virtual_subsongs) {
                                    if (cue_field_occurrence_count(
                                            document.cue_before,
                                            subsong,
                                            global_command) != 0) {
                                        reject_reason = "INHERITANCE_CONFLICT: a track-level override makes the global edit ambiguous.";
                                        break;
                                    }
                                }
                            }
                        } catch (const std::exception& error) {
                            reject_reason = std::string("MALFORMED_CUE: ") + error.what();
                        }
                    }

                    if (!reject_reason.empty()) {
                        safe = false;
                        note_blocking(reject_reason.c_str());
                        summary << "  reject " << field_name.c_str() << ": "
                                << reject_reason.c_str() << "\n";
                    } else {
                        const size_t occurrence = cue_field_occurrence_count(
                            document.cue_before, 0, global_command);
                        edits[document_index].push_back({
                            0,
                            global_command,
                            global_values.empty() ? std::string() : global_values[0],
                            global_values.empty() ? cue_field_edit_kind::remove :
                                occurrence == 0 ? cue_field_edit_kind::insert : cue_field_edit_kind::replace,
                        });
                        summary << "  resolve " << field_name.c_str()
                                << ": unique document/global scope ("
                                << (global_values.empty() ? "remove" : occurrence == 0 ? "insert" : "replace")
                                << ")\n";
                        m_log.verbose(
                            "scope.resolved",
                            {
                                log_count("document", document_index + 1),
                                log_token("field", field_name.c_str()),
                                log_token("scope", "GLOBAL"),
                                log_token("operation", global_values.empty() ? "REMOVE" : occurrence == 0 ? "INSERT" : "REPLACE"),
                                log_count("impact_tracks", document.virtual_subsongs.size()),
                            });
                    }
                }
            }

            for (const auto& document_groups : replaygain_groups) {
                const size_t document_index = document_groups.first;
                for (const auto& component_group : document_groups.second) {
                    try {
                        plan_replaygain_component(
                            document_index,
                            component_group.first,
                            edits[document_index]);
                        m_log.verbose(
                            "scope.resolved",
                            {
                                log_count("document", document_index + 1),
                                log_token("field", replaygain_component_name(component_group.first)),
                                log_token("scope", "REPLAYGAIN_EXACT"),
                                log_count("impact_tracks", component_group.second.size()),
                            });
                        summary << "  resolve ReplayGain "
                                << replaygain_component_name(component_group.first)
                                << ": exact numeric Cue state\n";
                    } catch (const std::exception& error) {
                        safe = false;
                        note_blocking("REPLAYGAIN_PRECISION_OR_REPRESENTATION_UNSAFE");
                        summary << "  reject ReplayGain "
                                << replaygain_component_name(component_group.first)
                                << ": REPLAYGAIN_PRECISION_OR_REPRESENTATION_UNSAFE: "
                                << error.what() << "\n"
                                << "  This operation changed virtual ReplayGain state, but the "
                                   "embedded Cuesheet cannot represent that state losslessly. "
                                   "No changes have been made to the original file.\n";
                    }
                }
            }

            if (safe) {
                for (auto& document_edits : edits) {
                    std::vector<cue_field_edit> unique;
                    for (const auto& edit : document_edits) {
                        const auto existing = std::find_if(
                            unique.begin(), unique.end(), [&edit](const cue_field_edit& candidate) {
                                return candidate.track_number == edit.track_number &&
                                    candidate.command == edit.command;
                            });
                        if (existing == unique.end()) {
                            unique.push_back(edit);
                        } else if (existing->new_value != edit.new_value || existing->kind != edit.kind) {
                            safe = false;
                            note_blocking("CONFLICTING_CUE_TARGET");
                            summary << "  reject: conflicting changes target the same Cue token\n";
                            break;
                        }
                    }
                    document_edits = std::move(unique);
                    if (!safe) break;
                }
            }

            if (safe) {
                for (size_t index = 0; index < m_documents.size(); ++index) {
                    m_documents[index].changed =
                        !edits[index].empty() || !track_number_edits[index].empty();
                    cue_patch_result patched = patch_cue_track_fields(
                        m_documents[index].cue_before,
                        edits[index]);
                    if (!track_number_edits[index].empty()) {
                        patched = patch_cue_track_numbers(
                            patched.text,
                            track_number_edits[index]);
                    }
                    m_documents[index].cue_after = patched.text;
                }

                // Construct the accepted postimage explicitly. Accepted diffs
                // use the Shadow target; safely skipped and unrelated fields
                // retain their exact original vectors. This is the dependency
                // gate: any accepted Cue patch that changes a skipped field
                // will fail complete staged postimage validation below.
                for (const projected_shadow& shadow : m_shadows) {
                    file_info_impl expected(shadow.before);
                    for (const metadata_field_change& change : shadow.diff.changes) {
                        const bool track_number_candidate =
                            change.name == "TRACKNUMBER" &&
                            change.kind == metadata_change_kind::change &&
                            change.before.size() == 1 && change.after.size() == 1;
                        if (change.diff_class == virtual_diff_class::committable ||
                            track_number_candidate) {
                            replace_metadata_values(expected, change.name, change.after);
                        }
                    }
                    if (!shadow.diff.replaygain_changes.empty()) {
                        expected.set_replaygain(shadow.after.get_replaygain());
                    }
                    m_documents[shadow.document_index].virtual_expected_after.emplace(
                        shadow.track_number,
                        std::move(expected));
                }
            }
            m_timing.planning_us = runtime_elapsed_us(runtime_planning_begin);
            const auto runtime_postimage_begin = runtime_clock::now();
            m_log.verbose(
                "accepted_postimage.built",
                {
                    log_count("committable_changes", m_accepted_change_count),
                    log_count("retained_skipped_changes", m_skipped_change_count),
                    log_count("impact_tracks", m_shadows.size()),
                });
#if defined(FB2K_ENABLE_TEST_PROBES)
            m_benchmark.analyze_planning_us = elapsed_microseconds(planning_begin);
            const auto postimage_begin = benchmark_clock::now();
#endif

            // Validate the complete effective postimage before the user is
            // offered a commit. Selected tracks must match their Shadows;
            // every unselected virtual track must remain metadata-identical.
            if (safe) {
                try {
                    for (size_t index = 0; index < m_documents.size(); ++index) {
                        source_document& document = m_documents[index];
                        if (!document.changed) {
                            validate_virtual_results(document.path.c_str(), index, abort);
                            continue;
                        }
                        pfc::string_formatter staged_name;
                        staged_name << "review-patched-" << (index + 1) << "-"
                                    << static_cast<unsigned long long>(GetTickCount64()) << "."
                                    << raw_cuesheet_storage_extension(document.storage);
                        const std::string staged_path = make_session_file_path(
                            m_directory, staged_name.c_str());
                        write_raw_cuesheet_patch(
                            document.storage,
                            document.path.c_str(),
                            staged_path.c_str(),
                            document.cue_before.c_str(),
                            document.cue_after.c_str(),
                            abort);
                        file_info_impl expected(document.physical_before);
                        expected.meta_set("CUESHEET", document.cue_after.c_str());
                        const file_snapshot staged = read_physical_snapshot(
                            staged_path.c_str(), abort);
                        if (!file_info::g_is_meta_equal(expected, staged.info) ||
                            expected.get_replaygain() != staged.info.get_replaygain()) {
                            throw pfc::exception("staged physical metadata does not match the planned Cue patch");
                        }
                        validate_virtual_results(staged_path.c_str(), index, abort);
                    }
                    summary << "\nPOSTIMAGE: PASS for every affected selected and unselected virtual subsong\n";
                } catch (const std::exception& error) {
                    safe = false;
                    note_blocking("POSTIMAGE_MISMATCH");
                    summary << "\n  reject POSTIMAGE_MISMATCH: " << error.what() << "\n";
                }
            }
            m_timing.accepted_postimage_us = runtime_elapsed_us(runtime_postimage_begin);
            m_log.verbose(
                "postimage.precommit_validation",
                {
                    log_token("result", safe ? "PASS" : "FAIL"),
                    log_count("impact_tracks", m_shadows.size()),
                    log_duration("duration_us", m_timing.accepted_postimage_us),
                });
#if defined(FB2K_ENABLE_TEST_PROBES)
            m_benchmark.analyze_postimage_us = elapsed_microseconds(postimage_begin);
#endif

            if (!safe) {
                summary << "\nRESULT: REJECT ENTIRE SESSION\n"
                        << "At least one diff is outside the strict release allowlist. "
                           "The original files remain untouched.";
            } else {
                summary << "\nRESULT: "
                        << (m_skipped_change_count == 0 ?
                            "COMMITTABLE" : "COMMITTABLE_WITH_SKIPPED_CHANGES")
                        << "\nACCEPTED: " << m_accepted_change_count
                        << "\nSKIPPED: " << m_skipped_change_count << "\n"
                        << "Only capability-resolved Cue directives will be minimally inserted, replaced or removed. "
                           "All other Cue bytes remain in place.\n\n"
                        << "Choose OK to commit, or Cancel to discard the session.";
            }
            m_log.standard(
                "diff.classified",
                {
                    log_count("accepted", m_accepted_change_count),
                    log_count("skipped", m_skipped_change_count),
                    log_count("blocking", m_blocking_change_count),
                });
            m_log.verbose(
                "plan.complete",
                {
                    log_flag("committable", safe),
                    log_count("source_documents", m_documents.size()),
                    log_count("impact_tracks", m_shadows.size()),
                    log_duration("duration_us", m_timing.planning_us),
                });
            m_committable = safe;
            m_summary = summary.c_str();
        } catch (const exception_aborted&) {
            m_log.standard(
                "command.cancelled",
                {log_token("reason", "COMMAND_CANCELLED")});
            throw;
        } catch (const std::exception& error) {
            m_error = error.what();
            note_blocking("DIFF_ANALYSIS_FAILED");
            m_log.standard(
                "diff.failed",
                {log_token("reason", "DIFF_ANALYSIS_FAILED")});
        }
    }

    bool committable() const { return m_committable; }
    bool has_changes() const { return m_has_changes; }
    const std::string& summary() const { return m_summary; }
    size_t selected_track_count() const { return m_shadows.size(); }
    size_t accepted_change_count() const { return m_accepted_change_count; }
    size_t skipped_change_count() const { return m_skipped_change_count; }
    const std::map<std::string, size_t>& skipped_fields() const { return m_skipped_fields; }
    const char* primary_reason() const { return m_primary_reason.c_str(); }
    bool originals_touched() const { return m_originals_touched; }
    bool rollback_succeeded() const {
        return m_rollback_attempted && !m_rollback_failed;
    }
    bool rollback_failed() const { return m_rollback_failed; }

    void commit(threaded_process_status& status, abort_callback& abort) {
        const auto runtime_commit_begin = runtime_clock::now();
        m_log.verbose(
            "commit.begin",
            {log_count("source_documents", m_documents.size())});
#if defined(FB2K_ENABLE_TEST_PROBES)
        const auto commit_begin = benchmark_clock::now();
        auto benchmark_begin = commit_begin;
#endif
        m_error.clear();
        m_originals_touched = false;
        m_rollback_attempted = false;
        m_rollback_failed = false;
        std::string stage = "validating commit state";
        if (!m_committable) {
            m_error = "Session is not committable";
            note_blocking("SESSION_NOT_COMMITTABLE");
            m_log.standard(
                "commit.failed",
                {log_token("reason", "SESSION_NOT_COMMITTABLE")});
            return;
        }
        std::vector<file_lock_ptr> locks;
        std::vector<size_t> order(m_documents.size());
        for (size_t index = 0; index < order.size(); ++index) order[index] = index;
        std::sort(order.begin(), order.end(), [this](size_t left, size_t right) {
            return metadb::path_compare(
                m_documents[left].path.c_str(), m_documents[right].path.c_str()) < 0;
        });

        try {
            auto runtime_phase_begin = runtime_clock::now();
            m_log.verbose(
                "stale.check",
                {log_token("state", "BEGIN")});
            stage = "acquiring source write locks";
            status.set_progress(0, order.size());
            for (size_t index : order) {
                abort.check();
                locks.push_back(file_lock_manager::get()->acquire_write(
                    m_documents[index].path.c_str(), abort));
            }

            // Revalidate every source before the first original is touched.
            stage = "revalidating original sources";
            for (size_t index : order) validate_unchanged(m_documents[index], abort);
            m_timing.stale_check_us += runtime_elapsed_us(runtime_phase_begin);
            m_log.verbose(
                "stale.check",
                {
                    log_count("source_documents", order.size()),
                    log_token("result", "MATCH"),
                    log_duration("duration_us", m_timing.stale_check_us),
                });
#if defined(FB2K_ENABLE_TEST_PROBES)
            m_benchmark.commit_lock_stale_us = elapsed_microseconds(benchmark_begin);
#endif

            // Byte-exact recovery copies are created while all foobar write locks are held.
            for (size_t index : order) {
                source_document& document = m_documents[index];
                if (!document.changed) continue;
                stage = "creating byte-exact recovery copy for " + document.path;
                pfc::string_formatter backup;
                backup << "backup-" << (index + 1) << "."
                       << raw_cuesheet_storage_extension(document.storage);
                document.backup_path = make_session_file_path(m_directory, backup.c_str());
                runtime_phase_begin = runtime_clock::now();
#if defined(FB2K_ENABLE_TEST_PROBES)
                benchmark_begin = benchmark_clock::now();
#endif
                filesystem::g_copy(document.path.c_str(), document.backup_path.c_str(), abort);
                if (fingerprint_file(document.backup_path.c_str(), abort) != document.fingerprint) {
                    throw pfc::exception("Recovery copy differs from the revalidated original source");
                }
                const uint64_t backup_us = runtime_elapsed_us(runtime_phase_begin);
                m_timing.backup_us += backup_us;
                m_log.verbose(
                    "commit.backup",
                    {
                        log_count("document", index + 1),
                        log_token("result", "PASS"),
                        log_duration("duration_us", backup_us),
                    });
#if defined(FB2K_ENABLE_TEST_PROBES)
                m_benchmark.commit_backup_us += elapsed_microseconds(benchmark_begin);
#endif
                pfc::string8 native;
                if (!filesystem::g_get_native_path(document.backup_path.c_str(), native)) {
                    throw pfc::exception("Recovery copy has no native path");
                }
                document.backup_native_path = native.c_str();
            }

            size_t completed = 0;
            const size_t changed_count = static_cast<size_t>(std::count_if(
                m_documents.begin(), m_documents.end(), [](const source_document& item) {
                    return item.changed;
                }));
            try {
                for (size_t index : order) {
                    source_document& document = m_documents[index];
                    if (!document.changed) continue;
                    stage = "building container-neutral raw CUESHEET patch for " + document.path;
                    status.set_item(document.path.c_str());
                    pfc::string_formatter patched_name;
                    patched_name << "patched-" << (index + 1) << "."
                                 << raw_cuesheet_storage_extension(document.storage);
                    const std::string patched_path = make_session_file_path(m_directory, patched_name.c_str());
                    runtime_phase_begin = runtime_clock::now();
#if defined(FB2K_ENABLE_TEST_PROBES)
                    benchmark_begin = benchmark_clock::now();
#endif
                    write_raw_cuesheet_patch(
                        document.storage,
                        document.backup_path.c_str(),
                        patched_path.c_str(),
                        document.cue_before.c_str(),
                        document.cue_after.c_str(),
                        fb2k::noAbort);

                    file_info_impl expected(document.physical_before);
                    expected.meta_set("CUESHEET", document.cue_after.c_str());
                    const file_snapshot staged = read_physical_snapshot(patched_path.c_str(), fb2k::noAbort);
                    if (!file_info::g_is_meta_equal(expected, staged.info) ||
                        expected.get_replaygain() != staged.info.get_replaygain()) {
                        throw pfc::exception("Staged raw patch metadata validation failed");
                    }
                    validate_virtual_results(patched_path.c_str(), index, fb2k::noAbort);
                    const hasher_md5_result staged_fingerprint = fingerprint_file(patched_path.c_str(), fb2k::noAbort);
                    pfc::string8 patched_native;
                    if (!filesystem::g_get_native_path(patched_path.c_str(), patched_native)) {
                        throw pfc::exception("Staged raw patch has no native path");
                    }
                    const uint64_t staging_us = runtime_elapsed_us(runtime_phase_begin);
                    m_timing.staging_us += staging_us;
                    m_log.verbose(
                        "commit.stage",
                        {
                            log_count("document", index + 1),
                            log_token("result", "PASS"),
                            log_duration("duration_us", staging_us),
                        });
#if defined(FB2K_ENABLE_TEST_PROBES)
                    m_benchmark.commit_staging_us += elapsed_microseconds(benchmark_begin);
#endif

                    stage = "revalidating source immediately before installation for " + document.path;
                    runtime_phase_begin = runtime_clock::now();
                    validate_unchanged(document, fb2k::noAbort);
                    m_timing.stale_check_us += runtime_elapsed_us(runtime_phase_begin);
                    m_log.verbose(
                        "stale.check",
                        {
                            log_count("document", index + 1),
                            log_token("result", "MATCH"),
                        });
                    stage = "installing validated raw CUESHEET patch for " + document.path;
                    runtime_phase_begin = runtime_clock::now();
#if defined(FB2K_ENABLE_TEST_PROBES)
                    benchmark_begin = benchmark_clock::now();
#endif
                    // Mark this conservatively before the overwrite: a failed
                    // native copy may already have modified its destination.
                    m_originals_touched = true;
                    copy_file_overwrite_native(patched_native.c_str(), document.native_path);
                    const uint64_t install_us = runtime_elapsed_us(runtime_phase_begin);
                    m_timing.install_us += install_us;
                    m_log.verbose(
                        "commit.install",
                        {
                            log_count("document", index + 1),
                            log_duration("duration_us", install_us),
                        });
                    runtime_phase_begin = runtime_clock::now();
#if defined(FB2K_ENABLE_TEST_PROBES)
                    m_benchmark.commit_install_us += elapsed_microseconds(benchmark_begin);
                    benchmark_begin = benchmark_clock::now();
#endif
                    stage = "post-validating " + document.path;
                    if (fingerprint_file(document.path.c_str(), fb2k::noAbort) != staged_fingerprint) {
                        throw pfc::exception("Installed file differs from the validated staged patch");
                    }
                    const file_snapshot after = read_physical_snapshot(document.path.c_str(), fb2k::noAbort);
                    if (!file_info::g_is_meta_equal(expected, after.info) ||
                        expected.get_replaygain() != after.info.get_replaygain()) {
                        throw pfc::exception("Post-commit metadata validation failed");
                    }
                    // The installed file is byte-identical to the staged file,
                    // whose complete selected/unselected virtual postimage was
                    // already validated above. Reopen selected tracks here as
                    // an installed-path smoke gate. Reopening an unselected
                    // embedded subsong while the source write lock is held
                    // crashes foo_input_std 2.26 Preview; no extra integrity
                    // evidence would be gained after the byte identity check.
                    validate_virtual_results(document.path.c_str(), index, fb2k::noAbort, true);
                    const uint64_t postvalidation_us = runtime_elapsed_us(runtime_phase_begin);
                    m_timing.postvalidation_us += postvalidation_us;
                    m_log.verbose(
                        "postvalidate.complete",
                        {
                            log_count("document", index + 1),
                            log_token("result", "PASS"),
                            log_duration("duration_us", postvalidation_us),
                        });
#if defined(FB2K_ENABLE_TEST_PROBES)
                    m_benchmark.commit_postvalidation_us += elapsed_microseconds(benchmark_begin);
#endif
                    ++completed;
#if defined(FB2K_ENABLE_TEST_PROBES)
                    if (m_diagnostic_fail_after_first_install && completed == 1) {
                        throw pfc::exception(
                            "TEST-PROBE injected failure after first installed document");
                    }
#endif
                    status.set_progress(completed, changed_count);
                }
            } catch (...) {
                rollback_from_backups(order);
                throw;
            }
            m_committed = true;
            m_timing.commit_total_us = runtime_elapsed_us(runtime_commit_begin);
            m_log.standard(
                "commit.complete",
                {
                    log_count("source_documents", changed_count),
                    log_duration("duration_us", m_timing.commit_total_us),
                });
            m_log.standard(
                "postvalidation.complete",
                {log_token("result", "PASS")});
#if defined(FB2K_ENABLE_TEST_PROBES)
            m_benchmark.commit_total_us = elapsed_microseconds(commit_begin);
#endif
        } catch (const std::exception& error) {
            m_error = stage + ": " + error.what();
            m_timing.commit_total_us = runtime_elapsed_us(runtime_commit_begin);
            const char* reason =
                stage.find("revalidating") != std::string::npos ? "STALE_SOURCE" :
                stage.find("post-validating") != std::string::npos ? "POSTVALIDATION_FAILED" :
                m_rollback_failed ? "ROLLBACK_FAILED" : "COMMIT_FAILED";
            note_blocking(reason);
            m_log.standard(
                "commit.failed",
                {
                    log_token("reason", reason),
                    log_flag("originals_touched", m_originals_touched),
                    log_flag("rollback_completed", rollback_succeeded()),
                });
#if defined(FB2K_ENABLE_TEST_PROBES)
            m_benchmark.commit_total_us = elapsed_microseconds(commit_begin);
#endif
        }
    }

    bool committed() const { return m_committed; }

    bool finalize_after_commit() {
        struct playlist_replacement {
            size_t playlist = 0;
            size_t item = 0;
            metadb_handle_ptr before;
            metadb_handle_ptr after;
        };

        std::vector<playlist_replacement> replacements;
        size_t applied = 0;
        try {
            playlist_manager::ptr playlists = playlist_manager::get();
            const size_t playlist_count = playlists->get_playlist_count();
            for (size_t playlist = 0; playlist < playlist_count; ++playlist) {
                const size_t item_count = playlists->playlist_get_item_count(playlist);
                bool affected_locked_playlist = false;
                for (size_t item = 0; item < item_count; ++item) {
                    const metadb_handle_ptr before =
                        playlists->playlist_get_item_handle(playlist, item);
                    for (const auto& document : m_documents) {
                        if (!document.changed || !document.rechaptered ||
                            !same_path(before->get_path(), document.path.c_str())) {
                            continue;
                        }
                        const auto found = document.track_number_after.find(
                            before->get_subsong_index());
                        if (found == document.track_number_after.end() ||
                            found->second == found->first) {
                            continue;
                        }
                        affected_locked_playlist = true;
                        replacements.push_back({
                            playlist,
                            item,
                            before,
                            metadb::get()->handle_create(document.path.c_str(), found->second),
                        });
                        break;
                    }
                }
                if (affected_locked_playlist && playlists->playlist_lock_is_present(playlist)) {
                    throw pfc::exception(
                        "A locked playlist contains a renumbered Cue track; exact handle replacement is unavailable");
                }
            }

            for (const auto& replacement : replacements) {
                if (!playlists->playlist_replace_item(
                        replacement.playlist,
                        replacement.item,
                        replacement.after)) {
                    throw pfc::exception(
                        "Could not replace a renumbered Cue handle in its playlist");
                }
                ++applied;
            }

            metadb_handle_list reload;
            for (const auto& document : m_documents) {
                if (!document.changed) continue;
                if (!document.rechaptered) {
                    reload.add_item(metadb::get()->handle_create(document.path.c_str(), 0));
                }
                for (unsigned original : document.virtual_subsongs) {
                    reload.add_item(metadb::get()->handle_create(
                        document.path.c_str(),
                        document.track_number_after.at(original)));
                }
            }
            if (reload.get_count() > 0) {
                metadb_io_v2::get()->load_info_async(
                    reload,
                    metadb_io::load_info_force,
                    core_api::get_main_window(),
                    metadb_io_v2::op_flag_delay_ui,
                    nullptr);
            }
            m_log.verbose(
                "finalize.complete",
                {
                    log_count("playlist_replacements", replacements.size()),
                    log_count("reload_handles", reload.get_count()),
                });
            return true;
        } catch (const std::exception& error) {
            playlist_manager::ptr playlists = playlist_manager::get();
            while (applied > 0) {
                --applied;
                const auto& replacement = replacements[applied];
                if (!playlists->playlist_replace_item(
                        replacement.playlist,
                        replacement.item,
                        replacement.before)) {
                    m_rollback_failed = true;
                    m_log.critical(
                        "rollback.playlist_handle",
                        {log_token("reason", "ROLLBACK_FAILED")});
                }
            }
            std::vector<size_t> order(m_documents.size());
            for (size_t index = 0; index < order.size(); ++index) order[index] = index;
            rollback_from_backups(order);
            m_committed = false;
            note_blocking(m_rollback_failed ? "ROLLBACK_FAILED" : "FINALIZATION_FAILED");
            m_log.standard(
                "finalize.failed",
                {
                    log_token("reason", m_rollback_failed ? "ROLLBACK_FAILED" : "FINALIZATION_FAILED"),
                    log_flag("rollback_completed", rollback_succeeded()),
                });
            m_error = std::string("finalizing renumbered playlist handles: ") + error.what();
            return false;
        }
    }

    void cleanup() {
        if (m_directory.empty()) return;
        const auto cleanup_begin = runtime_clock::now();
        for (const auto& document : m_documents) {
            if (!document.shadow_path.empty()) {
                unregister_virtual_shadow_path(document.shadow_path);
            }
        }
        filesystem::g_remove_object_recur(m_directory.c_str(), fb2k::noAbort);
        m_directory.clear();
        m_timing.cleanup_us += runtime_elapsed_us(cleanup_begin);
        m_log.verbose(
            "cleanup.complete",
            {log_duration("duration_us", m_timing.cleanup_us)});
    }

private:
    void note_blocking(const char* reason) {
        ++m_blocking_change_count;
        if (!m_primary_reason.empty()) return;
        if (reason == nullptr || *reason == 0) {
            m_primary_reason = "UNSPECIFIED_BLOCK";
            return;
        }
        const std::string text(reason);
        const size_t delimiter = text.find_first_of(": ;");
        m_primary_reason = text.substr(0, delimiter);
        if (m_primary_reason.empty()) m_primary_reason = "UNSPECIFIED_BLOCK";
    }

    void note_blocking(virtual_reject_reason reason) {
        note_blocking(virtual_reject_reason_name(reason));
    }

    void plan_replaygain_component(
        size_t document_index,
        replaygain_component component,
        std::vector<cue_field_edit>& edits) const {
        const source_document& document = m_documents[document_index];
        const std::string command = replaygain_cue_command(component);
        std::map<unsigned, replaygain_value> desired;
        for (unsigned subsong : document.virtual_subsongs) {
            const file_info* expected = &document.virtual_before.at(subsong);
            for (const projected_shadow& shadow : m_shadows) {
                if (shadow.document_index == document_index && shadow.track_number == subsong) {
                    expected = &shadow.after;
                    break;
                }
            }
            desired.emplace(
                subsong,
                replaygain_component_value(expected->get_replaygain(), component));
        }
        if (desired.empty()) throw pfc::exception("Cue document has no virtual tracks");

        const size_t global_occurrence = cue_field_occurrence_count(
            document.cue_before, 0, command);
        if (global_occurrence > 1) {
            throw pfc::exception("global ReplayGain directive is duplicated");
        }
        std::map<unsigned, size_t> local_occurrences;
        for (const auto& item : desired) {
            const size_t occurrence = cue_field_occurrence_count(
                document.cue_before, item.first, command);
            if (occurrence > 1) {
                throw pfc::exception("track ReplayGain directive is duplicated");
            }
            local_occurrences[item.first] = occurrence;
        }

        const replaygain_value common = desired.begin()->second;
        const bool all_equal = std::all_of(
            desired.begin(), desired.end(), [&common](const auto& item) {
                return item.second == common;
            });
        const auto add_edit = [&edits](
            unsigned track,
            const std::string& target_command,
            const std::string& value,
            cue_field_edit_kind kind) {
            edits.push_back({ track, target_command, value, kind });
        };

        if (all_equal) {
            if (common.present) {
                const std::string formatted = format_replaygain_value_exact(component, common);
                if (global_occurrence == 0) {
                    add_edit(0, command, formatted, cue_field_edit_kind::insert);
                } else if (cue_field_value(document.cue_before, 0, command) != formatted) {
                    add_edit(0, command, formatted, cue_field_edit_kind::replace);
                }
            } else if (global_occurrence == 1) {
                add_edit(0, command, {}, cue_field_edit_kind::remove);
            }
            for (const auto& item : local_occurrences) {
                if (item.second == 1) {
                    add_edit(item.first, command, {}, cue_field_edit_kind::remove);
                }
            }
            return;
        }

        if (global_occurrence == 1) {
            add_edit(0, command, {}, cue_field_edit_kind::remove);
        }
        for (const auto& item : desired) {
            const unsigned subsong = item.first;
            const replaygain_value& value = item.second;
            const size_t occurrence = local_occurrences.at(subsong);
            if (!value.present) {
                if (occurrence == 1) {
                    add_edit(subsong, command, {}, cue_field_edit_kind::remove);
                }
                continue;
            }
            const std::string formatted = format_replaygain_value_exact(component, value);
            if (occurrence == 0) {
                add_edit(subsong, command, formatted, cue_field_edit_kind::insert);
            } else if (cue_field_value(document.cue_before, subsong, command) != formatted) {
                add_edit(subsong, command, formatted, cue_field_edit_kind::replace);
            }
        }
    }

    void validate_virtual_results(
        const char* path,
        size_t document_index,
        abort_callback& abort,
        bool selected_only = false) const {
        const source_document& document = m_documents[document_index];
        input_info_reader::ptr reader;
        if (!selected_only) {
            const file_snapshot snapshot = read_physical_snapshot(path, abort, &reader);
            std::vector<unsigned> expected_subsongs;
            expected_subsongs.reserve(document.virtual_subsongs.size());
            for (unsigned original : document.virtual_subsongs) {
                expected_subsongs.push_back(document.track_number_after.at(original));
            }
            if (snapshot.virtual_subsongs != expected_subsongs) {
                throw pfc::exception(
                    "Virtual post-image has an unexpected track count, order or subsong identity");
            }
        } else {
            input_entry::g_open_for_info_read(reader, nullptr, path, abort);
        }
        for (unsigned subsong : document.virtual_subsongs) {
            if (selected_only && document.selected_subsongs.count(subsong) == 0) continue;
            const file_info* expected = nullptr;
            const file_info* expected_structural = nullptr;
            const auto planned = document.virtual_expected_after.find(subsong);
            if (planned != document.virtual_expected_after.end()) {
                expected = &planned->second;
                // Projected Shadows carry the physical file's technical
                // state (including its full duration), not the selected
                // embedded subsong's structural state. Only an explicit
                // timing transaction replaces that structural postimage.
                expected_structural = m_timing_prepared ?
                    &planned->second : &document.virtual_before.at(subsong);
            }
            for (const auto& shadow : m_shadows) {
                if (shadow.document_index == document_index && shadow.track_number == subsong) {
                    if (expected == nullptr) expected = &shadow.after;
                    break;
                }
            }
            if (expected == nullptr) expected = &document.virtual_before.at(subsong);
            const unsigned actual_subsong = document.track_number_after.at(subsong);
            file_info_impl actual;
            reader->get_info(actual_subsong, actual, abort);
            if (expected_structural == nullptr) {
                expected_structural = &document.virtual_before.at(subsong);
            }
            const bool metadata_equal = file_info::g_is_meta_equal(*expected, actual);
            const bool replaygain_equal = expected->get_replaygain() == actual.get_replaygain();
            const bool technical_equal = file_info::g_is_info_equal(*expected_structural, actual);
            const bool length_equal = expected_structural->get_length() == actual.get_length();
            if (!metadata_equal || !replaygain_equal || !technical_equal || !length_equal) {
                pfc::string_formatter message;
                message << "Virtual post-image validation failed for original TRACK "
                        << subsong << " mapped to TRACK " << actual_subsong;
                if (!metadata_equal) {
                    const metadata_diff_result mismatch = diff_shadow_metadata(*expected, actual);
                    if (!mismatch.changes.empty()) {
                        message << "; metadata mismatch at " << mismatch.changes.front().name.c_str();
                    } else {
                        message << "; metadata mismatch";
                    }
                }
                if (!replaygain_equal) message << "; ReplayGain mismatch";
                if (!technical_equal) message << "; technical-info mismatch";
                if (!length_equal) message << "; duration mismatch";
                throw pfc::exception(message);
            }
        }
    }

    size_t find_or_add_document(const char* canonical, abort_callback& abort) {
        for (size_t index = 0; index < m_documents.size(); ++index) {
            if (same_path(m_documents[index].path.c_str(), canonical)) {
                m_log.verbose(
                    "source.cache_hit",
                    {log_count("document", index + 1)});
                return index;
            }
        }
        const auto runtime_source_begin = runtime_clock::now();
        auto runtime_phase_begin = runtime_clock::now();
#if defined(FB2K_ENABLE_TEST_PROBES)
        auto benchmark_begin = benchmark_clock::now();
#endif
        input_info_reader::ptr snapshot_reader;
        file_snapshot snapshot = read_physical_snapshot(canonical, abort, &snapshot_reader);
#if defined(FB2K_ENABLE_TEST_PROBES)
        m_benchmark.source_snapshot_us += elapsed_microseconds(benchmark_begin);
#endif
        const size_t cue_index = snapshot.info.meta_find("CUESHEET");
        if (cue_index == SIZE_MAX || snapshot.info.meta_enum_value_count(cue_index) != 1) {
            throw pfc::exception("Physical source does not contain exactly one raw CUESHEET value");
        }
        pfc::string8 native;
        if (!filesystem::g_get_native_path(canonical, native)) {
            throw pfc::exception("Source has no local native path");
        }
        source_document document;
        document.path = canonical;
        document.native_path = native.c_str();
        document.stats = snapshot.stats;
        runtime_phase_begin = runtime_clock::now();
#if defined(FB2K_ENABLE_TEST_PROBES)
        benchmark_begin = benchmark_clock::now();
#endif
        document.fingerprint = fingerprint_file(canonical, abort);
        m_timing.source_hash_us += runtime_elapsed_us(runtime_phase_begin);
#if defined(FB2K_ENABLE_TEST_PROBES)
        m_benchmark.source_hash_us += elapsed_microseconds(benchmark_begin);
#endif
        document.physical_before = snapshot.info;
        document.virtual_subsongs = std::move(snapshot.virtual_subsongs);
        if (document.virtual_subsongs.empty()) {
            throw pfc::exception("Physical source exposes no embedded-Cue virtual subsongs");
        }
        runtime_phase_begin = runtime_clock::now();
#if defined(FB2K_ENABLE_TEST_PROBES)
        benchmark_begin = benchmark_clock::now();
#endif
        for (unsigned subsong : document.virtual_subsongs) {
            file_info_impl info;
            snapshot_reader->get_info(subsong, info, abort);
            document.virtual_before.emplace(subsong, std::move(info));
        }
        m_timing.virtual_inventory_us += runtime_elapsed_us(runtime_phase_begin);
#if defined(FB2K_ENABLE_TEST_PROBES)
        m_benchmark.virtual_inventory_us += elapsed_microseconds(benchmark_begin);
#endif
        document.cue_before = snapshot.info.meta_enum_value(cue_index, 0);
        runtime_phase_begin = runtime_clock::now();
#if defined(FB2K_ENABLE_TEST_PROBES)
        benchmark_begin = benchmark_clock::now();
#endif
        document.storage = identify_raw_cuesheet_storage(
            canonical, document.cue_before.c_str(), abort);
        m_timing.storage_inspection_us += runtime_elapsed_us(runtime_phase_begin);
#if defined(FB2K_ENABLE_TEST_PROBES)
        m_benchmark.storage_inspection_us += elapsed_microseconds(benchmark_begin);
#endif
        m_documents.push_back(std::move(document));
        m_timing.source_analysis_us += runtime_elapsed_us(runtime_source_begin);
        m_log.verbose(
            "source.document",
            {
                log_count("document", m_documents.size()),
                log_count("virtual_tracks", m_documents.back().virtual_subsongs.size()),
                log_token(
                    "storage",
                    raw_cuesheet_storage_extension(m_documents.back().storage)),
                log_duration("duration_us", runtime_elapsed_us(runtime_source_begin)),
            });
        return m_documents.size() - 1;
    }

    void validate_unchanged(const source_document& document, abort_callback& abort) const {
        const file_snapshot current = read_physical_snapshot(document.path.c_str(), abort);
        if (current.stats != document.stats ||
            current.info.get_replaygain() != document.physical_before.get_replaygain() ||
            !file_info::g_is_meta_equal(current.info, document.physical_before) ||
            fingerprint_file(document.path.c_str(), abort) != document.fingerprint) {
            m_log.standard(
                "stale.check",
                {
                    log_token("result", "MISMATCH"),
                    log_token("reason", "STALE_SOURCE"),
                });
            throw pfc::exception(
                "Original source changed after shadow creation; stale session rejected before commit");
        }
    }

    void rollback_from_backups(const std::vector<size_t>& order) noexcept {
        const auto rollback_begin = runtime_clock::now();
        bool any = false;
        m_log.verbose(
            "rollback.begin",
            {log_count("source_documents", order.size())});
        for (size_t index : order) {
            const source_document& document = m_documents[index];
            if (document.backup_native_path.empty()) continue;
            any = true;
            m_rollback_attempted = true;
            try {
                copy_file_overwrite_native(document.backup_native_path, document.native_path);
                m_log.verbose(
                    "rollback.document",
                    {log_count("document", index + 1), log_token("result", "RESTORED")});
            } catch (...) {
                m_rollback_failed = true;
                m_log.critical(
                    "rollback.document",
                    {log_count("document", index + 1), log_token("reason", "ROLLBACK_FAILED")});
            }
        }
        m_timing.rollback_us += runtime_elapsed_us(rollback_begin);
        if (!any) return;
        if (m_rollback_failed) {
            m_log.critical(
                "rollback.complete",
                {
                    log_token("result", "FAILED"),
                    log_token("reason", "ROLLBACK_FAILED"),
                    log_duration("duration_us", m_timing.rollback_us),
                });
        } else {
            m_log.standard(
                "rollback.complete",
                {
                    log_token("result", "PASS"),
                    log_flag("original_files_restored", true),
                    log_duration("duration_us", m_timing.rollback_us),
                });
        }
    }

    void cleanup_noexcept() noexcept {
        try {
            cleanup();
        } catch (...) {
            m_log.critical(
                "cleanup.failed",
                {log_token("reason", "CLEANUP_FAILED")});
        }
    }

    metadb_handle_list m_selection;
    operation_logger m_log;
    std::vector<source_document> m_documents;
    std::vector<projected_shadow> m_shadows;
    std::string m_directory;
    std::string m_error;
    std::string m_summary;
    bool m_prepared = false;
    bool m_has_changes = false;
    bool m_committable = false;
    bool m_committed = false;
    bool m_timing_prepared = false;
    bool m_originals_touched = false;
    bool m_rollback_attempted = false;
    bool m_rollback_failed = false;
    bool m_apply_supported_changes = true;
    size_t m_accepted_change_count = 0;
    size_t m_skipped_change_count = 0;
    size_t m_blocking_change_count = 0;
    std::string m_primary_reason;
    std::map<std::string, size_t> m_skipped_fields;
    runtime_timing_metrics m_timing;
#if defined(FB2K_ENABLE_TEST_PROBES)
    bool m_diagnostic_fail_after_first_install = false;
    projection_benchmark_metrics m_benchmark;
#endif
};

std::shared_ptr<virtual_edit_session> g_active_session;
bool g_building_shadow_context_menu = false;
bool g_finish_prompt_visible = false;

class shadow_menu_scope {
public:
    shadow_menu_scope() { g_building_shadow_context_menu = true; }
    ~shadow_menu_scope() { g_building_shadow_context_menu = false; }
};

contextmenu_node* find_shadow_command(contextmenu_node* node, const char* name) {
    if (node == nullptr) return nullptr;
    if (node->get_type() == contextmenu_item_node::TYPE_COMMAND) {
        return pfc::stricmp_ascii(node->get_name(), name) == 0 ? node : nullptr;
    }
    if (node->get_type() != contextmenu_item_node::TYPE_POPUP) return nullptr;
    for (size_t index = 0; index < node->get_num_children(); ++index) {
        if (contextmenu_node* found = find_shadow_command(node->get_child(index), name)) return found;
    }
    return nullptr;
}

void show_error(const char* heading, const std::string& detail) {
    pfc::string_formatter message;
    message << heading << "\n\n" << detail.c_str() << "\n\nNo original file was changed.";
    popup_message::g_show(message, "Metadata Isolation");
}

void show_transaction_failure(
    const std::shared_ptr<virtual_edit_session>& session,
    const char* heading) {
    pfc::string_formatter message;
    message << heading << "\n\n" << session->error().c_str() << "\n\n";
    if (!session->originals_touched()) {
        message << "The failure occurred before an original file was changed.";
    } else if (session->rollback_succeeded()) {
        message << "The operation failed and the original files were restored.";
    } else {
        message << "CRITICAL: Metadata Isolation could not confirm complete recovery. "
                   "Do not modify the affected files further; inspect the foobar2000 console "
                   "and the session recovery data.";
    }
    popup_message::g_show(message, "Metadata Isolation");
}

void show_finish_virtual_edit_prompt() {
    if (g_finish_prompt_visible || !g_active_session) return;
    g_finish_prompt_visible = true;
    const std::weak_ptr<virtual_edit_session> expected_session = g_active_session;
    popup_message_v3::query_t query;
    query.title = "Metadata Isolation - Finish virtual edit";
    query.msg =
        "Finish or cancel the external editor first.\n\n"
        "When it has finished writing, choose OK to validate and apply the virtual changes "
        "safely. Choose Cancel to leave the original files unchanged.";
    query.buttons = popup_message_v3::buttonOK | popup_message_v3::buttonCancel;
    query.defButton = popup_message_v3::buttonCancel;
    query.icon = popup_message_v3::iconQuestion;
    query.wndParent = core_api::get_main_window();
    query.reply = fb2k::makeCompletionNotify([expected_session](unsigned response) {
        g_finish_prompt_visible = false;
        const auto expected = expected_session.lock();
        if (!expected || g_active_session != expected) return;
        if ((response & popup_message_v3::buttonOK) != 0) {
            review_virtual_edit_session();
        } else {
            discard_virtual_edit_session(true);
        }
    });
    try {
        query.show();
    } catch (...) {
        g_finish_prompt_visible = false;
        throw;
    }
}

bool show_commit_dialog(
    const std::string& summary,
    const wchar_t* instruction = L"Review projected virtual metadata changes") {
    pfc::stringcvt::string_wide_from_utf8 summary_wide(summary.c_str());
    // TaskDialog owns these native buttons and exposes no per-button tooltip API.
    // The visible action labels and content therefore carry the complete accessible meaning.
    // On real Win32/x64 TaskDialogs, DM_GETDEFID reported IDOK even when
    // nDefaultButton explicitly requested IDCANCEL (and Discard was first).
    // Therefore NEVER make an implicit system IDOK equivalent to Commit.
    // These custom IDs are distinct from all standard dialog actions.
    constexpr int button_commit = 0x2001;
    constexpr int button_discard = 0x2002;
    const TASKDIALOG_BUTTON buttons[] = {
        { button_discard, L"Discard" },
        { button_commit, L"Commit" },
    };
    TASKDIALOGCONFIG config = {};
    config.cbSize = sizeof(config);
    config.hwndParent = core_api::get_main_window();
    config.dwFlags = TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle = L"Metadata Isolation";
    config.pszMainInstruction = instruction;
    config.pszContent = summary_wide;
    config.cButtons = _countof(buttons);
    config.pButtons = buttons;
    config.nDefaultButton = button_discard;
    int selected = button_discard;
    const HRESULT result = TaskDialogIndirect(&config, &selected, nullptr, nullptr);
    if (FAILED(result)) throw pfc::exception("Could not open the commit/discard review dialog");
    // Generic IDOK (e.g. Enter / native dialog fallback) never commits.
    return selected == button_commit;
}

struct timing_dialog_state {
    unsigned track_number = 0;
    cue_track_timing timing;
    cue_timing_edit result;
    fb2k::CCoreDarkModeHooks dark;
    dialog_tooltips tooltips;
};

struct timing_read_result {
    metadb_handle_ptr target;
    cue_track_timing timing;
    std::string error;
    bool ready = false;
};

constexpr LPARAM timing_choice_pregap = 1000;

void selected_timing_choice(
    HWND window,
    cue_timing_directive& directive,
    unsigned& index_number) {
    const HWND combo = GetDlgItem(window, IDC_TIMING_DIRECTIVE);
    const LRESULT selected = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (selected == CB_ERR) throw pfc::exception("No Cue timing directive is selected");
    const LRESULT data = SendMessageW(combo, CB_GETITEMDATA, selected, 0);
    if (data == CB_ERR) throw pfc::exception("Cue timing directive selection is invalid");
    if (data == timing_choice_pregap) {
        directive = cue_timing_directive::pregap;
        index_number = 0;
    } else {
        directive = cue_timing_directive::index;
        index_number = static_cast<unsigned>(data);
    }
}

void refresh_timing_dialog(HWND window, timing_dialog_state& state) {
    cue_timing_directive directive;
    unsigned index_number = 0;
    selected_timing_choice(window, directive, index_number);
    const cue_timing_value* current = timing_value(state.timing, directive, index_number);
    const std::string current_text = current == nullptr ? "<missing>" :
        format_cue_time_75(current->ticks);
    pfc::stringcvt::string_wide_from_utf8 current_wide(current_text.c_str());
    SetDlgItemTextW(window, IDC_TIMING_CURRENT, current_wide);
    SetDlgItemTextW(
        window,
        IDC_TIMING_VALUE,
        current == nullptr ? L"" : current_wide.get_ptr());
    const bool removable = current != nullptr &&
        !(directive == cue_timing_directive::index && index_number == 1);
    EnableWindow(GetDlgItem(window, IDC_TIMING_REMOVE), removable);
    CheckDlgButton(window, IDC_TIMING_REMOVE, BST_UNCHECKED);
    EnableWindow(GetDlgItem(window, IDC_TIMING_VALUE), TRUE);
}

INT_PTR CALLBACK timing_dialog_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<timing_dialog_state*>(
        GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<timing_dialog_state*>(lparam);
        SetWindowLongPtrW(window, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
        state->dark.AddDialogWithControls(window);
        if (state->tooltips.initialize(window, timing_tooltip_definitions())) {
            state->dark.AddCtrlAuto(state->tooltips.window());
        }
        pfc::string_formatter track_label;
        track_label << "Stable track identity: Cue TRACK " << state->track_number;
        pfc::stringcvt::string_wide_from_utf8 track_wide(track_label);
        SetDlgItemTextW(window, IDC_TIMING_TRACK, track_wide);
        const HWND combo = GetDlgItem(window, IDC_TIMING_DIRECTIVE);
        const auto add_choice = [combo](const wchar_t* label, LPARAM data) {
            const LRESULT item = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
            if (item == CB_ERR || item == CB_ERRSPACE) {
                throw pfc::exception("Could not populate the Cue timing directive list");
            }
            SendMessageW(combo, CB_SETITEMDATA, item, data);
        };
        try {
            add_choice(L"INDEX 01", 1);
            add_choice(L"INDEX 00", 0);
            add_choice(L"PREGAP", timing_choice_pregap);
            for (unsigned index = 2; index < 100; ++index) {
                wchar_t label[32] = {};
                _snwprintf_s(label, _countof(label), _TRUNCATE, L"INDEX %02u", index);
                add_choice(label, index);
            }
            SendMessageW(combo, CB_SETCURSEL, 0, 0);
            refresh_timing_dialog(window, *state);
        } catch (const std::exception& error) {
            pfc::stringcvt::string_wide_from_utf8 wide(error.what());
            MessageBoxW(window, wide, L"Metadata Isolation", MB_OK | MB_ICONERROR);
            EndDialog(window, IDCANCEL);
        }
        return TRUE;
    }
    if (message == WM_DPICHANGED) {
        const auto* suggested = reinterpret_cast<const RECT*>(lparam);
        if (suggested != nullptr) {
            SetWindowPos(
                window,
                nullptr,
                suggested->left,
                suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOACTIVATE | SWP_NOZORDER);
        }
        if (state != nullptr) state->tooltips.update_dpi(window);
        return TRUE;
    }
    if (state == nullptr || message != WM_COMMAND) return FALSE;
    const unsigned control = LOWORD(wparam);
    if (control == IDC_TIMING_DIRECTIVE && HIWORD(wparam) == CBN_SELCHANGE) {
        try {
            refresh_timing_dialog(window, *state);
        } catch (const std::exception& error) {
            pfc::stringcvt::string_wide_from_utf8 wide(error.what());
            MessageBoxW(window, wide, L"Metadata Isolation", MB_OK | MB_ICONERROR);
        }
        return TRUE;
    }
    if (control == IDC_TIMING_REMOVE && HIWORD(wparam) == BN_CLICKED) {
        EnableWindow(
            GetDlgItem(window, IDC_TIMING_VALUE),
            IsDlgButtonChecked(window, IDC_TIMING_REMOVE) != BST_CHECKED);
        return TRUE;
    }
    if (control == IDCANCEL) {
        EndDialog(window, IDCANCEL);
        return TRUE;
    }
    if (control != IDOK) return FALSE;
    try {
        cue_timing_directive directive;
        unsigned index_number = 0;
        selected_timing_choice(window, directive, index_number);
        const cue_timing_value* current = timing_value(state->timing, directive, index_number);
        const bool remove = IsDlgButtonChecked(window, IDC_TIMING_REMOVE) == BST_CHECKED;
        cue_timing_edit edit;
        edit.track_number = state->track_number;
        edit.directive = directive;
        edit.index_number = index_number;
        if (remove) {
            if (current == nullptr || (directive == cue_timing_directive::index && index_number == 1)) {
                throw pfc::exception("Only an existing optional timing directive can be removed");
            }
            edit.kind = cue_timing_edit_kind::remove;
            edit.before_ticks = current->ticks;
        } else {
            wchar_t value[128] = {};
            GetDlgItemTextW(window, IDC_TIMING_VALUE, value, _countof(value));
            pfc::stringcvt::string_utf8_from_wide utf8(value);
            edit.after_ticks = parse_cue_time_75(utf8.get_ptr());
            if (current == nullptr) {
                edit.kind = cue_timing_edit_kind::insert;
            } else {
                if (current->ticks == edit.after_ticks) {
                    throw pfc::exception("The requested timing value is unchanged");
                }
                edit.kind = cue_timing_edit_kind::replace;
                edit.before_ticks = current->ticks;
            }
        }
        state->result = edit;
        EndDialog(window, IDOK);
    } catch (const std::exception& error) {
        pfc::stringcvt::string_wide_from_utf8 wide(error.what());
        MessageBoxW(window, wide, L"Metadata Isolation", MB_OK | MB_ICONERROR);
    }
    return TRUE;
}

} // namespace

bool capture_virtual_shadow_write(
    const playable_location& location,
    const file_info& postimage) {
    const char* path = location.get_path();
    if (path == nullptr || *path == 0 || location.get_subsong() == 0) return false;
    std::lock_guard<std::mutex> lock(g_virtual_shadow_registry_mutex);
    for (auto& registered : g_virtual_shadow_registry) {
        if (metadb::path_compare(registered.first.c_str(), path) != 0) continue;
        registered.second.captured_postimages.insert_or_assign(
            location.get_subsong(), file_info_impl(postimage));
        return true;
    }
    return false;
}

bool virtual_edit_session_pending() {
    return g_active_session != nullptr;
}

bool virtual_shadow_menu_building() {
    return g_building_shadow_context_menu;
}

void begin_virtual_edit_session(metadb_handle_list_cref selection) {
    if (g_active_session) {
        popup_message::g_show(
            "A virtual metadata operation is already in progress. Finish or cancel it before "
            "starting another.",
            "Metadata Isolation");
        return;
    }
    auto session = std::make_shared<virtual_edit_session>(selection, "projected_virtual");
    auto worker = threaded_process_callback_lambda::create(
        [session](threaded_process_status& status, abort_callback& abort) {
            session->prepare(status, abort);
        });
    const bool completed = threaded_process::g_run_modal(
        worker,
        threaded_process::flag_show_abort | threaded_process::flag_show_progress |
            threaded_process::flag_show_item,
        core_api::get_main_window(),
        "Preparing isolated virtual metadata");
    if (!completed) {
        session->cleanup_for_completion();
        session->finish_logging("CANCELLED", "COMMAND_CANCELLED");
        return;
    }
    if (!session->prepared()) {
        show_error("The virtual metadata operation could not be prepared.", session->error());
        session->cleanup_for_completion();
        session->finish_logging("REJECTED", session->primary_reason());
        return;
    }

    g_active_session = session;
    session->logger().verbose(
        "command.begin",
        {
            log_token("surface", "CONTEXT_MENU"),
            log_count("projected_tracks", session->selected_track_count()),
        });
    const auto command_begin = runtime_clock::now();
    try {
        const metadb_handle_list handles = session->shadow_handles();
        shadow_menu_scope scope;
        contextmenu_manager::ptr manager = contextmenu_manager::g_create();
        manager->init_context_ex(
            handles,
            contextmenu_manager::flag_view_full,
            contextmenu_item::caller_undefined);
        manager->win32_run_menu_popup(core_api::get_main_window(), nullptr);
        session->record_command_duration(
            runtime_elapsed_us(command_begin), "RETURNED");
        show_finish_virtual_edit_prompt();
    } catch (const std::exception& error) {
        session->record_command_duration(
            runtime_elapsed_us(command_begin), "FAILED");
        discard_virtual_edit_session(false);
        session->finish_logging("REJECTED", "COMMAND_INVOCATION_FAILED");
        show_error("The isolated context menu could not be opened.", error.what());
    }
}

void edit_virtual_timing(metadb_handle_list_cref selection) {
    if (g_active_session) {
        popup_message::g_show(
            "A projected-shadow session is already pending. Review or discard it first.",
            "Metadata Isolation");
        return;
    }
    if (selection.get_count() != 1 || selection[0]->get_subsong_index() == 0) {
        popup_message::g_show(
            "Select exactly one virtual Embedded-Cue track.",
            "Metadata Isolation");
        return;
    }

    auto read = std::make_shared<timing_read_result>();
    const metadb_handle_ptr selected = selection[0];
    auto read_worker = threaded_process_callback_lambda::create(
        [read, selected](threaded_process_status& status, abort_callback& abort) {
            try {
                if (!qualified_timing_path(selected->get_path())) {
                    throw pfc::exception(
                        "Structural timing is release-scoped to qualified local MP3/FLAC/WavPack files");
                }
                pfc::string8 canonical;
                filesystem::g_get_canonical_path(selected->get_path(), canonical);
                if (filesystem::g_is_remote(canonical)) {
                    throw pfc::exception("Remote structural timing targets are unsupported");
                }
                status.set_item(canonical);
                const file_snapshot snapshot = read_physical_snapshot(canonical, abort);
                const size_t cue_index = snapshot.info.meta_find("CUESHEET");
                if (cue_index == SIZE_MAX || snapshot.info.meta_enum_value_count(cue_index) != 1) {
                    throw pfc::exception(
                        "Physical source does not contain exactly one raw CUESHEET value");
                }
                const unsigned track = selected->get_subsong_index();
                if (std::find(
                        snapshot.virtual_subsongs.begin(), snapshot.virtual_subsongs.end(), track) ==
                    snapshot.virtual_subsongs.end()) {
                    throw pfc::exception("Selected virtual track is not present in the current Cue document");
                }
                const cue_timing_document document = parse_cue_timing(
                    snapshot.info.meta_enum_value(cue_index, 0));
                read->target = metadb::get()->handle_create(canonical, track);
                read->timing = timing_track(document, track);
                read->ready = true;
            } catch (const exception_aborted&) {
                throw;
            } catch (const std::exception& error) {
                read->error = error.what();
            }
        });
    const bool loaded = threaded_process::g_run_modal(
        read_worker,
        threaded_process::flag_show_abort | threaded_process::flag_show_progress |
            threaded_process::flag_show_item,
        core_api::get_main_window(),
        "Reading embedded Cue timing");
    if (!loaded) return;
    if (!read->ready) {
        show_error("The Cue timing model could not be read.", read->error);
        return;
    }

    timing_dialog_state dialog;
    dialog.track_number = read->target->get_subsong_index();
    dialog.timing = read->timing;
    const INT_PTR dialog_result = DialogBoxParamW(
        core_api::get_my_instance(),
        MAKEINTRESOURCEW(IDD_EDIT_VIRTUAL_TIMING),
        core_api::get_main_window(),
        timing_dialog_proc,
        reinterpret_cast<LPARAM>(&dialog));
    if (dialog_result == -1) {
        show_error("The Cue timing editor could not be opened.", "Windows rejected the dialog resource");
        return;
    }
    if (dialog_result != IDOK) return;

    metadb_handle_list targets;
    targets.add_item(read->target);
    auto session = std::make_shared<virtual_edit_session>(targets, "structural_timing");
    std::vector<virtual_timing_edit_request> requests = {{ read->target, dialog.result }};
    auto prepare_worker = threaded_process_callback_lambda::create(
        [session, requests](threaded_process_status& status, abort_callback& abort) {
            session->prepare_timing(requests, status, abort);
        });
    const bool prepared = threaded_process::g_run_modal(
        prepare_worker,
        threaded_process::flag_show_abort | threaded_process::flag_show_progress |
            threaded_process::flag_show_item,
        core_api::get_main_window(),
        "Validating structural timing transaction");
    if (!prepared) {
        session->cleanup_for_completion();
        session->finish_logging("CANCELLED", "COMMAND_CANCELLED");
        return;
    }
    if (!session->error().empty() || !session->committable()) {
        show_error("The structural timing change cannot be committed safely.", session->error());
        session->cleanup_for_completion();
        session->finish_logging("REJECTED", session->primary_reason());
        return;
    }
    if (!show_commit_dialog(session->summary(), L"Review virtual Cue timing change")) {
        session->cleanup_for_completion();
        session->finish_logging("CANCELLED", "COMMAND_CANCELLED");
        return;
    }

    auto commit_worker = threaded_process_callback_lambda::create(
        [session](threaded_process_status& status, abort_callback& abort) {
            session->commit(status, abort);
        });
    threaded_process::g_run_modal(
        commit_worker,
        threaded_process::flag_show_progress | threaded_process::flag_show_item,
        core_api::get_main_window(),
        "Committing minimal Cue timing patch");
    if (!session->committed()) {
        show_transaction_failure(session, "The virtual timing operation failed.");
        session->cleanup_for_completion();
        session->finish_logging(
            session->rollback_failed() ||
                    (session->originals_touched() && !session->rollback_succeeded()) ?
                "ROLLBACK_FAILED" :
                session->rollback_succeeded() ? "FAILED_AND_ROLLED_BACK" : "REJECTED",
            session->primary_reason());
        return;
    }
    if (!session->finalize_after_commit()) {
        show_transaction_failure(session, "The virtual timing refresh failed.");
        session->cleanup_for_completion();
        session->finish_logging(
            session->rollback_failed() ? "ROLLBACK_FAILED" : "FAILED_AND_ROLLED_BACK",
            session->primary_reason());
        return;
    }
    session->cleanup_for_completion();
    session->finish_logging("SUCCESS");
    popup_message::g_show(
        "The exact 75 Hz Cue timing change was committed as a minimal raw-Cue patch. "
        "Complete virtual postimage validation passed.",
        "Metadata Isolation");
}

void review_virtual_edit_session() {
    const auto session = g_active_session;
    if (!session) {
        popup_message::g_show("No virtual metadata operation is awaiting completion.", "Metadata Isolation");
        return;
    }
    auto worker = threaded_process_callback_lambda::create(
        [session](threaded_process_status& status, abort_callback& abort) {
            session->analyze(status, abort);
        });
    const bool completed = threaded_process::g_run_modal(
        worker,
        threaded_process::flag_show_abort | threaded_process::flag_show_progress |
            threaded_process::flag_show_item,
        core_api::get_main_window(),
        "Validating virtual metadata changes");
    if (!completed) {
        discard_virtual_edit_session(false);
        session->finish_logging("CANCELLED", "COMMAND_CANCELLED");
        popup_message::g_showToast("Virtual metadata operation canceled. Original files unchanged.");
        return;
    }
    if (!session->error().empty()) {
        show_transaction_failure(session, "The virtual changes could not be validated safely.");
        discard_virtual_edit_session(false);
        session->finish_logging("REJECTED", session->primary_reason());
        return;
    }
    if (!session->has_changes()) {
        discard_virtual_edit_session(false);
        session->finish_logging("NO_CHANGE", "COMMAND_NO_CHANGE");
        popup_message::g_showToast("No virtual metadata changes were found. Original files unchanged.");
        return;
    }
    if (!session->committable()) {
        pfc::string_formatter message;
        message << "The virtual changes could not be applied safely.\n\n"
                << session->summary().c_str()
                << "\n\nNo unsafe or unsupported changes were written. "
                   "Original files are unchanged.";
        popup_message::g_show(message, "Metadata Isolation - Rejected");
        discard_virtual_edit_session(false);
        session->finish_logging("REJECTED", session->primary_reason());
        return;
    }

    auto commit_worker = threaded_process_callback_lambda::create(
        [session](threaded_process_status& status, abort_callback& abort) {
            session->commit(status, abort);
        });
    // Commit deliberately has no abort button: cancelling a tag write can corrupt a file.
    threaded_process::g_run_modal(
        commit_worker,
        threaded_process::flag_show_progress | threaded_process::flag_show_item,
        core_api::get_main_window(),
        "Applying virtual metadata safely");
    if (!session->committed()) {
        show_transaction_failure(session, "The virtual metadata operation failed.");
        discard_virtual_edit_session(false);
        const char* outcome = session->rollback_failed() ||
                (session->originals_touched() && !session->rollback_succeeded()) ?
            "ROLLBACK_FAILED" :
            session->rollback_succeeded() ? "FAILED_AND_ROLLED_BACK" : "REJECTED";
        session->finish_logging(outcome, session->primary_reason());
        return;
    }

    if (!session->finalize_after_commit()) {
        show_transaction_failure(session, "The virtual metadata refresh failed.");
        discard_virtual_edit_session(false);
        const char* outcome = session->rollback_failed() ?
            "ROLLBACK_FAILED" :
            session->rollback_succeeded() ? "FAILED_AND_ROLLED_BACK" : "REJECTED";
        session->finish_logging(outcome, session->primary_reason());
        return;
    }
    const size_t updated = session->selected_track_count();
    const size_t accepted = session->accepted_change_count();
    const size_t skipped = session->skipped_change_count();
    const auto skipped_fields = session->skipped_fields();
    if (skipped != 0) {
        session->logger().standard(
            "partial_commit.complete",
            {
                log_count("accepted", accepted),
                log_count("skipped", skipped),
                log_flag("skipped_original_values_retained", true),
            });
    }
    discard_virtual_edit_session(false);
    session->finish_logging(
        skipped == 0 ? "SUCCESS" : "SUCCESS_WITH_SKIPPED_CHANGES");
    if (skipped != 0) {
        pfc::string_formatter partial;
        partial << (accepted == 0 ?
            "No representable virtual metadata changes were applied.\n\n" :
            "Virtual metadata updated.\n\n")
                << "Applied: " << accepted << " changes\n"
                << "Not applied: " << skipped << " changes";
        for (const auto& item : skipped_fields) {
            partial << "\n- " << item.second << " " << item.first.c_str();
        }
        partial << "\n\nReason: These fields have a proven metadata state but cannot be "
                   "represented losslessly in embedded Cue Sheets.\n\n"
                   "The skipped fields were left unchanged.";
        popup_message::g_show(partial, "Metadata Isolation - Completed with skipped changes");
        return;
    }
    pfc::string_formatter success;
    success << updated << (updated == 1 ? " virtual track updated safely." :
        " virtual tracks updated safely.");
    popup_message::g_showToast(success);
}

void discard_virtual_edit_session(bool notify_user) {
    auto session = std::move(g_active_session);
    g_active_session.reset();
    if (session) {
        session->cleanup_for_completion();
        if (notify_user) {
            session->finish_logging("CANCELLED", "COMMAND_CANCELLED");
        }
    }
    if (notify_user) {
        popup_message::g_show(
            "The virtual metadata operation was canceled. Original files are unchanged.",
            "Metadata Isolation");
    }
}

void shutdown_virtual_edit_session() {
    discard_virtual_edit_session(false);
}

void cleanup_stale_virtual_edit_sessions() {
    wchar_t temp_path[MAX_PATH + 1] = {};
    const DWORD length = GetTempPathW(MAX_PATH, temp_path);
    if (length == 0 || length > MAX_PATH) return;

    wchar_t pattern[MAX_PATH + 1] = {};
    _snwprintf_s(
        pattern,
        _countof(pattern),
        _TRUNCATE,
        L"%sfoo_metadata_isolation-*",
        temp_path);
    WIN32_FIND_DATAW entry = {};
    HANDLE search = FindFirstFileW(pattern, &entry);
    if (search == INVALID_HANDLE_VALUE) return;
    do {
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) continue;
        unsigned long owner_pid = 0;
        unsigned long long tick = 0;
        unsigned long sequence = 0;
        wchar_t trailing = 0;
        if (swscanf_s(
                entry.cFileName,
                L"foo_metadata_isolation-%lu-%llu-%lu%c",
                &owner_pid,
                &tick,
                &sequence,
                &trailing,
                1) != 3) continue;
        if (owner_pid == GetCurrentProcessId()) continue;

        bool stale = false;
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, owner_pid);
        if (process != nullptr) {
            stale = WaitForSingleObject(process, 0) == WAIT_OBJECT_0;
            CloseHandle(process);
        } else if (GetLastError() == ERROR_INVALID_PARAMETER) {
            stale = true;
        }
        if (!stale) continue;

        wchar_t full_path[MAX_PATH + 1] = {};
        _snwprintf_s(full_path, _countof(full_path), _TRUNCATE, L"%s%s", temp_path, entry.cFileName);
        try {
            pfc::stringcvt::string_utf8_from_wide utf8(full_path);
            pfc::string8 canonical;
            filesystem::g_get_canonical_path(utf8, canonical);
            filesystem::g_remove_object_recur(canonical, fb2k::noAbort);
        } catch (const std::exception&) {
            operation_logger cleanup_log("stale_shadow_cleanup", "maintenance");
            cleanup_log.critical(
                "cleanup.failed", {log_token("reason", "STALE_TEMP_REMOVE_FAILED")});
            cleanup_log.finish("FAILED", "STALE_TEMP_REMOVE_FAILED");
        }
    } while (FindNextFileW(search, &entry));
    FindClose(search);
}

#if defined(FB2K_ENABLE_TEST_PROBES)
void finish_diagnostic_logging(
    const std::shared_ptr<virtual_edit_session>& session,
    bool succeeded) {
    session->cleanup_for_completion();
    const char* outcome = succeeded ?
        (session->skipped_change_count() == 0 ? "SUCCESS" :
            "SUCCESS_WITH_SKIPPED_CHANGES") :
        session->rollback_failed() ? "ROLLBACK_FAILED" :
        session->rollback_succeeded() ? "FAILED_AND_ROLLED_BACK" :
        !session->has_changes() ? "NO_CHANGE" : "REJECTED";
    session->finish_logging(outcome, succeeded ? nullptr : session->primary_reason());
}

bool run_virtual_edit_session_diagnostic(
    metadb_handle_list_cref selection,
    const char* context_command_name,
    bool commit,
    pfc::string_base& report,
    bool fail_after_first_install,
    bool apply_supported_changes) {
    pfc::string_formatter output;
    auto session = std::make_shared<virtual_edit_session>(selection);
    session->diagnostic_fail_after_first_install(fail_after_first_install);
    session->diagnostic_apply_supported_changes(apply_supported_changes);
    auto prepare_worker = threaded_process_callback_lambda::create(
        [session](threaded_process_status& status, abort_callback& abort) {
            session->prepare(status, abort);
        });
    if (!threaded_process::g_run_modal(
            prepare_worker,
            threaded_process::flag_show_progress | threaded_process::flag_show_item,
            core_api::get_main_window(),
            "Diagnostic: prepare projected shadows") ||
        !session->prepared()) {
        output << "diagnostic_prepared=false\nerror=" << session->error().c_str() << "\n";
        finish_diagnostic_logging(session, false);
        report = output;
        return false;
    }
    output << "diagnostic_prepared=true\nshadow_count="
           << session->shadow_handles().get_count() << "\n";

    try {
        const metadb_handle_list shadows = session->shadow_handles();
        shadow_menu_scope scope;
        contextmenu_manager::ptr manager = contextmenu_manager::g_create();
        manager->init_context_ex(
            shadows,
            contextmenu_manager::flag_view_full,
            contextmenu_item::caller_undefined);
        contextmenu_node* command = find_shadow_command(manager->get_root(), context_command_name);
        if (command == nullptr) throw pfc::exception("Requested diagnostic context command not found");
        command->execute();

        auto analyze_worker = threaded_process_callback_lambda::create(
            [session](threaded_process_status& status, abort_callback& abort) {
                session->analyze(status, abort);
            });
        threaded_process::g_run_modal(
            analyze_worker,
            threaded_process::flag_show_progress | threaded_process::flag_show_item,
            core_api::get_main_window(),
            "Diagnostic: analyze projected shadows");
        output << "diagnostic_has_changes=" << (session->has_changes() ? "true" : "false") << "\n"
               << "diagnostic_committable=" << (session->committable() ? "true" : "false") << "\n"
               << "diagnostic_summary_begin\n" << session->summary().c_str()
               << "\ndiagnostic_summary_end\n";
        if (!session->error().empty()) {
            output << "error=" << session->error().c_str() << "\n";
        }
        if (commit && session->committable()) {
            auto commit_worker = threaded_process_callback_lambda::create(
                [session](threaded_process_status& status, abort_callback& abort) {
                    session->commit(status, abort);
                });
            threaded_process::g_run_modal(
                commit_worker,
                threaded_process::flag_show_progress | threaded_process::flag_show_item,
                core_api::get_main_window(),
                "Diagnostic: commit projected shadows");
        }
        if (session->committed()) {
            session->finalize_after_commit();
        }
        output << "diagnostic_committed=" << (session->committed() ? "true" : "false") << "\n";
        if (!session->error().empty()) output << "error=" << session->error().c_str() << "\n";
        const bool committed = session->committed();
        finish_diagnostic_logging(session, committed);
        report = output;
        return committed;
    } catch (const std::exception& error) {
        output << "error=" << error.what() << "\ndiagnostic_committed=false\n";
        finish_diagnostic_logging(session, false);
        report = output;
        return false;
    }
}

bool run_virtual_projection_benchmark_diagnostic(
    metadb_handle_list_cref selection,
    bool complete_transaction,
    pfc::string_base& report) {
    pfc::string_formatter output;
    IO_COUNTERS io_before = {};
    GetProcessIoCounters(GetCurrentProcess(), &io_before);

    const auto benchmark_begin = benchmark_clock::now();
    auto session = std::make_shared<virtual_edit_session>(selection);
    auto prepare_worker = threaded_process_callback_lambda::create(
        [session](threaded_process_status& status, abort_callback& abort) {
            session->prepare(status, abort);
        });
    const bool prepared = threaded_process::g_run_modal(
        prepare_worker,
        threaded_process::flag_show_progress | threaded_process::flag_show_item,
        core_api::get_main_window(),
        "Diagnostic: benchmark projected shadows");
    output << "benchmark_complete_transaction="
           << (complete_transaction ? "true" : "false") << "\n"
           << "benchmark_selection_count=" << selection.get_count() << "\n"
           << "benchmark_prepared=" << (prepared && session->prepared() ? "true" : "false")
           << "\n";

    bool succeeded = prepared && session->prepared();
    uint64_t context_setup_us = 0;
    uint64_t command_execute_return_us = 0;
    try {
        if (!succeeded) throw pfc::exception(session->error().c_str());
        if (complete_transaction) {
            const metadb_handle_list shadows = session->shadow_handles();
            shadow_menu_scope scope;
            auto command_begin = benchmark_clock::now();
            contextmenu_manager::ptr manager = contextmenu_manager::g_create();
            manager->init_context_ex(
                shadows,
                contextmenu_manager::flag_view_full,
                contextmenu_item::caller_undefined);
            contextmenu_node* command = find_shadow_command(
                manager->get_root(), "Research: Set projected shadow TITLE");
            if (command == nullptr) {
                throw pfc::exception("Benchmark context command was not found");
            }
            context_setup_us = elapsed_microseconds(command_begin);
            command_begin = benchmark_clock::now();
            command->execute();
            command_execute_return_us = elapsed_microseconds(command_begin);
            session->record_command_duration(command_execute_return_us, "RETURNED");

            auto analyze_worker = threaded_process_callback_lambda::create(
                [session](threaded_process_status& status, abort_callback& abort) {
                    session->analyze(status, abort);
                });
            if (!threaded_process::g_run_modal(
                    analyze_worker,
                    threaded_process::flag_show_progress | threaded_process::flag_show_item,
                    core_api::get_main_window(),
                    "Diagnostic: benchmark shadow analysis") ||
                !session->committable()) {
                throw pfc::exception(session->error().empty()
                    ? "Benchmark transaction was not committable"
                    : session->error().c_str());
            }

            auto commit_worker = threaded_process_callback_lambda::create(
                [session](threaded_process_status& status, abort_callback& abort) {
                    session->commit(status, abort);
                });
            if (!threaded_process::g_run_modal(
                    commit_worker,
                    threaded_process::flag_show_progress | threaded_process::flag_show_item,
                    core_api::get_main_window(),
                    "Diagnostic: benchmark shadow commit") ||
                !session->committed()) {
                throw pfc::exception(session->error().empty()
                    ? "Benchmark transaction did not commit"
                    : session->error().c_str());
            }
            if (!session->finalize_after_commit()) {
                throw pfc::exception(session->error().c_str());
            }
        }
    } catch (const std::exception& error) {
        output << "error=" << error.what() << "\n";
        succeeded = false;
    }

    session->append_benchmark_metrics(output);
    output << "benchmark_context_setup_us=" << context_setup_us << "\n"
           << "benchmark_command_execute_return_us=" << command_execute_return_us << "\n";

    const auto cleanup_begin = benchmark_clock::now();
    try {
        session->cleanup();
    } catch (const std::exception& error) {
        output << "cleanup_error=" << error.what() << "\n";
        succeeded = false;
    }
    const uint64_t cleanup_us = elapsed_microseconds(cleanup_begin);

    IO_COUNTERS io_after = {};
    GetProcessIoCounters(GetCurrentProcess(), &io_after);
    output << "benchmark_cleanup_us=" << cleanup_us << "\n"
           << "benchmark_total_us=" << elapsed_microseconds(benchmark_begin) << "\n"
           << "benchmark_process_read_bytes="
           << (io_after.ReadTransferCount - io_before.ReadTransferCount) << "\n"
           << "benchmark_process_write_bytes="
           << (io_after.WriteTransferCount - io_before.WriteTransferCount) << "\n"
           << "benchmark_committed=" << (session->committed() ? "true" : "false") << "\n"
           << "benchmark_result=" << (succeeded ? "PASS" : "FAIL") << "\n";
    finish_diagnostic_logging(
        session, succeeded && (!complete_transaction || session->committed()));
    report = output;
    return succeeded;
}

bool begin_virtual_edit_session_diagnostic(
    metadb_handle_list_cref selection,
    const char* context_command_name,
    pfc::string_base& report) {
    pfc::string_formatter output;
    if (g_active_session) {
        output << "diagnostic_started=false\nerror=A projected-shadow session is already pending\n";
        report = output;
        return false;
    }

    auto session = std::make_shared<virtual_edit_session>(selection);
    auto prepare_worker = threaded_process_callback_lambda::create(
        [session](threaded_process_status& status, abort_callback& abort) {
            session->prepare(status, abort);
        });
    if (!threaded_process::g_run_modal(
            prepare_worker,
            threaded_process::flag_show_progress | threaded_process::flag_show_item,
            core_api::get_main_window(),
            "Diagnostic: prepare persistent projected shadows") ||
        !session->prepared()) {
        output << "diagnostic_started=false\nerror=" << session->error().c_str() << "\n";
        report = output;
        return false;
    }

    try {
        const metadb_handle_list shadows = session->shadow_handles();
        shadow_menu_scope scope;
        contextmenu_manager::ptr manager = contextmenu_manager::g_create();
        manager->init_context_ex(
            shadows,
            contextmenu_manager::flag_view_full,
            contextmenu_item::caller_undefined);
        contextmenu_node* command = find_shadow_command(manager->get_root(), context_command_name);
        if (command == nullptr) throw pfc::exception("Requested diagnostic context command not found");

        // Publish the session before executing: third-party commands may return
        // immediately and complete their own modal/modeless UI later.
        g_active_session = session;
        command->execute();
        output << "diagnostic_started=true\nshadow_count=" << shadows.get_count()
               << "\ncontext_command=" << context_command_name << "\n";
        report = output;
        return true;
    } catch (const std::exception& error) {
        g_active_session.reset();
        output << "diagnostic_started=false\nerror=" << error.what() << "\n";
        report = output;
        return false;
    }
}

bool finish_virtual_edit_session_diagnostic(bool commit, pfc::string_base& report) {
    pfc::string_formatter output;
    const auto session = g_active_session;
    if (!session) {
        output << "diagnostic_finished=false\nerror=No projected-shadow session is pending\n";
        report = output;
        return false;
    }

    try {
        auto analyze_worker = threaded_process_callback_lambda::create(
            [session](threaded_process_status& status, abort_callback& abort) {
                session->analyze(status, abort);
            });
        const bool analyzed = threaded_process::g_run_modal(
            analyze_worker,
            threaded_process::flag_show_progress | threaded_process::flag_show_item,
            core_api::get_main_window(),
            "Diagnostic: analyze pending projected shadows");
        output << "diagnostic_finished=" << (analyzed ? "true" : "false") << "\n"
               << "diagnostic_has_changes=" << (session->has_changes() ? "true" : "false") << "\n"
               << "diagnostic_committable=" << (session->committable() ? "true" : "false") << "\n"
               << "diagnostic_summary_begin\n" << session->summary().c_str()
               << "\ndiagnostic_summary_end\n";
        if (!analyzed || !session->error().empty()) {
            output << "error=" << session->error().c_str() << "\n";
        } else if (commit && session->committable()) {
            auto commit_worker = threaded_process_callback_lambda::create(
                [session](threaded_process_status& status, abort_callback& abort) {
                    session->commit(status, abort);
                });
            threaded_process::g_run_modal(
                commit_worker,
                threaded_process::flag_show_progress | threaded_process::flag_show_item,
                core_api::get_main_window(),
                "Diagnostic: commit pending projected shadows");
        }
        if (session->committed()) session->finalize_after_commit();
        const bool committed = session->committed();
        output << "diagnostic_committed=" << (committed ? "true" : "false") << "\n";
        if (!session->error().empty()) output << "error=" << session->error().c_str() << "\n";
        discard_virtual_edit_session(false);
        report = output;
        finish_diagnostic_logging(session, committed);
        return committed;
    } catch (const std::exception& error) {
        discard_virtual_edit_session(false);
        output << "error=" << error.what() << "\ndiagnostic_committed=false\n";
        report = output;
        finish_diagnostic_logging(session, false);
        return false;
    }
}

bool run_virtual_timing_diagnostic(
    const std::vector<virtual_timing_edit_request>& requests,
    bool commit,
    pfc::string_base& report,
    bool fail_after_first_install) {
    pfc::string_formatter output;
    metadb_handle_list selection;
    for (const auto& request : requests) selection.add_item(request.target);
    auto session = std::make_shared<virtual_edit_session>(selection);
    session->diagnostic_fail_after_first_install(fail_after_first_install);
    auto prepare_worker = threaded_process_callback_lambda::create(
        [session, requests](threaded_process_status& status, abort_callback& abort) {
            session->prepare_timing(requests, status, abort);
        });
    const bool prepared = threaded_process::g_run_modal(
        prepare_worker,
        threaded_process::flag_show_progress | threaded_process::flag_show_item,
        core_api::get_main_window(),
        "Diagnostic: prepare structural timing transaction");
    output << "diagnostic_prepared=" << (prepared && session->prepared() ? "true" : "false")
           << "\ndiagnostic_has_changes=" << (session->has_changes() ? "true" : "false")
           << "\ndiagnostic_committable=" << (session->committable() ? "true" : "false")
           << "\ndiagnostic_summary_begin\n" << session->summary().c_str()
           << "\ndiagnostic_summary_end\n";
    if (!session->error().empty()) output << "error=" << session->error().c_str() << "\n";
    if (prepared && commit && session->committable()) {
        auto commit_worker = threaded_process_callback_lambda::create(
            [session](threaded_process_status& status, abort_callback& abort) {
                session->commit(status, abort);
            });
        threaded_process::g_run_modal(
            commit_worker,
            threaded_process::flag_show_progress | threaded_process::flag_show_item,
            core_api::get_main_window(),
            "Diagnostic: commit structural timing transaction");
        if (session->committed()) session->finalize_after_commit();
    }
    output << "diagnostic_committed=" << (session->committed() ? "true" : "false") << "\n";
    if (!session->error().empty()) output << "error=" << session->error().c_str() << "\n";
    report = output;
    return session->committed();
}

bool begin_virtual_timing_diagnostic(
    const std::vector<virtual_timing_edit_request>& requests,
    pfc::string_base& report) {
    pfc::string_formatter output;
    if (g_active_session) {
        output << "diagnostic_started=false\nerror=A virtual session is already pending\n";
        report = output;
        return false;
    }
    metadb_handle_list selection;
    for (const auto& request : requests) selection.add_item(request.target);
    auto session = std::make_shared<virtual_edit_session>(selection);
    auto prepare_worker = threaded_process_callback_lambda::create(
        [session, requests](threaded_process_status& status, abort_callback& abort) {
            session->prepare_timing(requests, status, abort);
        });
    const bool prepared = threaded_process::g_run_modal(
        prepare_worker,
        threaded_process::flag_show_progress | threaded_process::flag_show_item,
        core_api::get_main_window(),
        "Diagnostic: prepare persistent structural timing transaction");
    output << "diagnostic_prepared=" << (prepared && session->prepared() ? "true" : "false")
           << "\ndiagnostic_has_changes=" << (session->has_changes() ? "true" : "false")
           << "\ndiagnostic_committable=" << (session->committable() ? "true" : "false")
           << "\ndiagnostic_summary_begin\n" << session->summary().c_str()
           << "\ndiagnostic_summary_end\n";
    if (!session->error().empty()) output << "error=" << session->error().c_str() << "\n";
    if (prepared && session->timing_prepared() && session->committable()) {
        g_active_session = session;
        output << "diagnostic_started=true\n";
        report = output;
        return true;
    }
    output << "diagnostic_started=false\n";
    report = output;
    return false;
}

bool finish_virtual_timing_diagnostic(bool commit, pfc::string_base& report) {
    pfc::string_formatter output;
    const auto session = g_active_session;
    if (!session || !session->timing_prepared()) {
        output << "diagnostic_finished=false\nerror=No structural timing session is pending\n";
        report = output;
        return false;
    }
    try {
        output << "diagnostic_finished=true\n"
               << "diagnostic_committable=" << (session->committable() ? "true" : "false") << "\n";
        if (commit && session->committable()) {
            auto commit_worker = threaded_process_callback_lambda::create(
                [session](threaded_process_status& status, abort_callback& abort) {
                    session->commit(status, abort);
                });
            threaded_process::g_run_modal(
                commit_worker,
                threaded_process::flag_show_progress | threaded_process::flag_show_item,
                core_api::get_main_window(),
                "Diagnostic: commit pending structural timing transaction");
            if (session->committed()) session->finalize_after_commit();
        }
        const bool committed = session->committed();
        output << "diagnostic_committed=" << (committed ? "true" : "false") << "\n";
        if (!session->error().empty()) output << "error=" << session->error().c_str() << "\n";
        discard_virtual_edit_session(false);
        report = output;
        return committed;
    } catch (const std::exception& error) {
        discard_virtual_edit_session(false);
        output << "error=" << error.what() << "\ndiagnostic_committed=false\n";
        report = output;
        return false;
    }
}
#endif

} // namespace metadata_isolation
