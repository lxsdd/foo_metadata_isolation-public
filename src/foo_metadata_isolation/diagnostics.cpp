#include "stdafx.h"
#include "guid.h"
#include "operation_log.h"
#include "ui_tooltips.h"
#include "physical_targets.h"
#include "physical_metadata_audit.h"
#include "preferences.h"
#include "write_guard.h"
#include "virtual_edit_session.h"
#include "ui_scaling.h"

#include <cstring>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace metadata_isolation {
namespace {

enum class probe_mode {
    virtual_title,
    virtual_artist,
    virtual_tracknumber,
    virtual_album,
    virtual_comment,
    virtual_custom,
    virtual_nochange,
    virtual_all_titles,
    direct_guard_virtual_title,
    chapterizer_title,
    physical_title,
    physical_all,
    physical_matrix,
    physical_replaygain_remove,
    physical_album_artist,
    physical_raw_cue_patch,
    map_physical,
    map_physical_with_duplicate,
    map_physical_with_remote,
    map_all_handles_physical,
    context_tree_physical,
    run_masstagger_physical,
    run_discogger_physical,
    run_discogger_plain_physical,
    run_fileops_move_physical,
    run_replaygain_remove_physical,
    run_cuesheet_editor,
    run_physical_router_menu,
    run_original_context_menu,
    run_original_discogger,
    run_original_masstagger,
    run_projected_shadow_discogger,
    run_projected_shadow_context_menu,
    run_projected_shadow_router,
    run_production_shadow_title,
    run_production_shadow_nochange,
    run_production_shadow_title_partial,
    run_production_shadow_title_artist,
    run_production_shadow_custom,
    run_production_shadow_mixed,
    run_production_shadow_delete,
    run_production_shadow_multivalue,
    run_production_shadow_native_text,
    run_production_shadow_latin1_text,
    run_production_shadow_unicode_text,
    run_production_shadow_long_text,
    run_production_shadow_unsafe_text,
    run_production_shadow_phase10_unicode,
    run_production_shadow_phase10_unicode_partial_style,
    run_production_shadow_phase10_unicode_rollback,
    run_production_shadow_global,
    run_production_shadow_global_partial,
    run_production_shadow_global_different,
    run_production_shadow_global_inherited,
    run_production_shadow_global_inherited_partial,
    run_production_shadow_global_inherited_uncoupled,
    run_production_shadow_add,
    run_production_shadow_delete_fields,
    run_production_shadow_remove_overrides,
    run_production_shadow_global_add,
    run_production_shadow_global_delete,
    run_production_shadow_add_mixed,
    run_production_shadow_remove_text_overrides,
    run_production_shadow_global_delete_partial,
    run_production_shadow_batch_atomic,
    run_production_shadow_delete_inherited_globals,
    run_production_shadow_add_partial,
    run_production_shadow_delete_fields_partial,
    run_production_shadow_rollback,
    run_production_shadow_partial_style,
    run_production_shadow_partial_style_strict,
    run_production_shadow_partial_style_blocking,
    run_production_shadow_partial_style_rollback,
    run_production_shadow_style_only,
    run_production_shadow_style_delete,
    run_production_shadow_replaygain_style,
    run_production_shadow_replaygain,
    run_production_shadow_replaygain_delete,
    run_production_shadow_replaygain_native_remove,
    run_production_shadow_replaygain_partial,
    run_production_shadow_replaygain_delete_partial,
    run_production_shadow_replaygain_mixed,
    run_production_shadow_replaygain_atomic,
    run_production_shadow_replaygain_rollback,
    run_production_shadow_tracknumber_valid,
    run_production_shadow_tracknumber_shift,
    run_production_shadow_tracknumber_conflict,
    run_production_shadow_tracknumber_reverse,
    run_production_shadow_tracknumber_alphanumeric,
    run_production_shadow_tracknumber_delete,
    run_production_shadow_tracknumber_atomic,
    run_production_shadow_tracknumber_mixed,
    run_production_shadow_tracknumber_partial,
    run_production_shadow_tracknumber_partial_reorder,
    run_production_shadow_tracknumber_playlist,
    run_production_shadow_tracknumber_playlist_locked,
    run_production_shadow_tracknumber_rollback,
    run_production_timing_index01_change,
    run_production_timing_index01_invalid,
    run_production_timing_pregap_add,
    run_production_timing_pregap_change,
    run_production_timing_pregap_delete,
    run_production_timing_index00_add,
    run_production_timing_index00_change,
    run_production_timing_index00_delete,
    run_production_timing_index02_add,
    run_production_timing_index02_change,
    run_production_timing_index02_delete,
    run_production_timing_batch,
    run_production_timing_rollback,
    run_production_timing_boundary_lf,
    run_production_timing_spacing,
    run_production_timing_terminal_add,
    begin_production_timing,
    finish_production_timing,
    run_product_timing_dialog,
    begin_production_shadow_tracknumber,
    begin_production_shadow_replaygain,
    begin_production_shadow_replaygain_scan,
    begin_production_shadow_title,
    begin_production_shadow_partial_style,
    begin_production_shadow_discogger,
    begin_production_shadow_discogger_mapping,
    begin_production_shadow_discogger_configuration,
    begin_production_shadow_musicbrainz,
    begin_production_shadow_masstagger,
    begin_production_shadow_masstagger_partial,
    begin_production_shadow_masstagger_saved_cue,
    finish_production_shadow,
    field_capability_lab,
    embedded_cue_inventory,
    projection_benchmark,
    projection_benchmark_inventory,
    physical_audit_classifier,
    logging_contract,
    logging_outcomes,
    logging_standard,
    logging_verbose,
    tooltip_contract,
    tooltip_contract_dark,
    open_preferences,
    open_preferences_dark,
    open_display_preferences,
    disable_guard,
    enable_guard,
};

class diagnostic_playlist_lock : public playlist_lock {
public:
    bool query_items_add(
        t_size,
        const pfc::list_base_const_t<metadb_handle_ptr>&,
        const bit_array&) override { return true; }
    bool query_items_reorder(const t_size*, t_size) override { return true; }
    bool query_items_remove(const bit_array&, bool) override { return true; }
    bool query_item_replace(
        t_size,
        const metadb_handle_ptr&,
        const metadb_handle_ptr&) override { return false; }
    bool query_playlist_rename(const char*, t_size) override { return true; }
    bool query_playlist_remove() override { return true; }
    bool execute_default_action(t_size) override { return false; }
    void on_playlist_index_change(t_size) override {}
    void on_playlist_remove() override {}
    void get_lock_name(pfc::string_base& output) override {
        output = "Metadata Isolation structural rollback probe";
    }
    void show_ui() override {}
    t_uint32 get_filter_mask() override { return filter_replace; }
};

bool parse_probe_mode(const char* token, probe_mode& mode) {
    struct mapping { const char* token; probe_mode mode; };
    static constexpr mapping mappings[] = {
        { "/metadata-isolation-probe", probe_mode::virtual_title },
        { "/metadata-isolation-probe-title", probe_mode::virtual_title },
        { "/metadata-isolation-probe-artist", probe_mode::virtual_artist },
        { "/metadata-isolation-probe-tracknumber", probe_mode::virtual_tracknumber },
        { "/metadata-isolation-probe-album", probe_mode::virtual_album },
        { "/metadata-isolation-probe-comment", probe_mode::virtual_comment },
        { "/metadata-isolation-probe-custom", probe_mode::virtual_custom },
        { "/metadata-isolation-probe-nochange", probe_mode::virtual_nochange },
        { "/metadata-isolation-probe-all-titles", probe_mode::virtual_all_titles },
        { "/metadata-isolation-probe-direct-guard-virtual-title", probe_mode::direct_guard_virtual_title },
        { "/metadata-isolation-probe-chapterizer-title", probe_mode::chapterizer_title },
        { "/metadata-isolation-probe-physical-title", probe_mode::physical_title },
        { "/metadata-isolation-probe-physical-all", probe_mode::physical_all },
        { "/metadata-isolation-probe-physical-matrix", probe_mode::physical_matrix },
        { "/metadata-isolation-probe-physical-replaygain-remove", probe_mode::physical_replaygain_remove },
        { "/metadata-isolation-probe-physical-album-artist", probe_mode::physical_album_artist },
        { "/metadata-isolation-probe-physical-raw-cue-patch", probe_mode::physical_raw_cue_patch },
        { "/metadata-isolation-probe-map-physical", probe_mode::map_physical },
        { "/metadata-isolation-probe-map-physical-duplicate", probe_mode::map_physical_with_duplicate },
        { "/metadata-isolation-probe-map-physical-remote", probe_mode::map_physical_with_remote },
        { "/metadata-isolation-probe-map-all-handles-physical", probe_mode::map_all_handles_physical },
        { "/metadata-isolation-probe-context-tree-physical", probe_mode::context_tree_physical },
        { "/metadata-isolation-probe-run-masstagger-physical", probe_mode::run_masstagger_physical },
        { "/metadata-isolation-probe-run-discogger-physical", probe_mode::run_discogger_physical },
        { "/metadata-isolation-probe-run-discogger-plain-physical", probe_mode::run_discogger_plain_physical },
        { "/metadata-isolation-probe-run-fileops-move-physical", probe_mode::run_fileops_move_physical },
        { "/metadata-isolation-probe-run-replaygain-remove-physical", probe_mode::run_replaygain_remove_physical },
        { "/metadata-isolation-probe-run-cuesheet-editor", probe_mode::run_cuesheet_editor },
        { "/metadata-isolation-probe-run-physical-router-menu", probe_mode::run_physical_router_menu },
        { "/metadata-isolation-probe-run-original-context-menu", probe_mode::run_original_context_menu },
        { "/metadata-isolation-probe-run-original-discogger", probe_mode::run_original_discogger },
        { "/metadata-isolation-probe-run-original-masstagger", probe_mode::run_original_masstagger },
        { "/metadata-isolation-probe-run-projected-shadow-discogger", probe_mode::run_projected_shadow_discogger },
        { "/metadata-isolation-probe-run-projected-shadow-context-menu", probe_mode::run_projected_shadow_context_menu },
        { "/metadata-isolation-probe-run-projected-shadow-router", probe_mode::run_projected_shadow_router },
        { "/metadata-isolation-probe-production-shadow-title", probe_mode::run_production_shadow_title },
        { "/metadata-isolation-probe-production-shadow-nochange", probe_mode::run_production_shadow_nochange },
        { "/metadata-isolation-probe-production-shadow-title-partial", probe_mode::run_production_shadow_title_partial },
        { "/metadata-isolation-probe-production-shadow-title-artist", probe_mode::run_production_shadow_title_artist },
        { "/metadata-isolation-probe-production-shadow-custom", probe_mode::run_production_shadow_custom },
        { "/metadata-isolation-probe-production-shadow-mixed", probe_mode::run_production_shadow_mixed },
        { "/metadata-isolation-probe-production-shadow-delete", probe_mode::run_production_shadow_delete },
        { "/metadata-isolation-probe-production-shadow-multivalue", probe_mode::run_production_shadow_multivalue },
        { "/metadata-isolation-probe-production-shadow-native-text", probe_mode::run_production_shadow_native_text },
        { "/metadata-isolation-probe-production-shadow-latin1-text", probe_mode::run_production_shadow_latin1_text },
        { "/metadata-isolation-probe-production-shadow-unicode-text", probe_mode::run_production_shadow_unicode_text },
        { "/metadata-isolation-probe-production-shadow-long-text", probe_mode::run_production_shadow_long_text },
        { "/metadata-isolation-probe-production-shadow-unsafe-text", probe_mode::run_production_shadow_unsafe_text },
        { "/metadata-isolation-probe-production-shadow-phase10-unicode", probe_mode::run_production_shadow_phase10_unicode },
        { "/metadata-isolation-probe-production-shadow-phase10-unicode-partial-style", probe_mode::run_production_shadow_phase10_unicode_partial_style },
        { "/metadata-isolation-probe-production-shadow-phase10-unicode-rollback", probe_mode::run_production_shadow_phase10_unicode_rollback },
        { "/metadata-isolation-probe-production-shadow-global", probe_mode::run_production_shadow_global },
        { "/metadata-isolation-probe-production-shadow-global-partial", probe_mode::run_production_shadow_global_partial },
        { "/metadata-isolation-probe-production-shadow-global-different", probe_mode::run_production_shadow_global_different },
        { "/metadata-isolation-probe-production-shadow-global-inherited", probe_mode::run_production_shadow_global_inherited },
        { "/metadata-isolation-probe-production-shadow-global-inherited-partial", probe_mode::run_production_shadow_global_inherited_partial },
        { "/metadata-isolation-probe-production-shadow-global-inherited-uncoupled", probe_mode::run_production_shadow_global_inherited_uncoupled },
        { "/metadata-isolation-probe-production-shadow-add", probe_mode::run_production_shadow_add },
        { "/metadata-isolation-probe-production-shadow-delete-fields", probe_mode::run_production_shadow_delete_fields },
        { "/metadata-isolation-probe-production-shadow-remove-overrides", probe_mode::run_production_shadow_remove_overrides },
        { "/metadata-isolation-probe-production-shadow-global-add", probe_mode::run_production_shadow_global_add },
        { "/metadata-isolation-probe-production-shadow-global-delete", probe_mode::run_production_shadow_global_delete },
        { "/metadata-isolation-probe-production-shadow-add-mixed", probe_mode::run_production_shadow_add_mixed },
        { "/metadata-isolation-probe-production-shadow-remove-text-overrides", probe_mode::run_production_shadow_remove_text_overrides },
        { "/metadata-isolation-probe-production-shadow-global-delete-partial", probe_mode::run_production_shadow_global_delete_partial },
        { "/metadata-isolation-probe-production-shadow-batch-atomic", probe_mode::run_production_shadow_batch_atomic },
        { "/metadata-isolation-probe-production-shadow-delete-inherited-globals", probe_mode::run_production_shadow_delete_inherited_globals },
        { "/metadata-isolation-probe-production-shadow-add-partial", probe_mode::run_production_shadow_add_partial },
        { "/metadata-isolation-probe-production-shadow-delete-fields-partial", probe_mode::run_production_shadow_delete_fields_partial },
        { "/metadata-isolation-probe-production-shadow-rollback", probe_mode::run_production_shadow_rollback },
        { "/metadata-isolation-probe-production-shadow-partial-style", probe_mode::run_production_shadow_partial_style },
        { "/metadata-isolation-probe-production-shadow-partial-style-strict", probe_mode::run_production_shadow_partial_style_strict },
        { "/metadata-isolation-probe-production-shadow-partial-style-blocking", probe_mode::run_production_shadow_partial_style_blocking },
        { "/metadata-isolation-probe-production-shadow-partial-style-rollback", probe_mode::run_production_shadow_partial_style_rollback },
        { "/metadata-isolation-probe-production-shadow-style-only", probe_mode::run_production_shadow_style_only },
        { "/metadata-isolation-probe-production-shadow-style-delete", probe_mode::run_production_shadow_style_delete },
        { "/metadata-isolation-probe-production-shadow-replaygain-style", probe_mode::run_production_shadow_replaygain_style },
        { "/metadata-isolation-probe-production-shadow-replaygain", probe_mode::run_production_shadow_replaygain },
        { "/metadata-isolation-probe-production-shadow-replaygain-delete", probe_mode::run_production_shadow_replaygain_delete },
        { "/metadata-isolation-probe-production-shadow-replaygain-native-remove", probe_mode::run_production_shadow_replaygain_native_remove },
        { "/metadata-isolation-probe-production-shadow-replaygain-partial", probe_mode::run_production_shadow_replaygain_partial },
        { "/metadata-isolation-probe-production-shadow-replaygain-delete-partial", probe_mode::run_production_shadow_replaygain_delete_partial },
        { "/metadata-isolation-probe-production-shadow-replaygain-mixed", probe_mode::run_production_shadow_replaygain_mixed },
        { "/metadata-isolation-probe-production-shadow-replaygain-atomic", probe_mode::run_production_shadow_replaygain_atomic },
        { "/metadata-isolation-probe-production-shadow-replaygain-rollback", probe_mode::run_production_shadow_replaygain_rollback },
        { "/metadata-isolation-probe-production-shadow-tracknumber-valid", probe_mode::run_production_shadow_tracknumber_valid },
        { "/metadata-isolation-probe-production-shadow-tracknumber-shift", probe_mode::run_production_shadow_tracknumber_shift },
        { "/metadata-isolation-probe-production-shadow-tracknumber-conflict", probe_mode::run_production_shadow_tracknumber_conflict },
        { "/metadata-isolation-probe-production-shadow-tracknumber-reverse", probe_mode::run_production_shadow_tracknumber_reverse },
        { "/metadata-isolation-probe-production-shadow-tracknumber-alphanumeric", probe_mode::run_production_shadow_tracknumber_alphanumeric },
        { "/metadata-isolation-probe-production-shadow-tracknumber-delete", probe_mode::run_production_shadow_tracknumber_delete },
        { "/metadata-isolation-probe-production-shadow-tracknumber-atomic", probe_mode::run_production_shadow_tracknumber_atomic },
        { "/metadata-isolation-probe-production-shadow-tracknumber-mixed", probe_mode::run_production_shadow_tracknumber_mixed },
        { "/metadata-isolation-probe-production-shadow-tracknumber-partial", probe_mode::run_production_shadow_tracknumber_partial },
        { "/metadata-isolation-probe-production-shadow-tracknumber-partial-reorder", probe_mode::run_production_shadow_tracknumber_partial_reorder },
        { "/metadata-isolation-probe-production-shadow-tracknumber-playlist", probe_mode::run_production_shadow_tracknumber_playlist },
        { "/metadata-isolation-probe-production-shadow-tracknumber-playlist-locked", probe_mode::run_production_shadow_tracknumber_playlist_locked },
        { "/metadata-isolation-probe-production-shadow-tracknumber-rollback", probe_mode::run_production_shadow_tracknumber_rollback },
        { "/metadata-isolation-probe-production-timing-index01-change", probe_mode::run_production_timing_index01_change },
        { "/metadata-isolation-probe-production-timing-index01-invalid", probe_mode::run_production_timing_index01_invalid },
        { "/metadata-isolation-probe-production-timing-pregap-add", probe_mode::run_production_timing_pregap_add },
        { "/metadata-isolation-probe-production-timing-pregap-change", probe_mode::run_production_timing_pregap_change },
        { "/metadata-isolation-probe-production-timing-pregap-delete", probe_mode::run_production_timing_pregap_delete },
        { "/metadata-isolation-probe-production-timing-index00-add", probe_mode::run_production_timing_index00_add },
        { "/metadata-isolation-probe-production-timing-index00-change", probe_mode::run_production_timing_index00_change },
        { "/metadata-isolation-probe-production-timing-index00-delete", probe_mode::run_production_timing_index00_delete },
        { "/metadata-isolation-probe-production-timing-index02-add", probe_mode::run_production_timing_index02_add },
        { "/metadata-isolation-probe-production-timing-index02-change", probe_mode::run_production_timing_index02_change },
        { "/metadata-isolation-probe-production-timing-index02-delete", probe_mode::run_production_timing_index02_delete },
        { "/metadata-isolation-probe-production-timing-batch", probe_mode::run_production_timing_batch },
        { "/metadata-isolation-probe-production-timing-rollback", probe_mode::run_production_timing_rollback },
        { "/metadata-isolation-probe-production-timing-boundary-lf", probe_mode::run_production_timing_boundary_lf },
        { "/metadata-isolation-probe-production-timing-spacing", probe_mode::run_production_timing_spacing },
        { "/metadata-isolation-probe-production-timing-terminal-add", probe_mode::run_production_timing_terminal_add },
        { "/metadata-isolation-probe-production-timing-begin", probe_mode::begin_production_timing },
        { "/metadata-isolation-probe-production-timing-finish", probe_mode::finish_production_timing },
        { "/metadata-isolation-probe-product-timing-dialog", probe_mode::run_product_timing_dialog },
        { "/metadata-isolation-probe-production-shadow-begin-tracknumber", probe_mode::begin_production_shadow_tracknumber },
        { "/metadata-isolation-probe-production-shadow-begin-replaygain", probe_mode::begin_production_shadow_replaygain },
        { "/metadata-isolation-probe-production-shadow-begin-replaygain-scan", probe_mode::begin_production_shadow_replaygain_scan },
        { "/metadata-isolation-probe-production-shadow-begin-title", probe_mode::begin_production_shadow_title },
        { "/metadata-isolation-probe-production-shadow-begin-partial-style", probe_mode::begin_production_shadow_partial_style },
        { "/metadata-isolation-probe-production-shadow-begin-discogger", probe_mode::begin_production_shadow_discogger },
        { "/metadata-isolation-probe-production-shadow-begin-discogger-mapping", probe_mode::begin_production_shadow_discogger_mapping },
        { "/metadata-isolation-probe-production-shadow-begin-discogger-configuration", probe_mode::begin_production_shadow_discogger_configuration },
        { "/metadata-isolation-probe-production-shadow-begin-musicbrainz", probe_mode::begin_production_shadow_musicbrainz },
        { "/metadata-isolation-probe-production-shadow-begin-masstagger", probe_mode::begin_production_shadow_masstagger },
        { "/metadata-isolation-probe-production-shadow-begin-masstagger-partial", probe_mode::begin_production_shadow_masstagger_partial },
        { "/metadata-isolation-probe-production-shadow-begin-masstagger-saved-cue", probe_mode::begin_production_shadow_masstagger_saved_cue },
        { "/metadata-isolation-probe-production-shadow-finish", probe_mode::finish_production_shadow },
        { "/metadata-isolation-probe-field-capability-lab", probe_mode::field_capability_lab },
        { "/metadata-isolation-probe-embedded-cue-inventory", probe_mode::embedded_cue_inventory },
        { "/metadata-isolation-probe-projection-benchmark", probe_mode::projection_benchmark },
        { "/metadata-isolation-probe-projection-benchmark-inventory", probe_mode::projection_benchmark_inventory },
        { "/metadata-isolation-probe-physical-audit-classifier", probe_mode::physical_audit_classifier },
        { "/metadata-isolation-probe-logging-contract", probe_mode::logging_contract },
        { "/metadata-isolation-probe-logging-outcomes", probe_mode::logging_outcomes },
        { "/metadata-isolation-probe-logging-standard", probe_mode::logging_standard },
        { "/metadata-isolation-probe-logging-verbose", probe_mode::logging_verbose },
        { "/metadata-isolation-probe-open-preferences", probe_mode::open_preferences },
        { "/metadata-isolation-probe-tooltip-contract", probe_mode::tooltip_contract },
        { "/metadata-isolation-probe-tooltip-contract-dark", probe_mode::tooltip_contract_dark },
        { "/metadata-isolation-probe-open-preferences-dark", probe_mode::open_preferences_dark },
        { "/metadata-isolation-probe-open-display-preferences", probe_mode::open_display_preferences },
        { "/metadata-isolation-probe-disable-guard", probe_mode::disable_guard },
        { "/metadata-isolation-probe-enable-guard", probe_mode::enable_guard },
    };
    for (const auto& item : mappings) {
        if (pfc::stricmp_ascii(token, item.token) == 0) {
            mode = item.mode;
            return true;
        }
    }
    return false;
}

bool is_timing_execution_mode(probe_mode mode) {
    switch (mode) {
    case probe_mode::run_production_timing_index01_change:
    case probe_mode::run_production_timing_index01_invalid:
    case probe_mode::run_production_timing_pregap_add:
    case probe_mode::run_production_timing_pregap_change:
    case probe_mode::run_production_timing_pregap_delete:
    case probe_mode::run_production_timing_index00_add:
    case probe_mode::run_production_timing_index00_change:
    case probe_mode::run_production_timing_index00_delete:
    case probe_mode::run_production_timing_index02_add:
    case probe_mode::run_production_timing_index02_change:
    case probe_mode::run_production_timing_index02_delete:
    case probe_mode::run_production_timing_batch:
    case probe_mode::run_production_timing_rollback:
    case probe_mode::run_production_timing_boundary_lf:
    case probe_mode::run_production_timing_spacing:
    case probe_mode::run_production_timing_terminal_add:
    case probe_mode::begin_production_timing:
        return true;
    default:
        return false;
    }
}

cue_timing_edit timing_edit_for_mode(probe_mode mode) {
    cue_timing_edit edit;
    switch (mode) {
    case probe_mode::run_production_timing_index01_invalid:
        edit = { 1, cue_timing_directive::index, 1, cue_timing_edit_kind::replace, 10, 13 };
        break;
    case probe_mode::run_production_timing_pregap_add:
        edit = { 3, cue_timing_directive::pregap, 0, cue_timing_edit_kind::insert, 0, 5 };
        break;
    case probe_mode::run_production_timing_pregap_change:
        edit = { 2, cue_timing_directive::pregap, 0, cue_timing_edit_kind::replace, 5, 7 };
        break;
    case probe_mode::run_production_timing_pregap_delete:
        edit = { 2, cue_timing_directive::pregap, 0, cue_timing_edit_kind::remove, 5, 0 };
        break;
    case probe_mode::run_production_timing_index00_add:
        edit = { 3, cue_timing_directive::index, 0, cue_timing_edit_kind::insert, 0, 40 };
        break;
    case probe_mode::run_production_timing_index00_change:
        edit = { 1, cue_timing_directive::index, 0, cue_timing_edit_kind::replace, 0, 2 };
        break;
    case probe_mode::run_production_timing_index00_delete:
        edit = { 1, cue_timing_directive::index, 0, cue_timing_edit_kind::remove, 0, 0 };
        break;
    case probe_mode::run_production_timing_index02_add:
        edit = { 2, cue_timing_directive::index, 2, cue_timing_edit_kind::insert, 0, 40 };
        break;
    case probe_mode::run_production_timing_index02_change:
        edit = { 1, cue_timing_directive::index, 2, cue_timing_edit_kind::replace, 12, 14 };
        break;
    case probe_mode::run_production_timing_index02_delete:
        edit = { 1, cue_timing_directive::index, 2, cue_timing_edit_kind::remove, 12, 0 };
        break;
    case probe_mode::run_production_timing_boundary_lf:
        edit = { 100, cue_timing_directive::index, 1, cue_timing_edit_kind::replace, 15, 16 };
        break;
    case probe_mode::run_production_timing_spacing:
        edit = { 3, cue_timing_directive::index, 1, cue_timing_edit_kind::replace, 15, 16 };
        break;
    case probe_mode::run_production_timing_terminal_add:
        edit = { 7, cue_timing_directive::index, 2, cue_timing_edit_kind::insert, 0, 35 };
        break;
    default:
        edit = { 2, cue_timing_directive::index, 1, cue_timing_edit_kind::replace, 30, 35 };
        break;
    }
    return edit;
}

void write_report(const char* source_path, const pfc::string8& report) {
    pfc::string_formatter report_path;
    report_path << source_path << ".metadata-isolation-probe.txt";
    file::ptr output;
    filesystem::g_open_write_new(output, report_path, fb2k::noAbort);
    pfc::string8 complete(report);
    for (const auto& line : snapshot_operation_log_capture_for_test()) {
        complete << "structured_log=" << line.c_str() << "\n";
    }
    output->write(complete.c_str(), complete.length(), fb2k::noAbort);
}

void write_edit_callback_report(const char* source_path, const pfc::string8& report) {
    pfc::string_formatter report_path;
    report_path << source_path << ".metadata-isolation-edit-callback.txt";
    file::ptr output;
    filesystem::g_open_write_new(output, report_path, fb2k::noAbort);
    output->write(report.c_str(), report.length(), fb2k::noAbort);
}

void run_physical_audit_classifier_probe(const char* report_source_path) {
    static const std::string cue =
        "PERFORMER \"Audit Artist\"\r\n"
        "TITLE \"Audit Album\"\r\n"
        "REM DATE 2026\r\n"
        "REM GENRE Trance\r\n"
        "FILE \"dummy.wav\" WAVE\r\n"
        "  TRACK 01 AUDIO\r\n"
        "    TITLE \"Track One\"\r\n"
        "    PERFORMER \"Track Artist\"\r\n"
        "    INDEX 01 00:00:00\r\n";

    const auto fill_complete = [](file_info_impl& info) {
        info.meta_set("ALBUM ARTIST", "Audit Artist");
        info.meta_set("ALBUM", "Audit Album");
        info.meta_set("DATE", "2026");
        info.meta_set("GENRE", "Trance");
    };
    const auto append_case = [](pfc::string_base& report, const char* name,
                                const physical_audit_assessment& result) {
        report << name << "_status=" << physical_audit_status_name(result.status) << "\n"
               << name << "_missing=" << result.missing_fields.c_str() << "\n"
               << name << "_differences=" << result.cue_differences.c_str() << "\n"
               << name << "_review=" << result.review_reason.c_str() << "\n";
    };

    pfc::string_formatter report;

    file_info_impl complete;
    fill_complete(complete);
    append_case(report, "ok", assess_physical_metadata(complete, cue));

    file_info_impl artist_fallback;
    artist_fallback.meta_set("ARTIST", "Audit Artist");
    artist_fallback.meta_set("ALBUM", "Audit Album");
    artist_fallback.meta_set("DATE", "2026");
    artist_fallback.meta_set("GENRE", "Trance");
    append_case(report, "artist_fallback", assess_physical_metadata(artist_fallback, cue));

    file_info_impl empty;
    append_case(report, "empty", assess_physical_metadata(empty, cue));

    file_info_impl incomplete;
    fill_complete(incomplete);
    incomplete.meta_remove_field("GENRE");
    append_case(report, "incomplete", assess_physical_metadata(incomplete, cue));

    file_info_impl whitespace;
    fill_complete(whitespace);
    whitespace.meta_set("DATE", "   ");
    append_case(report, "whitespace", assess_physical_metadata(whitespace, cue));

    file_info_impl mismatch;
    fill_complete(mismatch);
    mismatch.meta_set("GENRE", "House");
    append_case(report, "mismatch", assess_physical_metadata(mismatch, cue));

    file_info_impl review;
    fill_complete(review);
    const size_t genre = review.meta_set("GENRE", "Trance");
    review.meta_add_value(genre, "House");
    append_case(report, "review", assess_physical_metadata(review, cue));

    report << "write_result=read_only_classifier\n";
    write_report(report_source_path, report);
}

const char* meta_value_or_missing(const file_info& info, const char* name) {
    const char* value = info.meta_get(name, 0);
    return value != nullptr ? value : "<missing>";
}

std::string read_small_text_file(const char* path) {
    file::ptr input;
    filesystem::g_open_read(input, path, fb2k::noAbort);
    const t_filesize size64 = input->get_size_ex(fb2k::noAbort);
    if (size64 > 64 * 1024) throw pfc::exception("Capability-lab plan exceeds 64 KiB");
    const size_t size = pfc::downcast_guarded<size_t>(size64);
    std::string output(size, '\0');
    if (size > 0) input->read_object(output.data(), size, fb2k::noAbort);
    return output;
}

struct capability_lab_plan {
    std::string field;
    std::string operation;
    std::string scope = "virtual";
    std::string value = "Capability Lab Value";
    std::string value2 = "Capability Lab Value Two";
    std::string value3 = "Capability Lab Value Three";
};

capability_lab_plan read_capability_lab_plan(const char* source_path) {
    pfc::string_formatter plan_path;
    plan_path << source_path << ".metadata-isolation-lab-plan.txt";
    const std::string text = read_small_text_file(plan_path.c_str());
    std::map<std::string, std::string> values;
    size_t begin = 0;
    while (begin <= text.size()) {
        const size_t end = text.find_first_of("\r\n", begin);
        const std::string line = text.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
        if (!line.empty() && line[0] != '#') {
            const size_t separator = line.find('=');
            if (separator == std::string::npos || separator == 0) {
                throw pfc::exception("Invalid capability-lab plan line");
            }
            values[line.substr(0, separator)] = line.substr(separator + 1);
        }
        if (end == std::string::npos) break;
        begin = end + 1;
        if (begin < text.size() && text[end] == '\r' && text[begin] == '\n') ++begin;
    }
    capability_lab_plan plan;
    plan.field = values["field"];
    plan.operation = values["operation"];
    if (values.count("scope")) plan.scope = values["scope"];
    if (values.count("value")) plan.value = values["value"];
    if (values.count("value2")) plan.value2 = values["value2"];
    if (values.count("value3")) plan.value3 = values["value3"];
    if (plan.field.empty() || plan.operation.empty()) {
        throw pfc::exception("Capability-lab plan requires field and operation");
    }
    return plan;
}

void append_hex(pfc::string_base& output, const char* label, const char* value) {
    static constexpr char digits[] = "0123456789abcdef";
    output << label << "=";
    if (value != nullptr) {
        for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(value); *cursor; ++cursor) {
            char encoded[3] = { digits[*cursor >> 4], digits[*cursor & 15], 0 };
            output << encoded;
        }
    }
    output << "\n";
}

