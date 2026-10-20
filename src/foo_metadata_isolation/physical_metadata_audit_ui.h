#pragma once

namespace metadata_isolation {

// Opens the modeless read-only audit for the complete Media Library.
void show_physical_metadata_audit_library();

// Opens the same audit surface scoped to the current foobar selection.
void show_physical_metadata_audit_selection(metadb_handle_list_cref selection);

// Called from initquit::on_quit while foobar services are still valid.
void shutdown_physical_metadata_audit();

#if defined(FB2K_ENABLE_TEST_PROBES)
// Test-build only: aggregates, no file paths or raw tag values.
void read_audit_internal_state_for_test(pfc::string_base& output);
#endif

} // namespace metadata_isolation
