#pragma once

namespace metadata_isolation {

// Opens the modeless read-only audit for the complete Media Library.
void show_physical_metadata_audit_library();

// Opens the same audit surface scoped to the current foobar selection.
void show_physical_metadata_audit_selection(metadb_handle_list_cref selection);

// Called from initquit::on_quit while foobar services are still valid.
void shutdown_physical_metadata_audit();

} // namespace metadata_isolation