void append_info_inventory(
    pfc::string_base& output,
    const char* phase,
    unsigned subsong,
    const file_info& info) {
    pfc::string_formatter prefix;
    prefix << phase << "_track_" << subsong;
    uint64_t length_bits = 0;
    const double length = info.get_length();
    static_assert(sizeof(length_bits) == sizeof(length));
    std::memcpy(&length_bits, &length, sizeof(length_bits));
    output << prefix << "_length=" << pfc::format_float(length, 0, 15) << "\n"
           << prefix << "_length_bits=" << pfc::format_hex(length_bits, 16) << "\n"
           << prefix << "_field_count=" << info.meta_get_count() << "\n"
           << prefix << "_info_count=" << info.info_get_count() << "\n";
    for (size_t item = 0; item < info.info_get_count(); ++item) {
        pfc::string_formatter label;
        label << prefix << "_info_" << item << "_name_hex";
        append_hex(output, label.c_str(), info.info_enum_name(item));
        label.reset();
        label << prefix << "_info_" << item << "_value_hex";
        append_hex(output, label.c_str(), info.info_enum_value(item));
    }
    for (size_t field = 0; field < info.meta_get_count(); ++field) {
        pfc::string_formatter label;
        label << prefix << "_field_" << field << "_name_hex";
        append_hex(output, label.c_str(), info.meta_enum_name(field));
        output << prefix << "_field_" << field << "_value_count="
               << info.meta_enum_value_count(field) << "\n";
        for (size_t value = 0; value < info.meta_enum_value_count(field); ++value) {
            label.reset();
            label << prefix << "_field_" << field << "_value_" << value << "_hex";
            append_hex(output, label.c_str(), info.meta_enum_value(field, value));
        }
    }
    const replaygain_info rg = info.get_replaygain();
    const auto append_rg = [&output, &prefix](
        const char* name,
        float value,
        float invalid,
        bool gain) {
        uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        replaygain_info::t_text_buffer formatted = {};
        const bool present = value != invalid;
        if (present) {
            if (gain) replaygain_info::g_format_gain(value, formatted);
            else replaygain_info::g_format_peak(value, formatted);
        }
        output << prefix << "_rg_" << name << "_present="
               << (present ? "true" : "false") << "\n"
               << prefix << "_rg_" << name << "_bits="
               << pfc::format_hex(bits, 8) << "\n";
        pfc::string_formatter label;
        label << prefix << "_rg_" << name << "_formatted_hex";
        append_hex(output, label.c_str(), formatted);
    };
    append_rg("track_gain", rg.m_track_gain, replaygain_info::gain_invalid, true);
    append_rg("track_peak", rg.m_track_peak, replaygain_info::peak_invalid, false);
    append_rg("album_gain", rg.m_album_gain, replaygain_info::gain_invalid, true);
    append_rg("album_peak", rg.m_album_peak, replaygain_info::peak_invalid, false);
}

