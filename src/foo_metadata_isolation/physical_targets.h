#pragma once

namespace metadata_isolation {

struct physical_target_map {
    metadb_handle_list handles;
    size_t input_count = 0;
    size_t rejected_count = 0;
    pfc::string8 first_rejected_path;
    pfc::string8 first_rejected_reason;

    bool complete() const { return rejected_count == 0; }
    bool empty() const { return handles.get_count() == 0; }
};

physical_target_map map_to_physical_targets(metadb_handle_list_cref selected);
void format_physical_mapping_error(
    const physical_target_map& mapping,
    pfc::string_base& output);

} // namespace metadata_isolation
