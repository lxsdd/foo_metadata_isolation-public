#include "stdafx.h"
#include "guid.h"
#include "operation_log.h"
#include "physical_targets.h"
#include "physical_metadata_audit_ui.h"
#include "write_guard.h"
#include "virtual_edit_session.h"

namespace metadata_isolation {
namespace {

thread_local bool g_building_routed_context_menu = false;

class routed_menu_build_scope {
public:
    routed_menu_build_scope() { g_building_routed_context_menu = true; }
    ~routed_menu_build_scope() { g_building_routed_context_menu = false; }
};

contextmenu_node* find_command_by_name(contextmenu_node* node, const char* name) {
    if (node == nullptr) return nullptr;
    if (node->get_type() == contextmenu_item_node::TYPE_COMMAND) {
        return pfc::stricmp_ascii(node->get_name(), name) == 0 ? node : nullptr;
    }
    if (node->get_type() != contextmenu_item_node::TYPE_POPUP) return nullptr;
    for (size_t index = 0; index < node->get_num_children(); ++index) {
        if (contextmenu_node* found = find_command_by_name(node->get_child(index), name)) {
            return found;
        }
    }
    return nullptr;
}

enum command_index : unsigned {
    command_physical_properties,
    command_physical_context,
    command_virtual_cuesheet,
    command_virtual_timing,
    command_virtual_context,
    command_virtual_review,
    command_virtual_discard,
    command_protection_status,
    command_physical_audit,
};

void run_loaded_physical_command(
    unsigned index,
    const metadb_handle_list& handles,
    operation_logger logger) {
    try {
        logger.standard(
            "physical.prepare",
            {log_count("physical_sources", handles.get_count())});
        if (index == command_physical_properties) {
            logger.verbose(
                "command.begin",
                {log_token("command", "PROPERTIES")});
            if (!standard_commands::context_file_properties(
                    handles, ::contextmenu_item::caller_undefined)) {
                logger.standard(
                    "command.failed",
                    {log_token("reason", "COMMAND_UNAVAILABLE")});
                logger.finish("REJECTED", "COMMAND_UNAVAILABLE");
                popup_message::g_show(
                    "The foobar2000 Properties command could not be opened.",
                    "Metadata Isolation");
                return;
            }
            logger.verbose(
                "command.complete",
                {log_token("result", "OPENED")});
            logger.finish("COMMAND_OPENED");
            return;
        }

        logger.verbose(
            "command.begin",
            {log_token("command", "CONTEXT_MENU")});
        routed_menu_build_scope scope;
        contextmenu_manager::ptr manager = contextmenu_manager::g_create();
        manager->init_context_ex(
            handles,
            contextmenu_manager::flag_view_full,
            ::contextmenu_item::caller_undefined);
        manager->win32_run_menu_popup(core_api::get_main_window(), nullptr);
        logger.verbose(
            "command.complete",
            {log_token("result", "RETURNED")});
        logger.finish("COMMAND_RETURNED");
    } catch (const std::exception& error) {
        logger.standard(
            "command.failed",
            {log_token("reason", "COMMAND_INVOCATION_FAILED")});
        logger.finish("REJECTED", "COMMAND_INVOCATION_FAILED");
        pfc::string_formatter message;
        message << "The physical command could not be opened.\n\n" << error.what();
        popup_message::g_show(message, "Metadata Isolation");
    }
}

void run_cuesheet_editor(
    const metadb_handle_ptr& representative,
    operation_logger logger) {
    try {
        metadb_handle_list editor_target;
        editor_target.add_item(representative);
        routed_menu_build_scope scope;
        contextmenu_manager::ptr manager = contextmenu_manager::g_create();
        manager->init_context_ex(
            editor_target,
            contextmenu_manager::flag_view_full,
            ::contextmenu_item::caller_undefined);
        contextmenu_node* command = find_command_by_name(manager->get_root(), "Edit cuesheet");
        if (command == nullptr) {
            logger.standard(
                "command.failed",
                {log_token("reason", "COMMAND_UNAVAILABLE")});
            logger.finish("REJECTED", "COMMAND_UNAVAILABLE");
            popup_message::g_show(
                "foobar2000's Edit cuesheet command was not available for this target. "
                "No change was made.",
                "Metadata Isolation");
            return;
        }
        logger.verbose(
            "command.begin",
            {log_token("command", "EDIT_CUESHEET")});
        command->execute();
        logger.verbose(
            "command.complete",
            {log_token("result", "RETURNED")});
        logger.finish("COMMAND_RETURNED");
    } catch (const std::exception& error) {
        logger.standard(
            "command.failed",
            {log_token("reason", "COMMAND_INVOCATION_FAILED")});
        logger.finish("REJECTED", "COMMAND_INVOCATION_FAILED");
        pfc::string_formatter message;
        message << "The embedded Cue Sheet editor could not be opened.\n\n" << error.what();
        popup_message::g_show(message, "Metadata Isolation");
    }
}

void load_then_run_physical_command(
    unsigned index,
    const metadb_handle_list& handles,
    operation_logger logger) {
    metadb_handle_list retained = handles;
    logger.verbose(
        "physical.load",
        {log_token("state", "BEGIN")});
    completion_notify::ptr notify = fb2k::makeCompletionNotify(
        [index, retained, logger](unsigned status) {
            if (status != metadb_io::load_info_success) {
                logger.standard(
                    "physical.load",
                    {log_token("result", "FAILED"), log_token("reason", "SOURCE_LOAD_FAILED")});
                logger.finish("REJECTED", "SOURCE_LOAD_FAILED");
                popup_message::g_show(
                    "Physical file metadata could not be loaded. No command was started.",
                    "Metadata Isolation");
                return;
            }
            logger.verbose(
                "physical.load",
                {log_token("result", "PASS")});
            run_loaded_physical_command(index, retained, logger);
        });
    metadb_io_v2::get()->load_info_async(
        handles,
        metadb_io::load_info_check_if_changed,
        core_api::get_main_window(),
        metadb_io_v2::op_flag_delay_ui,
        notify);
}

contextmenu_group_popup_factory g_context_group(
    guids::context_group,
    contextmenu_groups::root,
    "Metadata Isolation",
    0);

class context_menu_item : public contextmenu_item_simple {
public:
    GUID get_parent() override { return guids::context_group; }
    unsigned get_num_items() override { return 9; }