std::vector<unsigned> virtual_subsongs(const char* path) {
    input_info_reader::ptr reader;
    input_entry::g_open_for_info_read(reader, nullptr, path, fb2k::noAbort);
    std::vector<unsigned> output;
    for (unsigned index = 0; index < reader->get_subsong_count(); ++index) {
        const unsigned subsong = reader->get_subsong(index);
        if (subsong != 0) output.push_back(subsong);
    }
    return output;
}

file_info_impl read_subsong_info(const char* path, unsigned subsong) {
    input_info_reader::ptr reader;
    input_entry::g_open_for_info_read(reader, nullptr, path, fb2k::noAbort);
    file_info_impl output;
    reader->get_info(subsong, output, fb2k::noAbort);
    return output;
}

void apply_capability_operation(
    file_info_impl& info,
    const capability_lab_plan& plan,
    const char* value) {
    if (replaygain_info::g_is_meta_replaygain(plan.field.c_str())) {
        if (plan.operation == "delete" || plan.operation == "delete_all") {
            replaygain_info rg = info.get_replaygain();
            if (pfc::stricmp_ascii(plan.field.c_str(), "REPLAYGAIN_TRACK_GAIN") == 0) rg.remove_track_gain();
            else if (pfc::stricmp_ascii(plan.field.c_str(), "REPLAYGAIN_TRACK_PEAK") == 0) rg.remove_track_peak();
            else if (pfc::stricmp_ascii(plan.field.c_str(), "REPLAYGAIN_ALBUM_GAIN") == 0) rg.remove_album_gain();
            else if (pfc::stricmp_ascii(plan.field.c_str(), "REPLAYGAIN_ALBUM_PEAK") == 0) rg.remove_album_peak();
            info.set_replaygain(rg);
        } else {
            info.info_set_replaygain(plan.field.c_str(),
                plan.operation == "empty" ? "" : value);
        }
        return;
    }
    if (plan.operation == "delete" || plan.operation == "delete_all") {
        info.meta_remove_field(plan.field.c_str());
    } else if (plan.operation == "empty") {
        info.meta_set(plan.field.c_str(), "");
    } else if (plan.operation == "multi") {
        const size_t index = info.meta_set(plan.field.c_str(), value);
        info.meta_add_value(index, plan.value2.c_str());
    } else if (plan.operation != "read") {
        info.meta_set(plan.field.c_str(), value);
    }
}

void run_one_field_capability_case(const char* path) {
    pfc::string8 report;
    try {
        const capability_lab_plan plan = read_capability_lab_plan(path);
        report << "lab_schema=1\n";
        append_hex(report, "lab_field_hex", plan.field.c_str());
        report << "lab_operation=" << plan.operation.c_str() << "\n"
               << "lab_scope=" << plan.scope.c_str() << "\n";
        const std::vector<unsigned> subsongs = virtual_subsongs(path);
        report << "lab_track_count=" << subsongs.size() << "\n";
        for (unsigned subsong : subsongs) {
            append_info_inventory(report, "before", subsong, read_subsong_info(path, subsong));
        }
        const file_info_impl physical_before = read_subsong_info(path, 0);
        append_info_inventory(report, "physical_before", 0, physical_before);
        append_hex(report, "cue_before_hex", physical_before.meta_get("CUESHEET", 0));

        if (plan.operation != "read") {
            const bool previous_guard = cfg_enabled;
            struct guard_restore {
                bool value;
                ~guard_restore() { cfg_enabled = value; }
            } restore{ previous_guard };
            cfg_enabled = false;
            auto lock = file_lock_manager::get()->acquire_write(path, fb2k::noAbort);
            input_info_writer::ptr writer;
            input_entry::g_open_for_info_write_timeout(
                writer, nullptr, path, fb2k::noAbort, 2.0);
            const bool physical_scope = plan.scope == "physical";
            const bool all_tracks = plan.operation == "same_all" ||
                plan.operation == "different_all" || plan.operation == "delete_all";
            const size_t targets = physical_scope ? 1 :
                all_tracks ? subsongs.size() : std::min<size_t>(1, subsongs.size());
            for (size_t index = 0; index < targets; ++index) {
                file_info_impl info;
                const unsigned target_subsong = physical_scope ? 0 : subsongs[index];
                writer->get_info(target_subsong, info, fb2k::noAbort);
                pfc::string_formatter value;
                if (plan.operation == "different_all" &&
                    replaygain_info::g_is_meta_replaygain(plan.field.c_str())) {
                    value << (index == 0 ? plan.value.c_str() :
                        index == 1 ? plan.value2.c_str() : plan.value3.c_str());
                } else {
                    value << plan.value.c_str();
                    if (plan.operation == "different_all") value << " " << (index + 1);
                }
                apply_capability_operation(info, plan, value.c_str());
                writer->set_info(target_subsong, info, fb2k::noAbort);
            }
            writer->commit(fb2k::noAbort);
            writer.release();
            report << "lab_write_result=committed\n";
        } else {
            report << "lab_write_result=read_only\n";
        }

        for (unsigned subsong : subsongs) {
            append_info_inventory(report, "after", subsong, read_subsong_info(path, subsong));
        }
        const file_info_impl physical_after = read_subsong_info(path, 0);
        append_info_inventory(report, "physical_after", 0, physical_after);
        append_hex(report, "cue_after_hex", physical_after.meta_get("CUESHEET", 0));
    } catch (const std::exception& error) {
        report << "lab_write_result=error\nerror=" << error.what() << "\n";
    }
    write_report(path, report);
}

void run_field_capability_lab(metadb_handle_list_cref items) {
    std::vector<std::string> paths;
    for (const auto& item : items) {
        bool found = false;
        for (const std::string& existing : paths) {
            if (metadb::path_compare(existing.c_str(), item->get_path()) == 0) {
                found = true;
                break;
            }
        }
        if (!found) paths.emplace_back(item->get_path());
    }
    for (const std::string& path : paths) run_one_field_capability_case(path.c_str());
}

void append_context_tree(contextmenu_node* node, pfc::string_base& report) {
    if (node == nullptr) return;
    if (node->get_type() == contextmenu_item_node::TYPE_COMMAND) {
        pfc::string8 full_name, description;
        node->get_full_name(full_name);
        if (full_name.is_empty()) full_name = node->get_name();
        node->get_description(description);
        report << "context_command=" << full_name << "\n";
        report << "context_command_flags=" << node->get_display_flags() << "\n";
        report << "context_command_description=" << description << "\n";
        return;
    }
    if (node->get_type() != contextmenu_item_node::TYPE_POPUP) return;
    for (size_t index = 0; index < node->get_num_children(); ++index) {
        append_context_tree(node->get_child(index), report);
    }
}

contextmenu_node* find_context_command(contextmenu_node* node, const char* name) {
    if (node == nullptr) return nullptr;
    if (node->get_type() == contextmenu_item_node::TYPE_COMMAND) {
        return pfc::stricmp_ascii(node->get_name(), name) == 0 ? node : nullptr;
    }
    if (node->get_type() != contextmenu_item_node::TYPE_POPUP) return nullptr;
    for (size_t index = 0; index < node->get_num_children(); ++index) {
        if (contextmenu_node* found = find_context_command(node->get_child(index), name)) {
            return found;
        }
    }
    return nullptr;
}

class probe_commandline_handler : public commandline_handler_metadb_handle {
public:
    result on_token(const char* token) override {
        if (!parse_probe_mode(token, m_mode)) return RESULT_NOT_OURS;
        return RESULT_PROCESSED_EXPECT_FILES;
    }

    void on_file(const metadb_handle_ptr& item) override {
        m_items.add_item(item);
    }

