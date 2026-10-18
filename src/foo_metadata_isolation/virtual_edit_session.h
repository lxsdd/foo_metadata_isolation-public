#pragma once

#include "cue_patch.h"

namespace metadata_isolation {

bool virtual_edit_session_pending();
bool virtual_shadow_menu_building();
bool capture_virtual_shadow_write(
    const playable_location& location,
    const file_info& postimage);
void begin_virtual_edit_session(metadb_handle_list_cref selection);
void review_virtual_edit_session();
void discard_virtual_edit_session(bool notify_user = true);
void shutdown_virtual_edit_session();
void cleanup_stale_virtual_edit_sessions();
void edit_virtual_timing(metadb_handle_list_cref selection);

struct virtual_timing_edit_request {
    metadb_handle_ptr target;
    cue_timing_edit edit;
};
#if defined(FB2K_ENABLE_TEST_PROBES)
bool run_virtual_edit_session_diagnostic(
    metadb_handle_list_cref selection,
    const char* context_command_name,
    bool commit,
    pfc::string_base& report,
    bool fail_after_first_install = false,
    bool apply_supported_changes = true);
bool run_virtual_projection_benchmark_diagnostic(
    metadb_handle_list_cref selection,
    bool complete_transaction,
    pfc::string_base& report);
bool begin_virtual_edit_session_diagnostic(
    metadb_handle_list_cref selection,
    const char* context_command_name,
    pfc::string_base& report);
bool finish_virtual_edit_session_diagnostic(
    bool commit,
    pfc::string_base& report);
bool run_virtual_timing_diagnostic(
    const std::vector<virtual_timing_edit_request>& requests,
    bool commit,
    pfc::string_base& report,
    bool fail_after_first_install = false);
bool begin_virtual_timing_diagnostic(
    const std::vector<virtual_timing_edit_request>& requests,
    pfc::string_base& report);
bool finish_virtual_timing_diagnostic(bool commit, pfc::string_base& report);
#endif

} // namespace metadata_isolation