    void get_item_name(unsigned index, pfc::string_base& output) override {
        switch (index) {
        case 0:
            output = "Physical file metadata: Properties...";
            break;
        case 1:
            output = "Physical file metadata: Run context command...";
            break;
        case 2:
            output = "Virtual subsong metadata: Edit embedded Cue Sheet...";
            break;
        case 3:
            output = "Virtual structure: Edit track timing...";
            break;
        case 4:
            output = "Virtual metadata: Run context command safely...";
            break;
        case 5:
            output = "Virtual subsong metadata: Review pending changes...";
            break;
        case 6:
            output = "Virtual subsong metadata: Discard pending session";
            break;
        case 7:
            output = "Protection status...";
            break;
        case 8:
            output = "Audit physical metadata for selection...";
            break;
        default:
            PFC_ASSERT(!"Invalid context-menu item index");
            output = "";
            break;
        }
    }

    GUID get_item_guid(unsigned index) override {
        switch (index) {
        case 0: return guids::edit_physical_tags;
        case 1: return guids::run_physical_context_command;
        case 2: return guids::edit_embedded_cuesheet;
        case 3: return guids::edit_virtual_timing;
        case 4: return guids::run_virtual_context_command;
        case 5: return guids::review_virtual_context_command;
        case 6: return guids::discard_virtual_context_command;
        case 7: return guids::show_protection_status;
        case 8: return guids::audit_context_command;
        default:
            PFC_ASSERT(!"Invalid context-menu item index");
            return pfc::guid_null;
        }
    }

    bool get_item_description(unsigned index, pfc::string_base& output) override {
        switch (index) {
        case 0:
            output = "Opens Properties for each underlying physical file. Changes affect the physical file tags even when virtual Cue tracks are selected.";
            break;
        case 1:
            output = "Runs a foobar2000 context command on each underlying physical file. Use this when the physical file tags should change.";
            break;
        case 2:
            output = "Opens foobar2000's embedded Cue Sheet editor for the selected source file. Structural edits can affect multiple virtual tracks.";
            break;
        case 3:
            output = "Edits INDEX or PREGAP timing for one virtual Cue track. The complete Cue timing is checked before the original file is replaced.";
            break;
        case 4:
            output = "Runs the chosen metadata command on the selected virtual Cue tracks. Supported changes are validated and physical file tags stay protected.";
            break;
        case 5:
            output = "Checks a pending virtual edit and applies only changes that can be represented safely. Cancel leaves the original files unchanged.";
            break;
        case 6:
            output = "Cancels the pending virtual edit. The original files remain unchanged.";
            break;
        case 7:
            output = "Shows whether protection against unsafe direct writes to virtual Cue tracks is active.";
            break;
        case 8:
            output = "Opens the read-only Physical Metadata Audit for the selected source files. It compares physical core tags with unambiguous global embedded-Cue values and never writes during the scan.";
            break;
        default:
            return false;
        }
        return true;
    }