    void on_files_done() override {
        if (m_items.get_count() == 0) return;

        clear_operation_log_capture_for_test();

        if (m_mode == probe_mode::field_capability_lab) {
            run_field_capability_lab(m_items);
            return;
        }

        const char* path = m_items[0]->get_path();
        pfc::string_formatter report;
        report << "handle_count=" << m_items.get_count() << "\n";
        for (size_t index = 0; index < m_items.get_count(); ++index) {
            report << "handle_" << index << "_subsong="
                   << m_items[index]->get_subsong_index() << "\n";
        }

        if (m_mode == probe_mode::physical_audit_classifier) {
            run_physical_audit_classifier_probe(path);
            return;
        }

        try {
            input_manager_v3::ptr manager = input_manager_v3::get();

            input_info_reader::ptr regular;
            regular ^= manager->open_v3(
                input_info_reader::class_guid,
                nullptr,
                path,
                0,
                nullptr,
                fb2k::noAbort);
            report << "regular_reader_subsongs=" << regular->get_subsong_count() << "\n";

            input_info_reader::ptr suppressed;
            suppressed ^= manager->open_v3(
                input_info_reader::class_guid,
                nullptr,
                path,
                input_manager_v3::flagSuppressFilters,
                nullptr,
                fb2k::noAbort);
            report << "suppress_filters_reader_subsongs=" << suppressed->get_subsong_count() << "\n";
            regular.release();
            suppressed.release();

            if (m_mode == probe_mode::embedded_cue_inventory) {
                input_info_reader::ptr inventory;
                input_entry::g_open_for_info_read(
                    inventory, nullptr, path, fb2k::noAbort);
                report << "inventory_subsong_count=" << inventory->get_subsong_count() << "\n";
                for (unsigned index = 0; index < inventory->get_subsong_count(); ++index) {
                    const unsigned subsong = inventory->get_subsong(index);
                    report << "inventory_subsong_" << index << "=" << subsong << "\n";
                    file_info_impl info;
                    inventory->get_info(subsong, info, fb2k::noAbort);
                    append_info_inventory(report, "inventory", subsong, info);
                }
                chapterizer::ptr chapters;
                if (chapterizer::g_find(chapters, path)) {
                    chapter_list_impl list;
                    chapters->get_chapters(path, list, fb2k::noAbort);
                    report << "chapterizer_present=true\n"
                           << "chapterizer_supports_pregaps="
                           << (chapters->supports_pregaps() ? "true" : "false") << "\n"
                           << "chapterizer_count=" << list.get_chapter_count() << "\n"
                           << "chapterizer_pregap="
                           << pfc::format_float(list.get_pregap(), 0, 15) << "\n";
                    for (size_t index = 0; index < list.get_chapter_count(); ++index) {
                        report << "chapterizer_" << index << "_length="
                               << pfc::format_float(list.get_info(index).get_length(), 0, 15)
                               << "\n";
                    }
                } else {
                    report << "chapterizer_present=false\n";
                }
                report << "write_result=inventory_only\n";
            } else if (m_mode == probe_mode::projection_benchmark ||
                m_mode == probe_mode::projection_benchmark_inventory) {
                metadb_handle_list virtual_tracks;
                std::set<std::string> seen_paths;
                for (const auto& source : m_items) {
                    if (!seen_paths.insert(source->get_path()).second) continue;
                    input_info_reader::ptr inventory;
                    input_entry::g_open_for_info_read(
                        inventory, nullptr, source->get_path(), fb2k::noAbort);
                    for (unsigned index = 0; index < inventory->get_subsong_count(); ++index) {
                        const unsigned subsong = inventory->get_subsong(index);
                        if (subsong == 0) continue;
                        virtual_tracks.add_item(metadb::get()->handle_create(
                            source->get_path(), subsong));
                        if (m_mode == probe_mode::projection_benchmark_inventory) break;
                    }
                }
                pfc::string8 diagnostic;
                const bool result = run_virtual_projection_benchmark_diagnostic(
                    virtual_tracks,
                    m_mode == probe_mode::projection_benchmark,
                    diagnostic);
                report << "production_virtual_track_count=" << virtual_tracks.get_count() << "\n"
                       << diagnostic
                       << "write_result=" << (result ? "benchmark_pass" : "benchmark_fail") << "\n";
            } else if (m_mode == probe_mode::run_product_timing_dialog) {
                metadb_handle_list selection;
                selection.add_item(metadb::get()->handle_create(path, 2));
                edit_virtual_timing(selection);
                report << "write_result=product_timing_dialog_closed\n";
            } else if (m_mode == probe_mode::finish_production_timing) {
                pfc::string8 diagnostic;
                const bool committed = finish_virtual_timing_diagnostic(true, diagnostic);
                report << diagnostic
                       << "write_result=" << (committed ? "committed" : "not_committed") << "\n";
            } else if (is_timing_execution_mode(m_mode)) {
                const cue_timing_edit edit = timing_edit_for_mode(m_mode);
                std::vector<virtual_timing_edit_request> requests;
                std::set<std::string> seen_paths;
                for (const auto& source : m_items) {
                    const std::string source_path(source->get_path());
                    if (!seen_paths.insert(source_path).second) continue;
                    requests.push_back({
                        metadb::get()->handle_create(source_path.c_str(), edit.track_number),
                        edit,
                    });
                }
                pfc::string8 diagnostic;
                const bool started = m_mode == probe_mode::begin_production_timing;
                const bool result = started
                    ? begin_virtual_timing_diagnostic(requests, diagnostic)
                    : run_virtual_timing_diagnostic(
                        requests,
                        true,
                        diagnostic,
                        m_mode == probe_mode::run_production_timing_rollback);
                report << "production_timing_document_count=" << requests.size() << "\n"
                       << diagnostic
                       << "write_result="
                       << (result ? (started ? "started" : "committed") : "not_committed")
                       << "\n";
            } else if (m_mode == probe_mode::run_production_shadow_title ||
                m_mode == probe_mode::run_production_shadow_nochange ||
                m_mode == probe_mode::run_production_shadow_title_partial ||
                m_mode == probe_mode::run_production_shadow_title_artist ||
                m_mode == probe_mode::run_production_shadow_custom ||
                m_mode == probe_mode::run_production_shadow_mixed ||
                m_mode == probe_mode::run_production_shadow_delete ||
                m_mode == probe_mode::run_production_shadow_multivalue ||
                m_mode == probe_mode::run_production_shadow_native_text ||
                m_mode == probe_mode::run_production_shadow_latin1_text ||
                m_mode == probe_mode::run_production_shadow_unicode_text ||
                m_mode == probe_mode::run_production_shadow_long_text ||
                m_mode == probe_mode::run_production_shadow_unsafe_text ||
                m_mode == probe_mode::run_production_shadow_phase10_unicode ||
                m_mode == probe_mode::run_production_shadow_phase10_unicode_partial_style ||
                m_mode == probe_mode::run_production_shadow_phase10_unicode_rollback ||
                m_mode == probe_mode::run_production_shadow_global ||
                m_mode == probe_mode::run_production_shadow_global_partial ||
                m_mode == probe_mode::run_production_shadow_global_different ||
                m_mode == probe_mode::run_production_shadow_global_inherited ||
                 m_mode == probe_mode::run_production_shadow_global_inherited_partial ||
                 m_mode == probe_mode::run_production_shadow_global_inherited_uncoupled ||
                 m_mode == probe_mode::run_production_shadow_add ||
                 m_mode == probe_mode::run_production_shadow_delete_fields ||
                 m_mode == probe_mode::run_production_shadow_remove_overrides ||
                 m_mode == probe_mode::run_production_shadow_global_add ||
                 m_mode == probe_mode::run_production_shadow_global_delete ||
                 m_mode == probe_mode::run_production_shadow_add_mixed ||
                 m_mode == probe_mode::run_production_shadow_remove_text_overrides ||
                 m_mode == probe_mode::run_production_shadow_global_delete_partial ||
                 m_mode == probe_mode::run_production_shadow_batch_atomic ||
                 m_mode == probe_mode::run_production_shadow_delete_inherited_globals ||
                 m_mode == probe_mode::run_production_shadow_add_partial ||
                 m_mode == probe_mode::run_production_shadow_delete_fields_partial ||
                 m_mode == probe_mode::run_production_shadow_rollback ||
                 m_mode == probe_mode::run_production_shadow_partial_style ||
                 m_mode == probe_mode::run_production_shadow_partial_style_strict ||
                 m_mode == probe_mode::run_production_shadow_partial_style_blocking ||
                 m_mode == probe_mode::run_production_shadow_partial_style_rollback ||
                 m_mode == probe_mode::run_production_shadow_style_only ||
                 m_mode == probe_mode::run_production_shadow_style_delete ||
                 m_mode == probe_mode::run_production_shadow_replaygain_style ||
                 m_mode == probe_mode::run_production_shadow_replaygain ||
                 m_mode == probe_mode::run_production_shadow_replaygain_delete ||
                 m_mode == probe_mode::run_production_shadow_replaygain_native_remove ||
                 m_mode == probe_mode::run_production_shadow_replaygain_partial ||
                 m_mode == probe_mode::run_production_shadow_replaygain_delete_partial ||
                 m_mode == probe_mode::run_production_shadow_replaygain_mixed ||
                 m_mode == probe_mode::run_production_shadow_replaygain_atomic ||
                 m_mode == probe_mode::run_production_shadow_replaygain_rollback ||
                 m_mode == probe_mode::run_production_shadow_tracknumber_valid ||
                 m_mode == probe_mode::run_production_shadow_tracknumber_shift ||
                 m_mode == probe_mode::run_production_shadow_tracknumber_conflict ||
                 m_mode == probe_mode::run_production_shadow_tracknumber_reverse ||
                 m_mode == probe_mode::run_production_shadow_tracknumber_alphanumeric ||
                 m_mode == probe_mode::run_production_shadow_tracknumber_delete ||
                 m_mode == probe_mode::run_production_shadow_tracknumber_atomic ||
                 m_mode == probe_mode::run_production_shadow_tracknumber_mixed ||
                 m_mode == probe_mode::run_production_shadow_tracknumber_partial ||
                 m_mode == probe_mode::run_production_shadow_tracknumber_partial_reorder ||
                 m_mode == probe_mode::run_production_shadow_tracknumber_playlist ||
                 m_mode == probe_mode::run_production_shadow_tracknumber_playlist_locked ||
                 m_mode == probe_mode::run_production_shadow_tracknumber_rollback ||
                 m_mode == probe_mode::begin_production_shadow_tracknumber ||
                 m_mode == probe_mode::begin_production_shadow_replaygain ||
                 m_mode == probe_mode::begin_production_shadow_replaygain_scan ||
                 m_mode == probe_mode::begin_production_shadow_title ||
                 m_mode == probe_mode::begin_production_shadow_partial_style ||
                 m_mode == probe_mode::begin_production_shadow_discogger ||
                 m_mode == probe_mode::begin_production_shadow_discogger_mapping ||
                 m_mode == probe_mode::begin_production_shadow_discogger_configuration ||
                 m_mode == probe_mode::begin_production_shadow_musicbrainz ||
                 m_mode == probe_mode::begin_production_shadow_masstagger ||
                 m_mode == probe_mode::begin_production_shadow_masstagger_partial ||
                 m_mode == probe_mode::begin_production_shadow_masstagger_saved_cue) {
                metadb_handle_list virtual_tracks;
                for (const auto& source : m_items) {
                    if (source->get_subsong_index() != 0) {
                        if (m_mode == probe_mode::run_production_shadow_tracknumber_partial ||
                            m_mode == probe_mode::run_production_shadow_tracknumber_partial_reorder ||
                            m_mode == probe_mode::run_production_shadow_tracknumber_playlist ||
                            m_mode == probe_mode::run_production_shadow_tracknumber_playlist_locked ||
                            m_mode == probe_mode::begin_production_shadow_masstagger_partial) {
                            if (source->get_subsong_index() == 3) virtual_tracks.add_item(source);
                            continue;
                        }
                        if ((m_mode != probe_mode::run_production_shadow_title_partial &&
                             m_mode != probe_mode::run_production_shadow_phase10_unicode_partial_style &&
                             m_mode != probe_mode::run_production_shadow_global_partial &&
                             m_mode != probe_mode::run_production_shadow_global_inherited_partial &&
                             m_mode != probe_mode::run_production_shadow_global_delete_partial &&
                             m_mode != probe_mode::run_production_shadow_add_partial &&
                             m_mode != probe_mode::run_production_shadow_delete_fields_partial &&
                             m_mode != probe_mode::run_production_shadow_replaygain_partial &&
                             m_mode != probe_mode::run_production_shadow_replaygain_delete_partial) ||
                            virtual_tracks.get_count() == 0) {
                            virtual_tracks.add_item(source);
                        }
                        continue;
                    }
                    input_info_reader::ptr reader;
                    input_entry::g_open_for_info_read(
                        reader, nullptr, source->get_path(), fb2k::noAbort);
                    for (unsigned index = 0; index < reader->get_subsong_count(); ++index) {
                        const unsigned subsong = reader->get_subsong(index);
                        if (subsong == 0) continue;
                        virtual_tracks.add_item(metadb::get()->handle_create(source->get_path(), subsong));
                        if (m_mode == probe_mode::run_production_shadow_title_partial ||
                            m_mode == probe_mode::run_production_shadow_phase10_unicode_partial_style ||
                            m_mode == probe_mode::run_production_shadow_global_partial ||
                            m_mode == probe_mode::run_production_shadow_global_inherited_partial ||
                            m_mode == probe_mode::run_production_shadow_global_delete_partial ||
                            m_mode == probe_mode::run_production_shadow_add_partial ||
                            m_mode == probe_mode::run_production_shadow_delete_fields_partial ||
                            m_mode == probe_mode::run_production_shadow_replaygain_partial ||
                            m_mode == probe_mode::run_production_shadow_replaygain_delete_partial) break;
                    }
                }
                const char* command = "";
                switch (m_mode) {
                case probe_mode::run_production_shadow_title:
                case probe_mode::run_production_shadow_title_partial:
                    command = "Research: Set projected shadow TITLE"; break;
                case probe_mode::run_production_shadow_nochange:
                    command = "Research: Leave projected shadows unchanged"; break;
                case probe_mode::run_production_shadow_title_artist:
                    command = "Research: Set projected shadow TITLE and ARTIST"; break;
                case probe_mode::run_production_shadow_custom:
                    command = "Research: Set projected shadow custom field"; break;
                case probe_mode::run_production_shadow_mixed:
                    command = "Research: Set projected shadow TITLE and custom field"; break;
                case probe_mode::run_production_shadow_delete:
                    command = "Research: Delete projected shadow TITLE"; break;
                case probe_mode::run_production_shadow_multivalue:
                    command = "Research: Set multivalue projected shadow TITLE"; break;
                case probe_mode::run_production_shadow_native_text:
                    command = "Research: Set projected shadow native Cue fields"; break;
                case probe_mode::run_production_shadow_latin1_text:
                    command = "Research: Set projected shadow Windows-1252 text"; break;
                case probe_mode::run_production_shadow_unicode_text:
                    command = "Research: Set projected shadow full Unicode text"; break;
                case probe_mode::run_production_shadow_long_text:
                    command = "Research: Set projected shadow long Unicode text"; break;
                case probe_mode::run_production_shadow_unsafe_text:
                    command = "Research: Set projected shadow unsafe Cue text"; break;
                case probe_mode::run_production_shadow_phase10_unicode:
                case probe_mode::run_production_shadow_phase10_unicode_rollback:
                    command = "Research: Apply phase 10 Unicode matrix"; break;
                case probe_mode::run_production_shadow_phase10_unicode_partial_style:
                    command = "Research: Apply phase 10 Unicode partial matrix"; break;
                case probe_mode::run_production_shadow_global:
                case probe_mode::run_production_shadow_global_partial:
                    command = "Research: Set projected shadow global Cue fields"; break;
                case probe_mode::run_production_shadow_global_different:
                    command = "Research: Set differing projected shadow ALBUM"; break;
                case probe_mode::run_production_shadow_global_inherited:
                case probe_mode::run_production_shadow_global_inherited_partial:
                    command = "Research: Set projected shadow inherited global Cue fields"; break;
                case probe_mode::run_production_shadow_global_inherited_uncoupled:
                    command = "Research: Set unrepresentable inherited projected shadow ALBUM ARTIST"; break;
                case probe_mode::run_production_shadow_add:
                    command = "Research: Add projected shadow Cue fields"; break;
                case probe_mode::run_production_shadow_delete_fields:
                    command = "Research: Delete projected shadow Cue fields"; break;
                case probe_mode::run_production_shadow_remove_overrides:
                    command = "Research: Set projected shadow fields to inherited values"; break;
                case probe_mode::run_production_shadow_global_add:
                    command = "Research: Add projected shadow global Cue fields"; break;
                case probe_mode::run_production_shadow_global_delete:
                    command = "Research: Delete projected shadow global Cue fields"; break;
                case probe_mode::run_production_shadow_add_mixed:
                    command = "Research: Add projected shadow TITLE and custom field"; break;
                case probe_mode::run_production_shadow_remove_text_overrides:
                    command = "Research: Set projected shadow text fields to inherited values"; break;
                case probe_mode::run_production_shadow_global_delete_partial:
                    command = "Research: Delete projected shadow global Cue fields"; break;
                case probe_mode::run_production_shadow_batch_atomic:
                    command = "Research: Batch atomic projected shadow add"; break;
                case probe_mode::run_production_shadow_delete_inherited_globals:
                    command = "Research: Delete projected shadow inherited global fields"; break;
                case probe_mode::run_production_shadow_add_partial:
                    command = "Research: Add projected shadow Cue fields"; break;
                case probe_mode::run_production_shadow_delete_fields_partial:
                    command = "Research: Delete projected shadow Cue fields"; break;
                case probe_mode::run_production_shadow_rollback:
                    command = "Research: Set projected shadow TITLE"; break;
                case probe_mode::run_production_shadow_partial_style:
                case probe_mode::run_production_shadow_partial_style_strict:
                case probe_mode::run_production_shadow_partial_style_rollback:
                    command = "Research: Set projected shadow TITLE and multivalue STYLE"; break;
                case probe_mode::run_production_shadow_partial_style_blocking:
                    command = "Research: Set projected shadow TITLE, multivalue STYLE and custom field"; break;
                case probe_mode::run_production_shadow_style_only:
                    command = "Research: Set projected shadow multivalue STYLE only"; break;
                case probe_mode::run_production_shadow_style_delete:
                    command = "Research: Delete projected shadow STYLE only"; break;
                case probe_mode::run_production_shadow_replaygain_style:
                    command = "Research: Set projected shadow ReplayGain and multivalue STYLE"; break;
                case probe_mode::run_production_shadow_replaygain:
                case probe_mode::run_production_shadow_replaygain_partial:
                case probe_mode::run_production_shadow_replaygain_rollback:
                case probe_mode::begin_production_shadow_replaygain:
                    command = "Research: Set projected shadow ReplayGain"; break;
                case probe_mode::begin_production_shadow_replaygain_scan:
                    command = "Scan per-file track gain"; break;
                case probe_mode::run_production_shadow_replaygain_delete:
                case probe_mode::run_production_shadow_replaygain_delete_partial:
                    command = "Research: Delete projected shadow ReplayGain"; break;
                case probe_mode::run_production_shadow_replaygain_native_remove:
                    command = "Remove ReplayGain information from files"; break;
                case probe_mode::run_production_shadow_replaygain_mixed:
                    command = "Research: Set projected shadow TITLE and ReplayGain"; break;
                case probe_mode::run_production_shadow_replaygain_atomic:
                    command = "Research: Set projected shadow ReplayGain and custom field"; break;
                case probe_mode::run_production_shadow_tracknumber_valid:
                    command = "Research: Renumber projected shadow TRACK midpoint"; break;
                case probe_mode::run_production_shadow_tracknumber_shift:
                    command = "Research: Shift projected shadow TRACK numbers"; break;
                case probe_mode::run_production_shadow_tracknumber_conflict:
                    command = "Research: Conflict projected shadow TRACK numbers"; break;
                case probe_mode::run_production_shadow_tracknumber_reverse:
                    command = "Research: Reverse projected shadow TRACK numbers"; break;
                case probe_mode::run_production_shadow_tracknumber_alphanumeric:
                    command = "Research: Set alphanumeric projected shadow TRACKNUMBER"; break;
                case probe_mode::run_production_shadow_tracknumber_delete:
                    command = "Research: Delete projected shadow TRACKNUMBER"; break;
                case probe_mode::run_production_shadow_tracknumber_atomic:
                    command = "Research: Set TITLE and conflicting projected shadow TRACK numbers"; break;
                case probe_mode::run_production_shadow_tracknumber_mixed:
                    command = "Research: Set TITLE and renumber projected shadow TRACK midpoint"; break;
                case probe_mode::run_production_shadow_tracknumber_partial:
                case probe_mode::run_production_shadow_tracknumber_playlist:
                case probe_mode::run_production_shadow_tracknumber_playlist_locked:
                    command = "Research: Partially renumber projected shadow TRACK midpoint"; break;
                case probe_mode::run_production_shadow_tracknumber_partial_reorder:
                    command = "Research: Partially reorder projected shadow TRACK midpoint"; break;
                case probe_mode::run_production_shadow_tracknumber_rollback:
                case probe_mode::begin_production_shadow_tracknumber:
                    command = "Research: Renumber projected shadow TRACK midpoint"; break;
                case probe_mode::begin_production_shadow_title:
                    command = "Research: Set projected shadow TITLE"; break;
                case probe_mode::begin_production_shadow_partial_style:
                    command = "Research: Set projected shadow TITLE and multivalue STYLE"; break;
                case probe_mode::begin_production_shadow_discogger:
                    command = "Write tags..."; break;
                case probe_mode::begin_production_shadow_discogger_mapping:
                    command = "Edit tag mappings..."; break;
                case probe_mode::begin_production_shadow_discogger_configuration:
                    command = "Configuration..."; break;
                case probe_mode::begin_production_shadow_musicbrainz:
                    command = "Get tags from MusicBrainz (by MB album ID)"; break;
                case probe_mode::begin_production_shadow_masstagger:
                case probe_mode::begin_production_shadow_masstagger_partial:
                    command = "Manage scripts"; break;
                case probe_mode::begin_production_shadow_masstagger_saved_cue:
                    command = "Discogs - Cue File (ohne Remix-Tag)"; break;
                default:
                    throw pfc::exception("Invalid production-shadow probe mode");
                }
                size_t diagnostic_playlist = SIZE_MAX;
                service_ptr_t<playlist_lock> diagnostic_lock;
                if (m_mode == probe_mode::run_production_shadow_tracknumber_playlist ||
                    m_mode == probe_mode::run_production_shadow_tracknumber_playlist_locked) {
                    playlist_manager::ptr playlists = playlist_manager::get();
                    diagnostic_playlist = playlists->create_playlist(
                        "Metadata Isolation structural handle probe",
                        SIZE_MAX,
                        SIZE_MAX);
                    if (diagnostic_playlist == SIZE_MAX) {
                        throw pfc::exception("Could not create structural handle probe playlist");
                    }
                    metadb_handle_list playlist_items;
                    const char* source_path = virtual_tracks[0]->get_path();
                    for (unsigned subsong : { 7u, 3u, 3u, 1u }) {
                        playlist_items.add_item(metadb::get()->handle_create(source_path, subsong));
                    }
                    if (playlists->playlist_insert_items(
                            diagnostic_playlist,
                            0,
                            playlist_items,
                            pfc::bit_array_one(1)) == SIZE_MAX) {
                        playlists->remove_playlist(diagnostic_playlist);
                        diagnostic_playlist = SIZE_MAX;
                        throw pfc::exception("Could not populate structural handle probe playlist");
                    }
                    playlists->playlist_set_focus_item(diagnostic_playlist, 2);
                    if (m_mode == probe_mode::run_production_shadow_tracknumber_playlist_locked) {
                        diagnostic_lock = new service_impl_t<diagnostic_playlist_lock>();
                        if (!playlists->playlist_lock_install(
                                diagnostic_playlist,
                                diagnostic_lock)) {
                            playlists->remove_playlist(diagnostic_playlist);
                            diagnostic_playlist = SIZE_MAX;
                            throw pfc::exception("Could not install structural rollback probe playlist lock");
                        }
                    }
                }

                pfc::string8 diagnostic;
                const bool persistent =
                    m_mode == probe_mode::begin_production_shadow_replaygain ||
                    m_mode == probe_mode::begin_production_shadow_replaygain_scan ||
                    m_mode == probe_mode::begin_production_shadow_title ||
                    m_mode == probe_mode::begin_production_shadow_partial_style ||
                    m_mode == probe_mode::begin_production_shadow_discogger ||
                    m_mode == probe_mode::begin_production_shadow_discogger_mapping ||
                    m_mode == probe_mode::begin_production_shadow_discogger_configuration ||
                    m_mode == probe_mode::begin_production_shadow_musicbrainz ||
                    m_mode == probe_mode::begin_production_shadow_masstagger ||
                    m_mode == probe_mode::begin_production_shadow_masstagger_partial ||
                    m_mode == probe_mode::begin_production_shadow_masstagger_saved_cue ||
                    m_mode == probe_mode::begin_production_shadow_tracknumber;
                const bool committed = persistent
                    ? begin_virtual_edit_session_diagnostic(virtual_tracks, command, diagnostic)
                    : run_virtual_edit_session_diagnostic(
                        virtual_tracks,
                        command,
                        true,
                        diagnostic,
                        m_mode == probe_mode::run_production_shadow_rollback ||
                        m_mode == probe_mode::run_production_shadow_phase10_unicode_rollback ||
                        m_mode == probe_mode::run_production_shadow_partial_style_rollback ||
                        m_mode == probe_mode::run_production_shadow_replaygain_rollback ||
                        m_mode == probe_mode::run_production_shadow_tracknumber_rollback,
                        m_mode != probe_mode::run_production_shadow_partial_style_strict);
                report << "production_virtual_track_count=" << virtual_tracks.get_count() << "\n"
                       << diagnostic
                       << "write_result=" << (committed ? (persistent ? "started" : "committed") : "not_committed") << "\n";
                if (diagnostic_playlist != SIZE_MAX) {
                    playlist_manager::ptr playlists = playlist_manager::get();
                    const size_t item_count = playlists->playlist_get_item_count(diagnostic_playlist);
                    report << "playlist_item_count=" << item_count << "\n"
                           << "playlist_focus="
                           << playlists->playlist_get_focus_item(diagnostic_playlist) << "\n";
                    for (size_t item = 0; item < item_count; ++item) {
                        const metadb_handle_ptr handle =
                            playlists->playlist_get_item_handle(diagnostic_playlist, item);
                        report << "playlist_item_" << item << "_subsong="
                               << handle->get_subsong_index() << "\n"
                               << "playlist_item_" << item << "_selected="
                               << (playlists->playlist_is_item_selected(diagnostic_playlist, item)
                                    ? "true" : "false") << "\n";
                    }
                    if (diagnostic_lock.is_valid()) {
                        report << "playlist_lock_removed="
                               << (playlists->playlist_lock_uninstall(
                                       diagnostic_playlist,
                                       diagnostic_lock)
                                    ? "true" : "false")
                               << "\n";
                    }
                    report << "playlist_removed="
                           << (playlists->remove_playlist(diagnostic_playlist) ? "true" : "false")
                           << "\n";
                }
            } else if (m_mode == probe_mode::finish_production_shadow) {
                pfc::string8 diagnostic;
                const bool committed = finish_virtual_edit_session_diagnostic(true, diagnostic);
                report << diagnostic
                       << "write_result=" << (committed ? "committed" : "not_committed") << "\n";
            } else if (m_mode == probe_mode::run_projected_shadow_router) {
                pfc::string8 second_path(path);
                if (second_path.replace_string("track-01.mp3", "track-02.mp3") != 1) {
                    report << "write_result=projected_shadow_sibling_not_found\n";
                } else {
                    metadb_handle_list shadows;
                    shadows.add_item(metadb::get()->handle_create(path, 0));
                    shadows.add_item(metadb::get()->handle_create(second_path, 0));
                    const bool started = menu_helpers::run_command_context_ex(
                        guids::run_physical_context_command,
                        pfc::guid_null,
                        shadows,
                        contextmenu_item::caller_undefined);
                    report << "projected_shadow_count=" << shadows.get_count() << "\n";
                    report << "projected_shadow_router_started="
                           << (started ? "true" : "false") << "\n";
                }
            } else if (m_mode == probe_mode::logging_standard ||
                m_mode == probe_mode::logging_verbose) {
                cfg_verbose_logging = m_mode == probe_mode::logging_verbose;
                report << "logging_verbose="
                       << (cfg_verbose_logging ? "true" : "false") << "\n";
                report << "write_result=logging_level_updated\n";
            } else if (m_mode == probe_mode::logging_outcomes) {
                clear_operation_log_capture_for_test();
                {
                    operation_logger success("phase8_logging_smoke", "virtual");
                    success.standard("diff.classified", {
                        log_count("accepted", 1),
                        log_count("skipped", 0),
                        log_count("blocking", 0),
                    });
                    success.finish("SUCCESS");
                }
                {
                    operation_logger partial("phase8_logging_smoke", "virtual");
                    partial.standard("diff.classified", {
                        log_count("accepted", 1),
                        log_count("skipped", 1),
                        log_count("blocking", 0),
                    });
                    partial.finish("SUCCESS_WITH_SKIPPED_CHANGES");
                }
                for (const auto& line : snapshot_operation_log_capture_for_test()) {
                    report << "structured_log=" << line.c_str() << "\n";
                }
                report << "write_result=logging_outcomes_complete\n";
            } else if (m_mode == probe_mode::logging_contract) {
                const char* unicode_value =
                    "Gr" "\xC3\xBC" "\xC3\x9F" "e-"
                    "\xE6\x97\xA5" "\xE6\x9C\xAC" "\xE8\xAA\x9E" "-"
                    "\xF0\x9D\x84\x9E" "-e" "\xCC\x81";
                const char* sentinels[] = {
                    "ACCESS_TOKEN_SENTINEL_DO_NOT_LOG",
                    "REFRESH_TOKEN_SENTINEL_DO_NOT_LOG",
                    "PASSWORD_SENTINEL_DO_NOT_LOG",
                    "COOKIE_SENTINEL_DO_NOT_LOG",
                };
                const std::string line = format_operation_log_line_for_test(
                    "unicode.redaction",
                    {
                        log_token("access_token", sentinels[0]),
                        log_token("refresh_token", sentinels[1]),
                        log_token("password", sentinels[2]),
                        log_token("cookie", sentinels[3]),
                        log_token("unicode", unicode_value),
                    });
                bool sentinels_absent = true;
                for (const char* sentinel : sentinels) {
                    sentinels_absent = sentinels_absent &&
                        line.find(sentinel) == std::string::npos;
                }
                size_t redaction_count = 0;
                size_t position = 0;
                while ((position = line.find("<redacted>", position)) != std::string::npos) {
                    ++redaction_count;
                    position += std::strlen("<redacted>");
                }
                report << "logging_line=" << line.c_str() << "\n";
                report << "credential_sentinels_absent="
                       << (sentinels_absent ? "true" : "false") << "\n";
                report << "redaction_count=" << redaction_count << "\n";
                report << "unicode_preserved="
                       << (line.find(unicode_value) != std::string::npos ? "true" : "false")
                       << "\n";
                report << "session_prefix_present="
                       << (line.rfind("[METAISO S000042]", 0) == 0 ? "true" : "false")
                       << "\n";
                report << "write_result=logging_contract_complete\n";
            } else if (m_mode == probe_mode::tooltip_contract ||
                m_mode == probe_mode::tooltip_contract_dark) {
                const auto tooltip = run_tooltip_runtime_probe(
                    m_mode == probe_mode::tooltip_contract_dark);
                report << "tooltip_requested_count=" << tooltip.requested_count << "\n"
                       << "tooltip_registered_count=" << tooltip.registered_count << "\n"
                       << "tooltip_texts_match="
                       << (tooltip.texts_match ? "true" : "false") << "\n"
                       << "tooltip_distinct_mapping="
                       << (tooltip.distinct_mapping ? "true" : "false") << "\n"
                       << "tooltip_hover_appeared="
                       << (tooltip.hover_appeared ? "true" : "false") << "\n"
                       << "tooltip_hover_dismissed="
                       << (tooltip.hover_dismissed ? "true" : "false") << "\n"
                       << "tooltip_unicode_roundtrip="
                       << (tooltip.unicode_roundtrip ? "true" : "false") << "\n"
                       << "tooltip_dark_mode_requested="
                       << (tooltip.dark_mode_requested ? "true" : "false") << "\n"
                       << "tooltip_dpi=" << tooltip.dpi << "\n"
                       << "write_result=tooltip_contract_complete\n";
            } else if (m_mode == probe_mode::open_preferences ||
                m_mode == probe_mode::open_preferences_dark ||
                m_mode == probe_mode::open_display_preferences) {
                if (m_mode == probe_mode::open_preferences_dark) {
                    force_preferences_dark_for_probe();
                }
                ui_control::get()->show_preferences(
                    m_mode != probe_mode::open_display_preferences ?
                        guids::preferences_page : preferences_page::guid_display);
                report << "preferences_forced_dark="
                       << (m_mode == probe_mode::open_preferences_dark ? "true" : "false")
                       << "\n";
                report << "ui_dark_mode="
                       << (ui_config_manager::g_is_dark_mode() ? "true" : "false") << "\n";
#ifdef _WIN32
                report << "main_window_dpi="
                       << static_cast<unsigned>(
                              ui::detect_window_dpi(core_api::get_main_window())) << "\n";
#endif
                report << "write_result=preferences_opened\n";
            } else if (m_mode == probe_mode::disable_guard || m_mode == probe_mode::enable_guard) {
                cfg_enabled = m_mode == probe_mode::enable_guard;
                report << "guard_enabled=" << (cfg_enabled ? "true" : "false") << "\n";
                report << "write_result=guard_setting_updated\n";
            } else if (m_mode == probe_mode::run_physical_router_menu) {
                const bool started = menu_helpers::run_command_context_ex(
                    guids::run_physical_context_command,
                    pfc::guid_null,
                    m_items,
                    contextmenu_item::caller_undefined);
                report << "physical_router_menu_started=" << (started ? "true" : "false") << "\n";
            } else if (m_mode == probe_mode::run_original_context_menu) {
                contextmenu_manager::ptr context = contextmenu_manager::g_create();
                context->init_context_ex(
                    m_items,
                    contextmenu_manager::flag_view_full,
                    contextmenu_item::caller_undefined);
                report << "original_context_menu_started=true\n";
                context->win32_run_menu_popup(core_api::get_main_window(), nullptr);
                report << "write_result=original_context_menu_returned\n";
            } else if (m_mode == probe_mode::run_projected_shadow_context_menu ||
                m_mode == probe_mode::run_projected_shadow_discogger) {
                pfc::string8 second_path(path);
                if (second_path.replace_string("track-01.mp3", "track-02.mp3") != 1) {
                    report << "write_result=projected_shadow_sibling_not_found\n";
                } else {
                    metadb_handle_list shadows;
                    shadows.add_item(metadb::get()->handle_create(path, 0));
                    shadows.add_item(metadb::get()->handle_create(second_path, 0));
#pragma warning(push)
#pragma warning(disable: 4996)
                    const auto load_status = metadb_io::get()->load_info_multi(
                        shadows,
                        metadb_io::load_info_check_if_changed,
                        core_api::get_main_window(),
                        false);
#pragma warning(pop)
                    report << "projected_shadow_count=" << shadows.get_count() << "\n";
                    report << "projected_shadow_load_status=" << load_status << "\n";
                    contextmenu_manager::ptr context = contextmenu_manager::g_create();
                    context->init_context_ex(
                        shadows,
                        contextmenu_manager::flag_view_full,
                        contextmenu_item::caller_undefined);
                    if (m_mode == probe_mode::run_projected_shadow_context_menu) {
                        report << "projected_shadow_context_menu_started=true\n";
                        write_report(path, report);
                        context->win32_run_menu_popup(core_api::get_main_window(), nullptr);
                        report << "write_result=projected_shadow_context_menu_returned\n";
                    } else {
                        contextmenu_node* command = find_context_command(
                            context->get_root(), "Write tags...");
                        if (command == nullptr) {
                            report << "write_result=discogger_not_found\n";
                        } else {
                            report << "context_command_flags="
                                   << command->get_display_flags() << "\n";
                            report << "context_command_executed=Write tags...\n";
                            command->execute();
                            report << "write_result=discogger_returned\n";
                        }
                    }
                }
            } else if (m_mode == probe_mode::run_original_discogger) {
                contextmenu_manager::ptr context = contextmenu_manager::g_create();
                context->init_context_ex(
                    m_items,
                    contextmenu_manager::flag_view_full,
                    contextmenu_item::caller_undefined);
                contextmenu_node* command = find_context_command(
                    context->get_root(), "Write tags...");
                if (command == nullptr) {
                    report << "write_result=discogger_not_found\n";
                } else {
                    report << "context_command_flags="
                           << command->get_display_flags() << "\n";
                    input_info_reader::ptr reader;
                    reader ^= manager->open_v3(
                        input_info_reader::class_guid,
                        nullptr,
                        path,
                        0,
                        nullptr,
                        fb2k::noAbort);
                    file_info_impl proxy_info;
                    reader->get_info(
                        m_items[0]->get_subsong_index(),
                        proxy_info,
                        fb2k::noAbort);
                    report << "proxy_discogs_release_id="
                           << meta_value_or_missing(proxy_info, "DISCOGS_RELEASE_ID")
                           << "\n";
                    report << "proxy_discogs_artist_id="
                           << meta_value_or_missing(proxy_info, "DISCOGS_ARTIST_ID")
                           << "\n";
                    report << "context_command_executed=Write tags...\n";
                    command->execute();
                    report << "write_result=discogger_returned\n";
                }
            } else if (m_mode == probe_mode::run_original_masstagger) {
                contextmenu_manager::ptr context = contextmenu_manager::g_create();
                context->init_context_ex(
                    m_items,
                    contextmenu_manager::flag_view_full,
                    contextmenu_item::caller_undefined);
                contextmenu_node* command = find_context_command(
                    context->get_root(), "Manage scripts");
                if (command == nullptr) {
                    report << "write_result=masstagger_not_found\n";
                } else {
                    report << "context_command_flags="
                           << command->get_display_flags() << "\n";
                    report << "context_command_executed=Manage scripts\n";
                    command->execute();
                    report << "write_result=masstagger_returned\n";
                }
            } else if (m_mode == probe_mode::run_cuesheet_editor) {
                metadb_handle_ptr representative = m_items[0];
                for (const auto& item : m_items) {
                    if (item->get_subsong_index() != 0) {
                        representative = item;
                        break;
                    }
                }
                metadb_handle_list editor_target;
                editor_target.add_item(representative);
                contextmenu_manager::ptr context = contextmenu_manager::g_create();
                context->init_context_ex(
                    editor_target,
                    contextmenu_manager::flag_view_full,
                    contextmenu_item::caller_undefined);
                contextmenu_node* command = find_context_command(context->get_root(), "Edit cuesheet");
                if (command == nullptr) {
                    report << "write_result=cuesheet_editor_not_found\n";
                } else {
                    report << "context_command_executed=Edit cuesheet\n";
                    command->execute();
                    report << "write_result=cuesheet_editor_returned\n";
                }
            } else if (m_mode == probe_mode::run_masstagger_physical ||
                m_mode == probe_mode::run_discogger_physical ||
                m_mode == probe_mode::run_discogger_plain_physical ||
                m_mode == probe_mode::run_fileops_move_physical ||
                m_mode == probe_mode::run_replaygain_remove_physical) {
                const physical_target_map mapping = map_to_physical_targets(m_items);
                report << "mapping_input_count=" << mapping.input_count << "\n";
                report << "mapping_target_count=" << mapping.handles.get_count() << "\n";
                report << "mapping_rejected_count=" << mapping.rejected_count << "\n";
                if (!mapping.complete() || mapping.empty()) {
                    report << "write_result=context_command_mapping_rejected\n";
                } else {
#pragma warning(push)
#pragma warning(disable: 4996)
                    const auto load_status = metadb_io::get()->load_info_multi(
                        mapping.handles,
                        metadb_io::load_info_check_if_changed,
                        core_api::get_main_window(),
                        false);
#pragma warning(pop)
                    report << "physical_info_load_status=" << load_status << "\n";
                    if (load_status != metadb_io::load_info_success) {
                        report << "write_result=context_command_load_failed\n";
                        write_report(path, report);
                        return;
                    }
                    const char* command_name =
                        m_mode == probe_mode::run_masstagger_physical ? "Manage scripts" :
                        m_mode == probe_mode::run_discogger_physical ? "Write tags..." :
                        m_mode == probe_mode::run_discogger_plain_physical ? "Plain write tags..." :
                        m_mode == probe_mode::run_fileops_move_physical ? "Move to..." :
                        "Remove ReplayGain information from files";
                    contextmenu_manager::ptr context = contextmenu_manager::g_create();
                    context->init_context_ex(
                        mapping.handles,
                        contextmenu_manager::flag_view_full,
                        contextmenu_item::caller_undefined);
                    contextmenu_node* command = find_context_command(context->get_root(), command_name);
                    if (command == nullptr) {
                        report << "write_result=context_command_not_found\n";
                    } else {
                        report << "context_command_executed=" << command_name << "\n";
                        command->execute();
                        report << "write_result=context_command_returned\n";
                    }
                }
            } else if (m_mode == probe_mode::context_tree_physical) {
                const physical_target_map mapping = map_to_physical_targets(m_items);
                report << "mapping_input_count=" << mapping.input_count << "\n";
                report << "mapping_target_count=" << mapping.handles.get_count() << "\n";
                report << "mapping_rejected_count=" << mapping.rejected_count << "\n";
                if (!mapping.complete() || mapping.empty()) {
                    report << "write_result=context_tree_mapping_rejected\n";
                } else {
                    contextmenu_manager::ptr context = contextmenu_manager::g_create();
                    context->init_context_ex(
                        mapping.handles,
                        contextmenu_manager::flag_view_full,
                        contextmenu_item::caller_undefined);
                    append_context_tree(context->get_root(), report);
                    report << "write_result=context_tree_listed\n";
                }
            } else if (m_mode == probe_mode::map_physical ||
                m_mode == probe_mode::map_physical_with_duplicate ||
                m_mode == probe_mode::map_physical_with_remote ||
                m_mode == probe_mode::map_all_handles_physical) {
                metadb_handle_list mapped_items = m_items;
                if (m_mode == probe_mode::map_physical_with_duplicate) {
                    mapped_items.add_item(metadb::get()->handle_create(path, 0));
                } else if (m_mode == probe_mode::map_physical_with_remote) {
                    mapped_items.add_item(metadb::get()->handle_create(
                        "http://127.0.0.1/metadata-isolation-unsupported.mp3", 0));
                } else if (m_mode == probe_mode::map_all_handles_physical) {
                    mapped_items.remove_all();
                    for (size_t source = 0; source < m_items.get_count(); ++source) {
                        const char* source_path = m_items[source]->get_path();
                        input_info_reader::ptr reader;
                        input_entry::g_open_for_info_read(
                            reader, nullptr, source_path, fb2k::noAbort);
                        mapped_items.add_item(metadb::get()->handle_create(source_path, 0));
                        for (unsigned index = 0; index < reader->get_subsong_count(); ++index) {
                            const unsigned subsong = reader->get_subsong(index);
                            if (subsong != 0) {
                                mapped_items.add_item(metadb::get()->handle_create(source_path, subsong));
                            }
                        }
                    }
                }
                const physical_target_map mapping = map_to_physical_targets(mapped_items);
                report << "mapping_input_count=" << mapping.input_count << "\n";
                report << "mapping_target_count=" << mapping.handles.get_count() << "\n";
                report << "mapping_rejected_count=" << mapping.rejected_count << "\n";
                report << "mapping_complete=" << (mapping.complete() ? "true" : "false") << "\n";
                if (!mapping.first_rejected_path.is_empty()) {
                    report << "mapping_first_rejected_path=" << mapping.first_rejected_path << "\n";
                    report << "mapping_first_rejected_reason=" << mapping.first_rejected_reason << "\n";
                }
                for (size_t index = 0; index < mapping.handles.get_count(); ++index) {
                    report << "mapping_target_" << index << "_path="
                           << mapping.handles[index]->get_path() << "\n";
                    report << "mapping_target_" << index << "_subsong="
                           << mapping.handles[index]->get_subsong_index() << "\n";
                }
                report << "write_result=map_only\n";
            } else if (m_mode == probe_mode::chapterizer_title) {
#ifdef FOOBAR2000_HAVE_CHAPTERIZER
                chapterizer::ptr editor;
                if (!chapterizer::g_find(editor, path)) throw pfc::exception("No chapterizer found");
                chapter_list_impl chapters;
                editor->get_chapters(path, chapters, fb2k::noAbort);
                file_info_impl first = chapters.get_info(0);
                first.meta_set("TITLE", "Cue Track A Edited");
                chapters.set_info(0, first);
                editor->set_chapters(path, chapters, fb2k::noAbort);
                report << "write_result=chapterizer_committed\n";
#else
                report << "write_result=chapterizer_not_in_sdk\n";
#endif
            } else if (m_mode == probe_mode::physical_matrix ||
                       m_mode == probe_mode::physical_replaygain_remove) {
                const physical_target_map mapping = map_to_physical_targets(m_items);
                report << "mapping_input_count=" << mapping.input_count << "\n";
                report << "mapping_target_count=" << mapping.handles.get_count() << "\n";
                report << "mapping_rejected_count=" << mapping.rejected_count << "\n";
                report << "mapping_complete=" << (mapping.complete() ? "true" : "false") << "\n";
                if (!mapping.complete() || mapping.empty()) {
                    throw pfc::exception("Phase-12 physical matrix mapping failed");
                }
                for (size_t target = 0; target < mapping.handles.get_count(); ++target) {
                    const char* target_path = mapping.handles[target]->get_path();
                    report << "mapping_target_" << target << "_path=" << target_path << "\n";
                    report << "mapping_target_" << target << "_subsong="
                           << mapping.handles[target]->get_subsong_index() << "\n";

                    {
                        auto lock = file_lock_manager::get()->acquire_write(target_path, fb2k::noAbort);
                        input_info_writer::ptr writer;
                        input_entry::g_open_for_info_write_timeout(
                            writer, nullptr, target_path, fb2k::noAbort, 1.0);
                        file_info_impl info;
                        writer->get_info(0, info, fb2k::noAbort);
                        if (m_mode == probe_mode::physical_matrix) {
                            // One native writer transition deliberately covers normal and
                            // custom metadata semantics without involving virtual routing.
                            info.meta_set("TITLE", "Physical Matrix Title \xCE\x94");
                            info.meta_set("DISCNUMBER", "2");
                            info.meta_set("PHASE12_ADD", "added \xE6\x9D\xB1\xE4\xBA\xAC");
                            info.meta_set("CUSTOM_FIELD", "changed \xCE\xA9");
                            info.meta_set(
                                "MUSICBRAINZ_RELEASEID",
                                "f6758afb-ecff-410e-8979-b131cfa3c6d3");
                            info.meta_remove_field("MULTI_VALUE_FIELD");
                            info.meta_remove_field("COMMENT");
                            const size_t vector = info.meta_set("PHASE12_VECTOR", "\xCE\xB1");
                            info.meta_add_value(vector, "\xCE\xB2");
                            info.meta_add_value(vector, "\xCE\xB1");
                            info.meta_add_value(vector, "\xF0\x9F\x98\x80");
                            info.meta_add_value(vector, "");
                            replaygain_info rg = info.get_replaygain();
                            rg.m_track_gain = -6.25f;
                            rg.m_track_peak = 0.712345f;
                            rg.m_album_gain = -8.75f;
                            rg.m_album_peak = 0.912345f;
                            info.set_replaygain(rg);
                        } else {
                            replaygain_info rg = info.get_replaygain();
                            rg.remove_track_gain();
                            rg.remove_track_peak();
                            rg.remove_album_gain();
                            rg.remove_album_peak();
                            info.set_replaygain(rg);
                        }
                        writer->set_info(0, info, fb2k::noAbort);
                        writer->commit(fb2k::noAbort);
                    }

                    input_info_reader::ptr reopened;
                    input_entry::g_open_for_info_read(
                        reopened, nullptr, target_path, fb2k::noAbort);
                    file_info_impl postimage;
                    reopened->get_info(0, postimage, fb2k::noAbort);
                    pfc::string_formatter phase;
                    phase << "target_" << target << "_after";
                    append_info_inventory(report, phase.c_str(), 0, postimage);
                }
                report << "write_result=physical_batch_committed\n";
            } else if (m_mode == probe_mode::direct_guard_virtual_title) {
                // Test-only command, compiled solely with EnableTestProbes=true.
                // Never use on anything except a create-only disposable runner fixture.
                // This deliberately exercises the native, unsafe host input writer
                // without the isolated-shadow pathway; the Write Guard must veto it.
                if (std::strstr(path, "mi-virtual-write-") == nullptr ||
                    pfc::stricmp_ascii(pfc::string_extension(path), "mp3") != 0 ||
                    !cfg_enabled) {
                    throw pfc::exception("Direct Guard probe requires enabled protection and a disposable test MP3");
                }
                input_info_reader::ptr inventory;
                input_entry::g_open_for_info_read(inventory, nullptr, path, fb2k::noAbort);
                bool track_one_found = false;
                for (unsigned i = 0; i < inventory->get_subsong_count(); ++i) {
                    if (inventory->get_subsong(i) == 1) track_one_found = true;
                }
                if (!track_one_found) throw pfc::exception("Disposable source has no embedded Cue subsong 1");
                file_info_impl pre;
                inventory->get_info(1, pre, fb2k::noAbort);
                pfc::string8 expected(pre.meta_get("TITLE", 0));
                if (expected != "Virtual Original One") {
                    throw pfc::exception("Unexpected virtual preimage in direct Guard probe");
                }
                inventory.release();

                const char* attempted = "Direct Unsafe Probe MUST NOT COMMIT";
                report << "direct_guard_subsong=1\n"
                       << "direct_guard_title_before=" << expected << "\n"
                       << "direct_guard_title_attempted=" << attempted << "\n"
                       << "direct_guard_protection_before="
                       << protection_status_name(get_protection_status()) << "\n";
                auto lock = file_lock_manager::get()->acquire_write(path, fb2k::noAbort);
                try {
                    input_info_writer::ptr writer;
                    input_entry::g_open_for_info_write_timeout(
                        writer, nullptr, path, fb2k::noAbort, 1.0);
                    file_info_impl updated;
                    writer->get_info(1, updated, fb2k::noAbort);
                    updated.meta_set("TITLE", attempted);
                    report << "direct_guard_write_attempted=true\n";
                    writer->set_info(1, updated, fb2k::noAbort);
                    writer->commit(fb2k::noAbort);
                    report << "direct_guard_commit_returned=true\n";
                    writer.release();
                } catch (const std::exception& error) {
                    // An exception is a valid guard outcome, but never an excuse
                    // to declare PASS without checking the source byte-for-byte.
                    report << "direct_guard_commit_returned=false\n"
                           << "direct_guard_writer_exception=" << error.what() << "\n";
                }
                lock.release();
                input_info_reader::ptr reloaded;
                input_entry::g_open_for_info_read(reloaded, nullptr, path, fb2k::noAbort);
                file_info_impl after;
                reloaded->get_info(1, after, fb2k::noAbort);
                const char* title_after = after.meta_get("TITLE", 0);
                report << "direct_guard_title_after="
                       << (title_after == nullptr ? "<missing>" : title_after) << "\n"
                       << "direct_guard_protection_after="
                       << protection_status_name(get_protection_status()) << "\n"
                       << "write_result="
                       << (title_after != nullptr && expected == title_after ?
                           "guard_preserved" : "unsafe_mutation") << "\n";
            } else if (m_mode == probe_mode::physical_title ||
                       m_mode == probe_mode::physical_all ||
                       m_mode == probe_mode::physical_album_artist ||
                       m_mode == probe_mode::physical_raw_cue_patch) {
                auto lock = file_lock_manager::get()->acquire_write(path, fb2k::noAbort);
                input_info_writer::ptr writer;
                input_entry::g_open_for_info_write_timeout(
                    writer, nullptr, path, fb2k::noAbort, 1.0);
                report << "physical_writer_subsongs=" << writer->get_subsong_count() << "\n";
                file_info_impl info;
                writer->get_info(0, info, fb2k::noAbort);
                report << "physical_title_before=" << meta_value_or_missing(info, "TITLE") << "\n";
                if (m_mode == probe_mode::physical_raw_cue_patch) {
                    const char* cue = info.meta_get("CUESHEET", 0);
                    if (cue == nullptr) throw pfc::exception("Raw CUESHEET field not found");
                    pfc::string8 patched(cue);
                    if (patched.replace_string("Cue Track A", "Give Life Back To Music") != 1 ||
                        patched.replace_string("Cue Artist A", "Daft Punk") != 1 ||
                        patched.replace_string("Cue Track B", "The Game Of Love") != 1 ||
                        patched.replace_string("Cue Artist B", "Daft Punk") != 1) {
                        throw pfc::exception("Raw Cue patch was not uniquely applicable");
                    }
                    info.meta_set("CUESHEET", patched);
                    report << "raw_cue_patch_count=4\n";
                } else if (m_mode == probe_mode::physical_album_artist) {
                    info.meta_set("ALBUM ARTIST", "Various Artists");
                } else {
                    info.meta_set("TITLE", "Physical Mix Edited");
                }
                if (m_mode == probe_mode::physical_all) {
                    info.meta_set("ARTIST", "Physical DJ Edited");
                    info.meta_set("TRACKNUMBER", "04");
                    info.meta_set("ALBUM", "Physical Album Edited");
                    info.meta_set("COMMENT", "Physical Comment Edited");
                    info.meta_set("CUSTOM_FIELD", "Custom Value Edited");
                }
                writer->set_info(0, info, fb2k::noAbort);
                writer->commit(fb2k::noAbort);
                report << "write_result=physical_committed\n";
            } else {
                auto lock = file_lock_manager::get()->acquire_write(path, fb2k::noAbort);
                input_info_writer::ptr writer;
                input_entry::g_open_for_info_write_timeout(
                    writer, nullptr, path, fb2k::noAbort, 1.0);

                size_t updated = 0;
                for (size_t index = 0; index < m_items.get_count(); ++index) {
                    const auto subsong = m_items[index]->get_subsong_index();
                    if (subsong == 0) continue;
                    file_info_impl info;
                    writer->get_info(subsong, info, fb2k::noAbort);
                    report << "target_" << updated << "_subsong=" << subsong << "\n";
                    report << "target_" << updated << "_title_before="
                           << meta_value_or_missing(info, "TITLE") << "\n";

                    switch (m_mode) {
                    case probe_mode::virtual_title:
                        info.meta_set("TITLE", "Cue Track A Edited");
                        break;
                    case probe_mode::virtual_artist:
                        info.meta_set("ARTIST", "Cue Artist A Edited");
                        break;
                    case probe_mode::virtual_tracknumber:
                        info.meta_set("TRACKNUMBER", "11");
                        break;
                    case probe_mode::virtual_album:
                        info.meta_set("ALBUM", "Cue Album Edited");
                        break;
                    case probe_mode::virtual_comment:
                        info.meta_set("COMMENT", "Cue Comment Edited");
                        break;
                    case probe_mode::virtual_custom:
                        info.meta_set("CUSTOM_FIELD", "Cue Custom Edited");
                        break;
                    case probe_mode::virtual_nochange:
                        break;
                    case probe_mode::virtual_all_titles:
                        info.meta_set("TITLE", subsong == 1 ? "Cue Track A Edited" : "Cue Track B Edited");
                        break;
                    default:
                        throw pfc::exception("Invalid virtual probe mode");
                    }
                    writer->set_info(subsong, info, fb2k::noAbort);
                    ++updated;
                    if (m_mode != probe_mode::virtual_all_titles) break;
                }
                if (updated == 0) {
                    report << "write_result=no_virtual_subsong\n";
                } else {
                    writer->commit(fb2k::noAbort);
                    report << "write_result=committed\n";
                }
            }
        } catch (const std::exception& error) {
            report << "write_result=error\n";
            report << "error=" << error.what() << "\n";
        }

        try {
            write_report(path, report);
        } catch (const std::exception& error) {
            FB2K_console_formatter() << "Metadata Isolation probe report failed: " << error;
        }
    }

private:
    metadb_handle_list m_items;
    probe_mode m_mode = probe_mode::virtual_title;
};

commandline_handler_factory_t<probe_commandline_handler> g_probe_factory;

class probe_generic_shadow_command : public contextmenu_item_simple {
public:
    unsigned get_num_items() override { return 46; }
    GUID get_parent() override { return contextmenu_groups::root; }

    void get_item_name(unsigned index, pfc::string_base& output) override {
        static const char* names[] = {
            "Research: Set projected shadow TITLE",
            "Research: Set projected shadow TITLE and ARTIST",
            "Research: Set projected shadow custom field",
            "Research: Set projected shadow TITLE and custom field",
            "Research: Delete projected shadow TITLE",
            "Research: Set multivalue projected shadow TITLE",
            "Research: Set projected shadow native Cue fields",
            "Research: Set projected shadow global Cue fields",
            "Research: Set differing projected shadow ALBUM",
            "Research: Set projected shadow inherited global Cue fields",
            "Research: Set unrepresentable inherited projected shadow ALBUM ARTIST",
            "Research: Add projected shadow Cue fields",
            "Research: Delete projected shadow Cue fields",
            "Research: Set projected shadow fields to inherited values",
            "Research: Add projected shadow global Cue fields",
            "Research: Delete projected shadow global Cue fields",
            "Research: Add projected shadow TITLE and custom field",
            "Research: Set projected shadow text fields to inherited values",
            "Research: Batch atomic projected shadow add",
            "Research: Delete projected shadow inherited global fields",
            "Research: Set projected shadow ReplayGain",
            "Research: Delete projected shadow ReplayGain",
            "Research: Set projected shadow TITLE and ReplayGain",
            "Research: Set projected shadow ReplayGain and custom field",
            "Research: Renumber projected shadow TRACK midpoint",
            "Research: Shift projected shadow TRACK numbers",
            "Research: Conflict projected shadow TRACK numbers",
            "Research: Reverse projected shadow TRACK numbers",
            "Research: Set alphanumeric projected shadow TRACKNUMBER",
            "Research: Delete projected shadow TRACKNUMBER",
            "Research: Set TITLE and conflicting projected shadow TRACK numbers",
            "Research: Set TITLE and renumber projected shadow TRACK midpoint",
            "Research: Partially renumber projected shadow TRACK midpoint",
            "Research: Partially reorder projected shadow TRACK midpoint",
            "Research: Set projected shadow Windows-1252 text",
            "Research: Set projected shadow full Unicode text",
            "Research: Set projected shadow long Unicode text",
            "Research: Set projected shadow unsafe Cue text",
            "Research: Set projected shadow TITLE and multivalue STYLE",
            "Research: Set projected shadow TITLE, multivalue STYLE and custom field",
            "Research: Set projected shadow multivalue STYLE only",
            "Research: Delete projected shadow STYLE only",
            "Research: Set projected shadow ReplayGain and multivalue STYLE",
            "Research: Leave projected shadows unchanged",
            "Research: Apply phase 10 Unicode matrix",
            "Research: Apply phase 10 Unicode partial matrix",
        };
        output = names[index];
    }