    bool context_get_display(
        unsigned index,
        metadb_handle_list_cref data,
        pfc::string_base& output,
        unsigned& display_flags,
        const GUID& caller) override {
        if (index >= get_num_items() || data.get_count() == 0 ||
            g_building_routed_context_menu || virtual_shadow_menu_building()) return false;
        // Retain the legacy recovery implementations and stable GUIDs for
        // diagnostics, but keep transaction internals out of the normal menu UX.
        if (index == command_virtual_review || index == command_virtual_discard) return false;
        if (index == command_virtual_timing &&
            (data.get_count() != 1 || data[0]->get_subsong_index() == 0)) return false;
        if (index == command_virtual_context) {
            for (const auto& item : data) {
                if (item->get_subsong_index() == 0) return false;
            }
        }
        return contextmenu_item_simple::context_get_display(
            index, data, output, display_flags, caller);
    }

    void context_command(
        unsigned index,
        metadb_handle_list_cref data,
        const GUID&) override {
        PFC_ASSERT(index < get_num_items());
        if (index == command_physical_audit) {
            show_physical_metadata_audit_selection(data);
            return;
        }

        if (index == command_protection_status) {
            const protection_status status = get_protection_status();
            pfc::string_formatter message;
            message << "Write guard status: " << protection_status_name(status) << "\n\n";
            switch (status) {
            case protection_status::active:
                message << "An unsafe virtual write was blocked in this foobar2000 session. "
                           "This is historical evidence, not a live check of Track Info Filters.";
                break;
            case protection_status::inactive:
                message << "The Metadata Isolation write-guard setting is disabled.";
                break;
            case protection_status::unknown:
                message << "No unsafe virtual write block has been observed in this session. "
                           "This does not prove the filter is disabled; verify it under "
                           "Preferences > Track Info Filters.";
                break;
            }
            popup_message::g_show(message, "Metadata Isolation");
            return;
        }

        if (index == command_virtual_context) {
            begin_virtual_edit_session(data);
            return;
        }
        if (index == command_virtual_timing) {
            edit_virtual_timing(data);
            return;
        }
        if (index == command_virtual_review) {
            review_virtual_edit_session();
            return;
        }
        if (index == command_virtual_discard) {
            discard_virtual_edit_session(true);
            return;
        }

        const bool cue_editor = index == command_virtual_cuesheet;
        const char* operation = cue_editor ? "embedded_cuesheet_editor" :
            index == command_physical_properties ? "physical_properties" : "physical_context";
        operation_logger logger(operation, cue_editor ? "virtual_structure" : "physical");
        logger.standard(
            "session.start",
            {log_count("selected_handles", data.get_count())});

        const physical_target_map mapping = map_to_physical_targets(data);
        if (!mapping.complete() || mapping.empty()) {
            logger.standard(
                "mapping.failed",
                {log_token("reason", "PHYSICAL_MAPPING_FAILED")});
            logger.finish("REJECTED", "PHYSICAL_MAPPING_FAILED");
            pfc::string8 message;
            format_physical_mapping_error(mapping, message);
            popup_message::g_show(message, "Metadata Isolation");
            return;
        }
        logger.standard(
            "mapping.complete",
            {
                log_count("selected_handles", data.get_count()),
                log_count("deduplicated_sources", mapping.handles.get_count()),
            });
        if (cue_editor) {
            if (mapping.handles.get_count() != 1) {
                logger.standard(
                    "mapping.failed",
                    {log_token("reason", "MULTIPLE_SOURCE_DOCUMENTS")});
                logger.finish("REJECTED", "MULTIPLE_SOURCE_DOCUMENTS");
                popup_message::g_show(
                    "The foobar2000 Cue Sheet editor handles one source document at a time. "
                    "Select subsongs from exactly one physical file and retry.",
                    "Metadata Isolation");
                return;
            }
            metadb_handle_ptr representative = data[0];
            for (const auto& item : data) {
                if (item->get_subsong_index() != 0) {
                    representative = item;
                    break;
                }
            }
            run_cuesheet_editor(representative, logger);
            return;
        }
        load_then_run_physical_command(index, mapping.handles, logger);
    }
};

contextmenu_item_factory_t<context_menu_item> g_context_item_factory;

} // namespace
} // namespace metadata_isolation