    GUID get_item_guid(unsigned index) override {
        static constexpr GUID guids[] = {
            { 0x5708089d, 0xd8da, 0x44d9, { 0xb7, 0x2d, 0xc2, 0x55, 0x3d, 0xe6, 0xe4, 0x8d } },
            { 0x074bc226, 0xba6c, 0x4e6d, { 0x83, 0x69, 0x03, 0x56, 0xc7, 0x75, 0x33, 0xf6 } },
            { 0x8b395435, 0xc027, 0x4d7f, { 0x8f, 0x12, 0xc3, 0x9f, 0x46, 0x0b, 0x54, 0xd3 } },
            { 0xf9a450c7, 0x10b1, 0x4ec7, { 0x8d, 0x61, 0xa5, 0xd8, 0xf1, 0x77, 0x8e, 0xf4 } },
            { 0x43bf9a35, 0x762f, 0x48ed, { 0x9a, 0x99, 0xba, 0x61, 0x82, 0x2d, 0xb6, 0x6f } },
            { 0x85ed51b1, 0x0f71, 0x4ead, { 0x91, 0xb9, 0x35, 0x4f, 0x51, 0x57, 0x7b, 0xaf } },
            { 0xd55efc93, 0x88d5, 0x45c4, { 0x8b, 0xb2, 0x54, 0x27, 0x0d, 0xab, 0x94, 0x26 } },
            { 0x7069838f, 0xbcbe, 0x49c9, { 0xa4, 0xcd, 0xea, 0x46, 0xfd, 0x50, 0x45, 0x5e } },
            { 0xe90176f3, 0x2d68, 0x402d, { 0xb1, 0x6a, 0x83, 0x35, 0xc9, 0x14, 0x7e, 0xa9 } },
            { 0x12bf45da, 0xb721, 0x44d7, { 0x8c, 0x49, 0x55, 0x11, 0xcb, 0x50, 0x93, 0x27 } },
            { 0x64e51d6b, 0xad77, 0x4495, { 0xa4, 0x17, 0x4c, 0x9a, 0x24, 0xd7, 0x11, 0x83 } },
            { 0x8c7ef032, 0xb560, 0x4df4, { 0xa7, 0x2d, 0x4b, 0x4b, 0x2b, 0xf5, 0x34, 0x01 } },
            { 0x32da7fd9, 0xe156, 0x4c4a, { 0x9c, 0x74, 0x65, 0xe7, 0x17, 0x79, 0x03, 0x4c } },
            { 0xab51855c, 0x7139, 0x40f1, { 0x9a, 0x65, 0xd3, 0x16, 0xda, 0xd2, 0xd9, 0x73 } },
            { 0x30770dc6, 0x4882, 0x4f6f, { 0xb4, 0x89, 0x80, 0x39, 0x14, 0x41, 0xea, 0xd1 } },
            { 0x432191f9, 0xd900, 0x4ba4, { 0x87, 0x76, 0x3c, 0x8f, 0xcc, 0xc0, 0x55, 0x74 } },
            { 0xdb2f2182, 0x7b97, 0x4aef, { 0x9e, 0x47, 0xf1, 0x96, 0xb0, 0x9b, 0xc1, 0x52 } },
            { 0xa70760db, 0x9e5d, 0x4af5, { 0xa8, 0x87, 0xf0, 0x0c, 0xe9, 0xb9, 0x56, 0xa1 } },
            { 0x447a8f2f, 0x4fa2, 0x47de, { 0x86, 0x56, 0x27, 0xe4, 0x3a, 0x08, 0xaa, 0x0c } },
            { 0x6e6a68dd, 0x1440, 0x4846, { 0x8e, 0x8e, 0x1b, 0x22, 0xa1, 0x67, 0xe8, 0x1d } },
            { 0x3ccbb69f, 0xe2f9, 0x4a23, { 0xb4, 0x45, 0x10, 0x83, 0x94, 0xe5, 0x20, 0x61 } },
            { 0x8c3c9fc3, 0x1cbc, 0x44d0, { 0x9b, 0xc8, 0xa1, 0xd1, 0x77, 0xfb, 0x7c, 0x92 } },
            { 0x528614d8, 0xc0c7, 0x424a, { 0xb2, 0x25, 0xce, 0xb7, 0x04, 0x33, 0xf2, 0x56 } },
            { 0x93214029, 0x05cf, 0x48d0, { 0xab, 0x4b, 0xa3, 0xdc, 0x17, 0x06, 0xa5, 0x14 } },
            { 0xb2239294, 0x60c5, 0x46e5, { 0x8d, 0xca, 0xe7, 0xf4, 0x78, 0xe8, 0xf4, 0xf1 } },
            { 0xb8643da4, 0xb227, 0x4367, { 0x96, 0x31, 0xd4, 0xee, 0xd4, 0x1f, 0xa3, 0xc6 } },
            { 0x26de4bf0, 0xe56a, 0x4ce4, { 0xa8, 0x55, 0xad, 0xed, 0x87, 0x91, 0x90, 0xd4 } },
            { 0x20fd74d9, 0x1953, 0x4d08, { 0x93, 0xfd, 0x50, 0x87, 0x3b, 0xb8, 0x3f, 0x68 } },
            { 0xaf5754ca, 0x8a70, 0x478d, { 0xa4, 0x08, 0xb3, 0x5c, 0x4b, 0xf8, 0x85, 0xf9 } },
            { 0xef0d1e80, 0x597c, 0x42a7, { 0xaf, 0x83, 0x29, 0xd2, 0x43, 0xf0, 0x03, 0xb8 } },
            { 0x8298db1e, 0x47aa, 0x4ff2, { 0x8a, 0x09, 0xa2, 0xc7, 0xe6, 0xda, 0x5d, 0x84 } },
            { 0x0dd4f929, 0xb0ee, 0x4f8b, { 0x93, 0xa6, 0xb4, 0x9e, 0x2d, 0x48, 0x1f, 0x47 } },
            { 0x2eeea830, 0xf432, 0x45da, { 0xa5, 0xca, 0xcf, 0x9e, 0xd0, 0xe4, 0x65, 0xc9 } },
            { 0x1bcaf655, 0xb6ec, 0x485c, { 0xbe, 0xba, 0x13, 0xcb, 0xb1, 0x66, 0x57, 0x69 } },
            { 0x91d37b6a, 0x4312, 0x47aa, { 0x89, 0xba, 0xb1, 0xa4, 0x11, 0xef, 0x74, 0x20 } },
            { 0x758a2f9c, 0x574e, 0x4bc1, { 0x98, 0x32, 0x5d, 0x47, 0xeb, 0x91, 0x08, 0xc4 } },
            { 0xa13c5e8f, 0x9e24, 0x4f60, { 0x84, 0xe7, 0x66, 0x2a, 0x19, 0xcf, 0xb8, 0x35 } },
            { 0x3f862ca1, 0x7d55, 0x49ce, { 0xa2, 0x10, 0xdd, 0x7c, 0x43, 0x5b, 0xe6, 0x92 } },
            { 0x2787f26c, 0xb927, 0x44f1, { 0x91, 0x2f, 0xfa, 0xe4, 0x35, 0x54, 0x82, 0x1b } },
            { 0x3aec6117, 0x80d5, 0x45ed, { 0xa2, 0x73, 0xb6, 0x67, 0x66, 0xec, 0xc8, 0x9d } },
            { 0x9228e381, 0xbc5b, 0x4fe8, { 0x9b, 0xb3, 0x8e, 0x09, 0x14, 0xad, 0x84, 0xef } },
            { 0x328cdb50, 0x4fc7, 0x4a4d, { 0x89, 0xe2, 0xdc, 0x95, 0x63, 0xaa, 0x25, 0xe8 } },
            { 0x3a07d8ea, 0x1fb1, 0x4e45, { 0xa4, 0x0f, 0x1e, 0xd6, 0x8e, 0xa7, 0xb2, 0x56 } },
            { 0x6203b1a9, 0x253a, 0x4fcb, { 0x9a, 0x5d, 0x77, 0x36, 0x19, 0x1c, 0xae, 0x04 } },
            { 0x0eb8995a, 0x896a, 0x4f64, { 0x8a, 0xdc, 0xb8, 0xa7, 0x8d, 0x2d, 0xd0, 0x31 } },
            { 0x5c57234a, 0x56d9, 0x4455, { 0xa3, 0x38, 0x97, 0x9e, 0x8e, 0xf4, 0xc5, 0xf2 } },
        };
        return guids[index];
    }

    bool get_item_description(unsigned, pfc::string_base& output) override {
        output = "TEST-PROBE: writes deterministic TITLE values to disposable physical shadows.";
        return true;
    }

    bool context_get_display(
        unsigned index,
        metadb_handle_list_cref data,
        pfc::string_base& output,
        unsigned& flags,
        const GUID& caller) override {
        if (data.get_count() == 0) return false;
        for (const auto& item : data) {
            if ((std::strstr(item->get_path(), "safe-shadow-generic-projected") == nullptr &&
                 std::strstr(item->get_path(), "foo_metadata_isolation-") == nullptr)) {
                return false;
            }
        }
        return contextmenu_item_simple::context_get_display(
            index, data, output, flags, caller);
    }

    void context_command(
        unsigned command_index,
        metadb_handle_list_cref data,
        const GUID&) override {
        if (command_index == 43) return;
        size_t track_index = 0;
        std::string active_path;
        file_lock_ptr lock;
        input_info_writer::ptr writer;
        const auto commit_active_writer = [&writer]() {
            if (writer.is_valid()) {
                writer->commit(fb2k::noAbort);
                writer.release();
            }
        };
        for (const auto& item : data) {
            if (active_path.empty() ||
                metadb::path_compare(active_path.c_str(), item->get_path()) != 0) {
                commit_active_writer();
                lock.release();
                active_path = item->get_path();
                lock = file_lock_manager::get()->acquire_write(
                    item->get_path(), fb2k::noAbort);
                input_entry::g_open_for_info_write_timeout(
                    writer, nullptr, item->get_path(), fb2k::noAbort, 1.0);
            }
            file_info_impl info;
            const unsigned target_subsong = item->get_subsong_index();
            writer->get_info(target_subsong, info, fb2k::noAbort);
            pfc::string_formatter value;
            value << (command_index == 0 ? "Generic Context Title " :
                      command_index == 1 ? "Generic Context Artist " :
                      command_index == 3 ? "Mixed Unsupported Custom " :
                      "Unsupported Custom ")
                  << (track_index + 1);
            const auto set_replaygain = [&info, track_index]() {
                static const char* track_gains[] = { "-6.84 dB", "-7.12 dB", "-4.44 dB" };
                static const char* track_peaks[] = { "0.975321", "0.876543", "0.765432" };
                static const char* album_gains[] = { "-8.40 dB", "-9.10 dB", "-7.70 dB" };
                static const char* album_peaks[] = { "0.965432", "0.854321", "0.743210" };
                const size_t value_index = std::min<size_t>(track_index, 2);
                replaygain_info state;
                state.set_track_gain_text(track_gains[value_index]);
                state.set_track_peak_text(track_peaks[value_index]);
                state.set_album_gain_text(album_gains[value_index]);
                state.set_album_peak_text(album_peaks[value_index]);
                info.set_replaygain(state);
            };
            if (command_index == 0) {
                info.meta_set("TITLE", value);
            } else if (command_index == 1) {
                pfc::string_formatter title;
                title << "Generic Context Title " << (track_index + 1);
                info.meta_set("TITLE", title);
                info.meta_set("ARTIST", value);
            } else if (command_index == 2) {
                info.meta_set("MI_UNSUPPORTED_CUSTOM", value);
            } else if (command_index == 3) {
                pfc::string_formatter title;
                title << "Mixed Generic Context Title " << (track_index + 1);
                info.meta_set("TITLE", title);
                info.meta_set("MI_UNSUPPORTED_CUSTOM", value);
            } else if (command_index == 4) {
                info.meta_remove_field("TITLE");
            } else if (command_index == 5) {
                const auto set_values = [&info](
                    const char* field,
                    std::initializer_list<const char*> values) {
                    info.meta_remove_field(field);
                    size_t field_index = SIZE_MAX;
                    for (const char* item : values) {
                        if (field_index == SIZE_MAX) field_index = info.meta_set(field, item);
                        else info.meta_add_value(field_index, item);
                    }
                };
                // TEST-PROBE: one command exercises exact file_info vectors,
                // including punctuation that must never be split, Unicode,
                // order, duplicates and the main transition classes. Source
                // fixtures seed the MV_* preimages through ordinary ID3v2
                // metadata inherited by each virtual subsong.
                set_values("TITLE", { "House; Techno", "Ambient" });
                set_values("ARTIST", { "Artist A", "Artist B" });
                set_values("GENRE", { "House", "Techno" });
                set_values("COMMENT", {
                    "Grüße, 日本語 / pipe|quote\"",
                    " leading and trailing "
                });
                set_values("MV_ADD_ONE", { "A", "B", "C" });
                set_values("MV_CHANGE", { "A", "C" });
                set_values("MV_DELETE_ONE", { "A" });
                info.meta_remove_field("MV_DELETE_ALL");
                set_values("MV_REORDER", { "B", "A" });
                set_values("MV_DUPLICATES", { "A", "A" });
                set_values("MV_EMPTY", { "A", "" });
                info.meta_remove_field("MV_TEN");
                info.meta_remove_field("MV_HUNDRED");
                for (size_t value_index = 0; value_index < 100; ++value_index) {
                    pfc::string_formatter generated;
                    generated << "Value " << (value_index + 1);
                    if (value_index < 10) info.meta_add("MV_TEN", generated);
                    info.meta_add("MV_HUNDRED", generated);
                }
            } else if (command_index == 6) {
                pfc::string_formatter writer_value, isrc_value, comment_value;
                writer_value << "Capability Writer " << (track_index + 1);
                isrc_value << "USLAB260000" << (track_index + 1);
                comment_value << "Capability Comment " << (track_index + 1);
                info.meta_set("SONGWRITER", writer_value);
                info.meta_set("ISRC", isrc_value);
                info.meta_set("DATE", "2031");
                info.meta_set("COMMENT", comment_value);
            } else if (command_index == 7) {
                info.meta_set("ALBUM", "Capability Global Album");
                info.meta_set("ALBUM ARTIST", "Capability Global Album Artist");
                info.meta_set("GENRE", "Capability Global Genre");
                info.meta_set("DISCNUMBER", "7");
                info.meta_set("TOTALDISCS", "9");
            } else if (command_index == 8) {
                pfc::string_formatter album_value;
                album_value << "Capability Differing Album " << (track_index + 1);
                info.meta_set("ALBUM", album_value);
            } else if (command_index == 9) {
                info.meta_set("ARTIST", "Capability Inherited Artist");
                info.meta_set("SONGWRITER", "Capability Inherited Writer");
                info.meta_set("DATE", "2042");
                info.meta_set("COMMENT", "Capability Inherited Comment");
            } else if (command_index == 10) {
                info.meta_set("ALBUM ARTIST", "Capability Unrepresentable Album Artist");
            } else if (command_index == 11) {
                pfc::string_formatter suffix, isrc;
                suffix << (track_index + 1);
                isrc << "USADD260000" << (track_index + 1);
                info.meta_set("TITLE", (std::string("Added Track Title ") + suffix.c_str()).c_str());
                info.meta_set("ARTIST", (std::string("Added Track Artist ") + suffix.c_str()).c_str());
                info.meta_set("SONGWRITER", (std::string("Added Track Writer ") + suffix.c_str()).c_str());
                info.meta_set("DATE", "2050");
                info.meta_set("COMMENT", (std::string("Added Track Comment ") + suffix.c_str()).c_str());
                info.meta_set("ISRC", isrc);
            } else if (command_index == 12) {
                for (const char* field : { "TITLE", "ARTIST", "SONGWRITER", "DATE", "COMMENT", "ISRC" }) {
                    info.meta_remove_field(field);
                }
            } else if (command_index == 13) {
                info.meta_set("ARTIST", "Cue Album Artist");
                info.meta_set("SONGWRITER", "Album Writer");
                info.meta_set("DATE", "2020");
                info.meta_set("COMMENT", "Album Comment");
            } else if (command_index == 14) {
                info.meta_set("ALBUM", "Added Global Album");
                info.meta_set("ALBUM ARTIST", "Added Global Album Artist");
                info.meta_set("GENRE", "Added Global Genre");
                info.meta_set("DISCNUMBER", "4");
                info.meta_set("TOTALDISCS", "6");
            } else if (command_index == 15) {
                for (const char* field : { "ALBUM", "ALBUM ARTIST", "GENRE", "DISCNUMBER", "TOTALDISCS" }) {
                    info.meta_remove_field(field);
                }
            } else if (command_index == 16) {
                info.meta_set("TITLE", "Added Mixed Title");
                info.meta_set("MI_UNSUPPORTED_CUSTOM", "Atomic reject");
            } else if (command_index == 17) {
                info.meta_set("SONGWRITER", "Album Writer");
                info.meta_set("DATE", "2020");
                info.meta_set("COMMENT", "Album Comment");
            } else if (command_index == 18) {
                info.meta_set("TITLE", "Batch Added Title");
                if (track_index >= 2) {
                    info.meta_set("MI_UNSUPPORTED_CUSTOM", "Atomic second-document reject");
                }
            } else if (command_index == 19) {
                for (const char* field : { "ARTIST", "SONGWRITER", "DATE", "COMMENT" }) {
                    info.meta_remove_field(field);
                }
            } else if (command_index == 20) {
                set_replaygain();
            } else if (command_index == 21) {
                info.reset_replaygain();
            } else if (command_index == 22) {
                pfc::string_formatter title;
                title << "ReplayGain Mixed Title " << (track_index + 1);
                info.meta_set("TITLE", title);
                set_replaygain();
            } else if (command_index == 23) {
                set_replaygain();
                info.meta_set("MI_UNSUPPORTED_CUSTOM", "Atomic ReplayGain reject");
            } else if (command_index == 24 || command_index == 31) {
                const char* current = info.meta_get("TRACKNUMBER", 0);
                if (current != nullptr && std::strcmp(current, "3") == 0) {
                    info.meta_set("TRACKNUMBER", "5");
                }
                if (command_index == 31) info.meta_set("TITLE", "Structural Mixed Title");
            } else if (command_index == 25) {
                static const char* numbers[] = { "5", "6", "7" };
                info.meta_set("TRACKNUMBER", numbers[std::min<size_t>(track_index, 2)]);
            } else if (command_index == 26 || command_index == 30) {
                const char* current = info.meta_get("TRACKNUMBER", 0);
                if (current != nullptr && std::strcmp(current, "3") == 0) {
                    info.meta_set("TRACKNUMBER", "7");
                }
                if (command_index == 30) info.meta_set("TITLE", "Atomic Structural Reject");
            } else if (command_index == 27) {
                const char* current = info.meta_get("TRACKNUMBER", 0);
                if (current != nullptr && std::strcmp(current, "1") == 0) {
                    info.meta_set("TRACKNUMBER", "7");
                } else if (current != nullptr && std::strcmp(current, "7") == 0) {
                    info.meta_set("TRACKNUMBER", "1");
                }
            } else if (command_index == 28) {
                info.meta_set("TRACKNUMBER", "A1");
            } else if (command_index == 29) {
                info.meta_remove_field("TRACKNUMBER");
            } else if (command_index == 32) {
                info.meta_set("TRACKNUMBER", "5");
            } else if (command_index == 33) {
                info.meta_set("TRACKNUMBER", "9");
            } else if (command_index == 34) {
                pfc::string_formatter unicode;
                unicode << "Gr\xc3\xbc\xc3\x9f" "e \xc3\xa9t\xc3\xa9 \xc3\x98 " << (track_index + 1);
                info.meta_set("TITLE", unicode);
            } else if (command_index == 35) {
                static const char* values[] = {
                    "NFC \xc3\xa9 | NFD e\xcc\x81 | \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e | \xf0\x9f\x98\x80 | \xf0\x9d\x84\x9e",
                    "NFD e\xcc\x81 | NFC \xc3\xa9 | semicolon; comma, slash/ pipe| \xe6\x9d\xb1\xe4\xba\xac",
                };
                info.meta_set("TITLE", values[std::min<size_t>(track_index, 1)]);
            } else if (command_index == 36) {
                pfc::string8 long_value("Long Unicode ");
                for (size_t repeat = 0; repeat < 4096; ++repeat) {
                    long_value << "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xf0\x9f\x98\x80";
                }
                long_value << " " << (track_index + 1);
                info.meta_set("TITLE", long_value);
            } else if (command_index == 37) {
                info.meta_set("TITLE", "Unsafe \"quoted\" value\nsecond line");
            } else if (command_index == 38 || command_index == 39) {
                pfc::string_formatter title;
                title << "Partial Commit Title " << (track_index + 1);
                info.meta_set("TITLE", title);
                info.meta_remove_field("STYLE");
                const size_t style = info.meta_set("STYLE", "House");
                info.meta_add_value(style, "Techno");
                if (command_index == 39) {
                    info.meta_set("MI_UNSUPPORTED_CUSTOM", "Blocking unknown field");
                }
            } else if (command_index == 40 || command_index == 42) {
                info.meta_remove_field("STYLE");
                const size_t style = info.meta_set("STYLE", "House");
                info.meta_add_value(style, "Techno");
                if (command_index == 42) set_replaygain();
            } else if (command_index == 41) {
                info.meta_remove_field("STYLE");
            } else if (command_index == 44) {
                const bool first = target_subsong == 1;
                info.meta_set("TITLE", first
                    ? "\xd7\xa2\xd7\x91\xd7\xa8\xd7\x99\xd7\xaa / English \xd7\x97\xd7\x93\xd7\xa9"
                    : "\xd8\xa7\xd9\x84\xd8\xb9\xd9\x8e\xd8\xb1\xd9\x8e\xd8\xa8\xd9\x90\xd9\x8a\xd9\x8e\xd9\x91\xd8\xa9 / Latin");
                info.meta_set("ALBUM", "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4 \xec\x95\xa8\xeb\xb2\x94");
                info.meta_set("GENRE", "\xce\x95\xce\xbb\xce\xbb\xce\xb7\xce\xbd\xce\xb9\xce\xba\xce\xac");
                info.meta_set("DATE", "\xd0\x9a\xd0\xb8\xd1\x80\xd0\xb8\xd0\xbb\xd0\xbb\xd0\xb8\xd1\x86\xd0\xb0 \xd0\xae\xd0\xbd\xd0\xb8\xd0\xba\xd0\xbe\xd0\xb4");
                info.meta_set("SONGWRITER", first
                    ? "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4 \xec\x9e\x91\xea\xb0\x80"
                    : "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e \xe4\xbd\x9c\xe5\xae\xb6");
                info.meta_set("COMMENT",
                    "\xe4\xb8\xad\xe6\x96\x87\xe5\x85\xa8\xe5\xb1\x80\xe8\xaf\x84\xe8\xae\xba");
                info.meta_remove_field("ISRC");
            } else if (command_index == 45) {
                info.meta_set("TITLE",
                    "\xd7\xa2\xd7\x91\xd7\xa8\xd7\x99\xd7\xaa / English accepted");
                info.meta_remove_field("STYLE");
                const size_t style = info.meta_set("STYLE", "\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9");
                info.meta_add_value(style, "\xe4\xb8\xad\xe6\x96\x87");
            }
            writer->set_info(target_subsong, info, fb2k::noAbort);
            ++track_index;
        }
        commit_active_writer();
    }
};

contextmenu_item_factory_t<probe_generic_shadow_command> g_probe_generic_shadow_command_factory;

class probe_edit_callback : public metadb_io_edit_callback {
public:
    void on_edited(
        metadb_handle_list_cref items,
        t_infosref before,
        t_infosref after) override {
        if (items.get_count() == 0 || after.get_count() != items.get_count()) return;

        // TEST-PROBE only: constrain the global callback to the disposable
        // external-Cue fixture. Never log arbitrary user metadata.
        const char* edited_path = items[0]->get_path();
        if (std::strstr(edited_path, "external-proxy-capture") == nullptr &&
            std::strstr(edited_path, "safe-shadow-discogger") == nullptr) return;

        pfc::string_formatter report;
        report << "edit_callback_count=" << items.get_count() << "\n";
        for (size_t index = 0; index < items.get_count(); ++index) {
            report << "item_" << index << "_subsong="
                   << items[index]->get_subsong_index() << "\n";
            report << "item_" << index << "_title_before="
                   << meta_value_or_missing(*before[index], "TITLE") << "\n";
            report << "item_" << index << "_title_after="
                   << meta_value_or_missing(*after[index], "TITLE") << "\n";
            report << "item_" << index << "_masstagger_after="
                   << meta_value_or_missing(*after[index], "MI_MASSTAGGER_TEST") << "\n";
            report << "item_" << index << "_meta_before_count="
                   << before[index]->meta_get_count() << "\n";
            for (size_t field = 0; field < before[index]->meta_get_count(); ++field) {
                report << "item_" << index << "_before_"
                       << before[index]->meta_enum_name(field) << "="
                       << before[index]->meta_enum_value(field, 0) << "\n";
            }
            report << "item_" << index << "_meta_after_count="
                   << after[index]->meta_get_count() << "\n";
            for (size_t field = 0; field < after[index]->meta_get_count(); ++field) {
                report << "item_" << index << "_after_"
                       << after[index]->meta_enum_name(field) << "="
                       << after[index]->meta_enum_value(field, 0) << "\n";
            }
        }

        try {
            write_edit_callback_report(items[0]->get_path(), report);
        } catch (const std::exception& error) {
            FB2K_console_formatter() << "Metadata Isolation edit callback report failed: "
                                    << error;
        }
    }
};

service_factory_single_t<probe_edit_callback> g_probe_edit_callback_factory;

} // namespace
} // namespace metadata_isolation
