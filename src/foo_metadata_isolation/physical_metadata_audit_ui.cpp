#include "stdafx.h"
#include "guid.h"
#include "operation_log.h"
#include "physical_metadata_audit.h"
#include "physical_metadata_audit_ui.h"
#include "physical_targets.h"
#include "resource.h"
#include "ui_scaling.h"
#include "ui_tooltips.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <set>
#include <sstream>
#include <vector>

namespace metadata_isolation {
namespace {

constexpr UINT WM_AUDIT_START = WM_APP + 0x61;
constexpr UINT WM_AUDIT_PROGRESS = WM_APP + 0x62;
constexpr UINT WM_AUDIT_DONE = WM_APP + 0x63;

constexpr int k_filter_problems = 0;
constexpr int k_filter_all = 1;
constexpr int k_filter_empty = 2;
constexpr int k_filter_incomplete = 3;
constexpr int k_filter_mismatch = 4;
constexpr int k_filter_review = 5;
constexpr int k_filter_ok = 6;

enum audit_column : int {
    column_status = 0,
    column_file,
    column_folder,
    column_format,
    column_artist,
    column_album_artist,
    column_album,
    column_date,
    column_genre,
    column_cue_artist,
    column_cue_album,
    column_cue_date,
    column_cue_genre,
    column_missing,
    column_cue_comparison,
    column_count,
};

struct column_definition {
    const wchar_t* title;
    int default_width;
    bool default_visible;
};

constexpr column_definition k_columns[column_count] = {
    {L"Status", 82, true},
    {L"File", 190, true},
    {L"Folder", 220, false},
    {L"Format", 62, false},
    {L"Artist", 150, true},
    {L"Album Artist", 150, true},
    {L"Album", 190, true},
    {L"Date", 82, true},
    {L"Genre", 120, true},
    {L"Cue Performer", 150, false},
    {L"Cue Album", 190, false},
    {L"Cue Date", 82, false},
    {L"Cue Genre", 120, false},
    {L"Missing core fields", 200, true},
    {L"Cue comparison", 230, true},
};

cfg_int g_cfg_audit_win_x(guids::cfg_audit_win_x, -1);
cfg_int g_cfg_audit_win_y(guids::cfg_audit_win_y, -1);
cfg_int g_cfg_audit_win_w(guids::cfg_audit_win_w, 1080);
cfg_int g_cfg_audit_win_h(guids::cfg_audit_win_h, 600);
cfg_bool g_cfg_audit_win_max(guids::cfg_audit_win_max, false);
cfg_int g_cfg_audit_filter(guids::cfg_audit_filter, k_filter_problems);
cfg_int g_cfg_audit_sort_col(guids::cfg_audit_sort_col, column_status);
cfg_int g_cfg_audit_sort_dir(guids::cfg_audit_sort_dir, 1);
cfg_string g_cfg_audit_columns(guids::cfg_audit_columns, "");
cfg_string g_cfg_audit_column_state(guids::cfg_audit_column_state, "");

HWND g_audit_window = nullptr;

#if defined(FB2K_ENABLE_TEST_PROBES)
// Only synthetic CI snapshots. Never record paths, metadata or user values.
std::atomic_size_t g_audit_probe_model_rows{0};
std::atomic_size_t g_audit_probe_filtered_rows{0};
std::atomic_int g_audit_probe_native_rows{-1};
std::atomic_int g_audit_probe_filter{-1};
std::atomic_bool g_audit_probe_list_visible{false};
#endif

enum class audit_scope {
    library,
    selection,
};

struct audit_path_less {
    bool operator()(const std::string& left, const std::string& right) const {
        return metadb::path_compare(left.c_str(), right.c_str()) < 0;
    }
};


void show_error(const char* heading, const std::string& detail) {
    pfc::string_formatter message;
    message << heading;
    if (!detail.empty()) message << "\n\n" << detail.c_str();
    popup_message::g_show(message, "Metadata Isolation");
}

std::vector<int> parse_int_list(const char* value) {
    std::vector<int> output;
    if (value == nullptr) return output;
    std::stringstream stream(value);
    std::string part;
    while (std::getline(stream, part, ',')) {
        try {
            output.push_back(std::stoi(part));
        } catch (...) {
            return {};
        }
    }
    return output;
}

std::string join_int_list(const std::vector<int>& values) {
    std::ostringstream stream;
    for (size_t index = 0; index < values.size(); ++index) {
        if (index != 0) stream << ',';
        stream << values[index];
    }
    return stream.str();
}

void set_text(HWND window, int id, const char* utf8) {
    pfc::stringcvt::string_wide_from_utf8 wide(utf8 == nullptr ? "" : utf8);
    SetDlgItemTextW(window, id, wide);
}

std::string cue_comparison_text(const physical_audit_row& row) {
    if (row.review_reason.empty()) return row.cue_differences;
    if (row.cue_differences.empty()) return std::string("Review: ") + row.review_reason;
    return row.cue_differences + "; Review: " + row.review_reason;
}

std::string tsv_cell(std::string value) {
    for (char& ch : value) {
        if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
    }
    return value;
}

std::string row_tooltip_text(const physical_audit_row& row) {
    std::ostringstream output;
    output << physical_audit_status_name(row.status) << " - " << row.file_name;
    if (!row.missing_fields.empty()) output << "\nMissing: " << row.missing_fields;
    const std::string comparison = cue_comparison_text(row);
    if (!comparison.empty()) output << "\nCue comparison: " << comparison;
    if (!row.album.empty()) output << "\nAlbum: " << row.album;
    if (!row.album_artist.empty()) output << "\nAlbum Artist: " << row.album_artist;
    else if (!row.artist.empty()) output << "\nArtist: " << row.artist;
    if (!row.date.empty()) output << "\nDate: " << row.date;
    if (!row.genre.empty()) output << "\nGenre: " << row.genre;
    if (!row.cue_artist.empty()) output << "\nCue Performer: " << row.cue_artist;
    if (!row.cue_album.empty()) output << "\nCue Album: " << row.cue_album;
    if (!row.cue_date.empty()) output << "\nCue Date: " << row.cue_date;
    if (!row.cue_genre.empty()) output << "\nCue Genre: " << row.cue_genre;
    if (!row.folder.empty()) output << "\nFolder: " << row.folder;
    return output.str();
}

const std::string& column_value_ref(
    const physical_audit_row& row,
    int column,
    std::string& scratch) {
    switch (column) {
    case column_file: return row.file_name;
    case column_folder: return row.folder;
    case column_format: return row.format;
    case column_artist: return row.artist;
    case column_album_artist: return row.album_artist;
    case column_album: return row.album;
    case column_date: return row.date;
    case column_genre: return row.genre;
    case column_cue_artist: return row.cue_artist;
    case column_cue_album: return row.cue_album;
    case column_cue_date: return row.cue_date;
    case column_cue_genre: return row.cue_genre;
    case column_missing: return row.missing_fields;
    case column_cue_comparison:
        scratch = cue_comparison_text(row);
        return scratch;
    case column_status:
    default:
        scratch = physical_audit_status_name(row.status);
        return scratch;
    }
}

bool filter_matches(const physical_audit_row& row, int filter) {
    switch (filter) {
    case k_filter_all: return true;
    case k_filter_empty: return row.status == physical_audit_status::empty;
    case k_filter_incomplete: return row.status == physical_audit_status::incomplete;
    case k_filter_mismatch: return row.status == physical_audit_status::mismatch;
    case k_filter_review: return row.status == physical_audit_status::review;
    case k_filter_ok: return row.status == physical_audit_status::ok;
    case k_filter_problems:
    default:
        return physical_audit_status_is_problem(row.status);
    }
}

int compare_rows(const physical_audit_row& left, const physical_audit_row& right, int column) {
    if (column == column_status) {
        const int a = static_cast<int>(left.status);
        const int b = static_cast<int>(right.status);
        if (a < b) return -1;
        if (a > b) return 1;
        return metadb::path_compare(left.path.c_str(), right.path.c_str());
    }
    std::string left_scratch;
    std::string right_scratch;
    const std::string& a = column_value_ref(left, column, left_scratch);
    const std::string& b = column_value_ref(right, column, right_scratch);
    const int value = stricmp_utf8(a.c_str(), b.c_str());
    if (value != 0) return value;
    return metadb::path_compare(left.path.c_str(), right.path.c_str());
}

struct scan_state {
    explicit scan_state(audit_scope source_scope)
        : scope(source_scope), logger("physical_metadata_audit", "PHYSICAL_READ_ONLY") {}

    audit_scope scope;
    metadb_handle_list source_items;
    physical_audit_batch batch;
    size_t mapping_rejected = 0;
    std::atomic_size_t progress_done{0};
    std::atomic_size_t progress_total{0};
    std::atomic_bool cancel_requested{false};
    std::string error;
    operation_logger logger;
};

class audit_window {
public:
    audit_window(audit_scope scope, metadb_handle_list_cref seed)
        : m_scope(scope), m_seed(seed) {
        for (int index = 0; index < column_count; ++index) {
            m_column_visible[index] = k_columns[index].default_visible;
            m_column_widths[index] = k_columns[index].default_width;
        }
    }

    int show_command() const noexcept {
        return g_cfg_audit_win_max ? SW_SHOWMAXIMIZED : SW_SHOW;
    }

    INT_PTR on_init(HWND window) {
        m_window = window;
        m_list = GetDlgItem(window, IDC_AUDIT_RESULTS);
        m_progress = GetDlgItem(window, IDC_AUDIT_PROGRESS);
        m_dpi = ui::detect_window_dpi(window);
        if (m_dpi == 0) m_dpi = 96;

        INITCOMMONCONTROLSEX controls = {
            sizeof(controls),
            ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_WIN95_CLASSES};
        InitCommonControlsEx(&controls);

        const HWND main_window = core_api::get_main_window();
        const HICON big_icon = reinterpret_cast<HICON>(
            SendMessageW(main_window, WM_GETICON, ICON_BIG, 0));
        const HICON small_icon = reinterpret_cast<HICON>(
            SendMessageW(main_window, WM_GETICON, ICON_SMALL, 0));
        if (big_icon != nullptr) SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big_icon));
        if (small_icon != nullptr) SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));

        m_dark.AddDialogWithControls(window);
        m_tooltips.initialize(window, physical_audit_tooltip_definitions());
        if (m_tooltips.window() != nullptr) m_dark.AddCtrlAuto(m_tooltips.window());

        ListView_SetExtendedListViewStyleEx(
            m_list,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_LABELTIP |
                LVS_EX_INFOTIP,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_LABELTIP |
                LVS_EX_INFOTIP);

        for (int index = 0; index < column_count; ++index) {
            LVCOLUMNW column = {};
            column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            column.fmt = LVCFMT_LEFT;
            column.iSubItem = index;
            column.pszText = const_cast<wchar_t*>(k_columns[index].title);
            column.cx = ui::scale_for_window_dpi(window, k_columns[index].default_width);
            ListView_InsertColumn(m_list, index, &column);
        }
        m_tooltips.add_control(
            window,
            ListView_GetHeader(m_list),
            L"Click a header to sort. Right-click a header to show or hide columns. "
            L"Physical tag columns show subsong-0 values; Cue columns show only "
            L"unambiguous global Cue values used for exact comparison.");

        restore_columns();
        setup_filter();
        restore_geometry();
        layout();

        EnableWindow(GetDlgItem(window, IDC_AUDIT_CANCEL), FALSE);
        EnableWindow(GetDlgItem(window, IDC_AUDIT_EDIT_PHYSICAL), FALSE);
        ShowWindow(m_progress, SW_HIDE);
        set_text(
            window,
            IDC_AUDIT_STATUS,
            m_scope == audit_scope::library
                ? "Ready to audit embedded-Cue files in the Media Library."
                : "Ready to audit the selected physical source files.");

        modeless_dialog_manager::g_add(window);
        m_modeless_registered = true;
        PostMessageW(window, WM_AUDIT_START, 0, 0);
        return TRUE;
    }

    void request_scope(audit_scope scope, metadb_handle_list_cref seed) {
        if (m_scan) {
            m_pending_scope = scope;
            m_pending_seed = seed;
            m_pending_scan = true;
            cancel_scan();
            set_text(
                m_window,
                IDC_AUDIT_STATUS,
                "Changing audit scope; cancelling the current read-only scan...");
            return;
        }
        m_scope = scope;
        m_seed = seed;
        on_start_scan();
    }

    void on_start_scan() {
        if (m_scan) return;

        auto state = std::make_shared<scan_state>(m_scope);
        if (m_scope == audit_scope::library) {
            try {
                library_manager::get()->get_all_items(state->source_items);
            } catch (const std::exception& error) {
                show_error("The Media Library could not be enumerated.", error.what());
                return;
            }
        } else {
            state->source_items = m_seed;
        }

        if (state->source_items.get_count() == 0) {
            set_text(m_window, IDC_AUDIT_STATUS, "Nothing to audit in the current scope.");
            return;
        }

        m_scan = state;
        m_close_when_done = false;
        set_scan_busy(true);
        EnableWindow(GetDlgItem(m_window, IDC_AUDIT_RESCAN), FALSE);
        EnableWindow(GetDlgItem(m_window, IDC_AUDIT_CANCEL), TRUE);
        EnableWindow(GetDlgItem(m_window, IDC_AUDIT_EDIT_PHYSICAL), FALSE);
        set_text(m_window, IDC_AUDIT_STATUS, "Scanning physical metadata...");

        state->logger.standard(
            "audit.begin",
            {
                log_token("scope", m_scope == audit_scope::library ? "LIBRARY" : "SELECTION"),
                log_count("input_handles", state->source_items.get_count()),
                log_flag("read_only", true),
            });

        const HWND target_window = m_window;
        service_ptr_t<threaded_process_callback_lambda> worker;

        // threaded_process_callback_lambda::create() overload order is init/run/done;
        // construct explicitly so completion is guaranteed on the main thread.
        worker = threaded_process_callback_lambda::create(
            [](threaded_process_callback::ctx_t) {},
            [state, target_window](threaded_process_status& status, abort_callback& abort) {
                try {
                    PostMessageW(target_window, WM_AUDIT_PROGRESS, 0, 0);
                    metadb_handle_list candidates;
                    std::set<std::string, audit_path_less> candidate_paths;
                    const size_t source_count = state->source_items.get_count();
                    status.set_progress(0, source_count);
                    for (size_t index = 0; index < source_count; ++index) {
                        abort.check();
                        if (state->cancel_requested.load()) throw exception_aborted();
                        const metadb_handle_ptr item = state->source_items[index];
                        bool include = state->scope == audit_scope::selection;
                        if (!include && item->get_subsong_index() != 0) include = true;
                        if (!include) {
                            metadb_info_container::ptr info;
                            if (item->get_info_ref(info) &&
                                info->info().meta_find("CUESHEET") != SIZE_MAX) {
                                include = true;
                            }
                        }
                        if (include && candidate_paths.emplace(item->get_path()).second) {
                            candidates.add_item(item);
                        }
                    }
                    const physical_target_map mapping = map_to_physical_targets(candidates);
                    state->mapping_rejected = mapping.rejected_count;
                    state->progress_total.store(mapping.handles.get_count());
                    PostMessageW(
                        target_window,
                        WM_AUDIT_PROGRESS,
                        0,
                        static_cast<LPARAM>(mapping.handles.get_count()));
                    state->batch = audit_physical_metadata(
                        mapping.handles,
                        status,
                        abort,
                        [state, target_window](size_t done, size_t total) {
                            if (state->cancel_requested.load()) throw exception_aborted();
                            state->progress_done.store(done);
                            state->progress_total.store(total);
                            if ((done & 0x0Fu) == 0 || done == total) {
                                PostMessageW(
                                    target_window,
                                    WM_AUDIT_PROGRESS,
                                    static_cast<WPARAM>(done),
                                    static_cast<LPARAM>(total));
                            }
                        });
                } catch (const exception_aborted&) {
                    throw;
                } catch (const std::exception& error) {
                    state->error = error.what();
                }
            },
            [state, target_window](threaded_process_callback::ctx_t, bool aborted) {
                PostMessageW(target_window, WM_AUDIT_DONE, aborted ? 1 : 0, 0);
            });

        threaded_process::g_run_modeless(
            worker,
            threaded_process::flag_silent,
            m_window,
            "Metadata Isolation physical metadata audit");
    }

    void on_progress(size_t done, size_t total) {
        if (!m_scan) return;
        pfc::string_formatter text;
        if (total == 0) {
            text << "Finding embedded-Cue physical sources...";
        } else {
            text << "Scanning physical metadata: " << done << " / " << total;
        }
        set_text(m_window, IDC_AUDIT_STATUS, text.c_str());
    }

    void on_scan_done(bool aborted) {
        if (!m_scan) return;
        const auto state = m_scan;
        m_scan.reset();
        set_scan_busy(false);

        EnableWindow(GetDlgItem(m_window, IDC_AUDIT_RESCAN), TRUE);
        EnableWindow(GetDlgItem(m_window, IDC_AUDIT_CANCEL), FALSE);

        if (aborted || state->cancel_requested.load()) {
            state->logger.finish(
                "CANCELLED",
                m_pending_scan ? "SCOPE_CHANGED" : "USER_CANCELLED",
                {log_flag("read_only", true)});
            if (m_close_when_done) {
                save_state();
                DestroyWindow(m_window);
                return;
            }
            if (m_pending_scan) {
                m_scope = m_pending_scope;
                m_seed = m_pending_seed;
                m_pending_seed.remove_all();
                m_pending_scan = false;
                set_text(m_window, IDC_AUDIT_STATUS, "Starting requested audit scope...");
                on_start_scan();
                return;
            }
            set_text(m_window, IDC_AUDIT_STATUS, "Audit cancelled. No metadata was written.");
            return;
        }
        if (!state->error.empty()) {
            set_text(m_window, IDC_AUDIT_STATUS, "Audit failed. No metadata was written.");
            state->logger.finish(
                "FAILED",
                "AUDIT_READ_FAILED",
                {log_flag("read_only", true)});
            show_error("The physical metadata audit failed.", state->error);
            if (m_close_when_done) { save_state(); DestroyWindow(m_window); }
            return;
        }

        m_rows = std::move(state->batch.rows);
        refresh_list(false);

        size_t ok = 0, empty = 0, incomplete = 0, mismatch = 0, review = 0;
        for (const auto& row : m_rows) {
            switch (row.status) {
            case physical_audit_status::ok: ++ok; break;
            case physical_audit_status::empty: ++empty; break;
            case physical_audit_status::incomplete: ++incomplete; break;
            case physical_audit_status::mismatch: ++mismatch; break;
            case physical_audit_status::review: ++review; break;
            }
        }

        pfc::string_formatter summary;
        summary << "Audited " << state->batch.textual_cue_sources
                << " embedded-Cue physical files: "
                << (empty + incomplete + mismatch + review) << " problems, "
                << ok << " OK";
        if (state->mapping_rejected != 0) {
            summary << "; " << state->mapping_rejected << " non-local/unmappable items skipped";
        }
        if (state->batch.read_failures != 0) {
            summary << "; " << state->batch.read_failures << " read failures require review";
        }
        set_text(m_window, IDC_AUDIT_STATUS, summary.c_str());

        state->logger.finish(
            "SUCCESS",
            nullptr,
            {
                log_count("mapped_sources", state->batch.mapped_sources),
                log_count("embedded_cue_sources", state->batch.textual_cue_sources),
                log_count("problems", empty + incomplete + mismatch + review),
                log_count("ok", ok),
                log_count("empty", empty),
                log_count("incomplete", incomplete),
                log_count("mismatch", mismatch),
                log_count("review", review),
                log_count("mapping_rejected", state->mapping_rejected),
                log_count("read_failures", state->batch.read_failures),
                log_flag("read_only", true),
            });

        update_edit_enabled();
        if (m_close_when_done) { save_state(); DestroyWindow(m_window); }
    }

    void set_scan_busy(bool active) {
        if (m_progress == nullptr) return;
        SendMessageW(m_progress, PBM_SETMARQUEE, active ? TRUE : FALSE, active ? 30 : 0);
        ShowWindow(m_progress, active ? SW_SHOW : SW_HIDE);
    }

    void cancel_scan() {
        if (!m_scan) return;
        m_scan->cancel_requested.store(true);
        EnableWindow(GetDlgItem(m_window, IDC_AUDIT_CANCEL), FALSE);
        set_text(m_window, IDC_AUDIT_STATUS, "Cancelling read-only audit...");
    }

    void close() {
        if (m_scan) {
            m_close_when_done = true;
            cancel_scan();
            return;
        }
        save_state();
        DestroyWindow(m_window);
    }

    void edit_selected_physical() {
        if (m_scan) return;
        metadb_handle_list selected;
        int item = -1;
        while ((item = ListView_GetNextItem(m_list, item, LVNI_SELECTED)) >= 0) {
            LVITEMW entry = {};
            entry.mask = LVIF_PARAM;
            entry.iItem = item;
            if (!ListView_GetItem(m_list, &entry)) continue;
            const size_t row_index = static_cast<size_t>(entry.lParam);
            if (row_index < m_rows.size() && m_rows[row_index].target.is_valid()) {
                selected.add_item(m_rows[row_index].target);
            }
        }
        if (selected.get_count() == 0) return;
        if (!standard_commands::context_file_properties(selected, ::contextmenu_item::caller_undefined)) {
            popup_message::g_show(
                "foobar2000 did not open Properties for the selected physical files.",
                "Metadata Isolation");
        }
    }

    void copy_selected() {
        if (ListView_GetSelectedCount(m_list) == 0) return;
        std::string output =
            "Status\tFile\tFolder\tFormat\tArtist\tAlbum Artist\tAlbum\tDate\tGenre\tCue Performer\tCue Album\tCue Date\tCue Genre\tMissing core fields\tCue comparison\r\n";
        int item = -1;
        while ((item = ListView_GetNextItem(m_list, item, LVNI_SELECTED)) >= 0) {
            LVITEMW entry = {};
            entry.mask = LVIF_PARAM;
            entry.iItem = item;
            if (!ListView_GetItem(m_list, &entry)) continue;
            const size_t row_index = static_cast<size_t>(entry.lParam);
            if (row_index >= m_rows.size()) continue;
            const auto& row = m_rows[row_index];
            const std::string issue = cue_comparison_text(row);
            output += physical_audit_status_name(row.status);
            output += '\t'; output += tsv_cell(row.file_name);
            output += '\t'; output += tsv_cell(row.folder);
            output += '\t'; output += tsv_cell(row.format);
            output += '\t'; output += tsv_cell(row.artist);
            output += '\t'; output += tsv_cell(row.album_artist);
            output += '\t'; output += tsv_cell(row.album);
            output += '\t'; output += tsv_cell(row.date);
            output += '\t'; output += tsv_cell(row.genre);
            output += '\t'; output += tsv_cell(row.cue_artist);
            output += '\t'; output += tsv_cell(row.cue_album);
            output += '\t'; output += tsv_cell(row.cue_date);
            output += '\t'; output += tsv_cell(row.cue_genre);
            output += '\t'; output += tsv_cell(row.missing_fields);
            output += '\t'; output += tsv_cell(issue);
            output += "\r\n";
        }
        if (output.empty()) return;

        pfc::stringcvt::string_wide_from_utf8 wide(output.c_str());
        const size_t bytes = (wcslen(static_cast<const wchar_t*>(wide)) + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (memory == nullptr) return;
        void* destination = GlobalLock(memory);
        if (destination == nullptr) {
            GlobalFree(memory);
            return;
        }
        memcpy(destination, static_cast<const wchar_t*>(wide), bytes);
        GlobalUnlock(memory);

        if (!OpenClipboard(m_window)) {
            GlobalFree(memory);
            return;
        }
        EmptyClipboard();
        if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) GlobalFree(memory);
        CloseClipboard();
    }

    void select_all() {
        for (int index = 0, count = ListView_GetItemCount(m_list); index < count; ++index) {
            ListView_SetItemState(m_list, index, LVIS_SELECTED, LVIS_SELECTED);
        }
        update_edit_enabled();
    }

    void on_column_click(int column) {
        int direction = g_cfg_audit_sort_dir;
        if (g_cfg_audit_sort_col == column) direction = -direction;
        else direction = 1;
        g_cfg_audit_sort_col = column;
        g_cfg_audit_sort_dir = direction;
        refresh_list(true);
    }

    void on_filter_changed() {
        const int value = static_cast<int>(
            SendDlgItemMessageW(m_window, IDC_AUDIT_FILTER, CB_GETCURSEL, 0, 0));
        if (value < k_filter_problems || value > k_filter_ok) return;
        g_cfg_audit_filter = value;
        refresh_list(true);
    }

    void update_edit_enabled() {
        EnableWindow(
            GetDlgItem(m_window, IDC_AUDIT_EDIT_PHYSICAL),
            !m_scan && ListView_GetSelectedCount(m_list) != 0);
    }

    void show_column_menu(POINT point) {
        HMENU menu = CreatePopupMenu();
        if (menu == nullptr) return;
        int visible_count = 0;
        for (bool visible : m_column_visible) if (visible) ++visible_count;
        for (int index = 0; index < column_count; ++index) {
            UINT flags = MF_STRING | (m_column_visible[index] ? MF_CHECKED : MF_UNCHECKED);
            if (m_column_visible[index] && visible_count == 1) flags |= MF_GRAYED;
            AppendMenuW(menu, flags, 43000 + index, k_columns[index].title);
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 43100, L"Reset columns");
        const int command = TrackPopupMenu(
            menu,
            TPM_RETURNCMD | TPM_RIGHTBUTTON,
            point.x,
            point.y,
            0,
            m_window,
            nullptr);
        DestroyMenu(menu);
        if (command >= 43000 && command < 43000 + column_count) {
            toggle_column(command - 43000);
        } else if (command == 43100) {
            reset_columns();
        }
    }

    void toggle_column(int column) {
        if (column < 0 || column >= column_count) return;
        int visible_count = 0;
        for (bool visible : m_column_visible) if (visible) ++visible_count;
        if (m_column_visible[column] && visible_count <= 1) return;
        if (m_column_visible[column]) {
            const int width = ListView_GetColumnWidth(m_list, column);
            if (width > 0) m_column_widths[column] = MulDiv(width, 96, static_cast<int>(m_dpi));
            m_column_visible[column] = false;
            ListView_SetColumnWidth(m_list, column, 0);
        } else {
            m_column_visible[column] = true;
            ListView_SetColumnWidth(
                m_list,
                column,
                MulDiv(m_column_widths[column], static_cast<int>(m_dpi), 96));
        }
        save_columns();
    }

    void reset_columns() {
        std::vector<int> order(column_count);
        for (int index = 0; index < column_count; ++index) {
            order[index] = index;
            m_column_visible[index] = k_columns[index].default_visible;
            m_column_widths[index] = k_columns[index].default_width;
            ListView_SetColumnWidth(
                m_list,
                index,
                m_column_visible[index]
                    ? MulDiv(k_columns[index].default_width, static_cast<int>(m_dpi), 96)
                    : 0);
        }
        ListView_SetColumnOrderArray(m_list, column_count, order.data());
        g_cfg_audit_sort_col = column_status;
        g_cfg_audit_sort_dir = 1;
        save_columns();
        refresh_list(true);
    }

    void save_columns() {
        std::vector<int> widths(column_count);
        for (int index = 0; index < column_count; ++index) {
            if (m_column_visible[index]) {
                const int width = ListView_GetColumnWidth(m_list, index);
                if (width > 0) m_column_widths[index] =
                    MulDiv(width, 96, static_cast<int>(m_dpi));
            }
            widths[index] = m_column_widths[index];
        }
        g_cfg_audit_columns = join_int_list(widths).c_str();

        std::vector<int> order(column_count);
        if (!ListView_GetColumnOrderArray(m_list, column_count, order.data())) {
            for (int index = 0; index < column_count; ++index) order[index] = index;
        }
        unsigned visibility_mask = 0;
        for (int index = 0; index < column_count; ++index) {
            if (m_column_visible[index]) visibility_mask |= (1u << index);
        }
        std::ostringstream state;
        state << visibility_mask << "|" << join_int_list(order);
        g_cfg_audit_column_state = state.str().c_str();
    }

    void restore_columns() {
        const pfc::string8 saved_widths = g_cfg_audit_columns.get();
        const std::vector<int> widths = parse_int_list(saved_widths.get_ptr());
        if (widths.size() == column_count) {
            for (int index = 0; index < column_count; ++index) {
                if (widths[index] >= 24 && widths[index] <= 2000) {
                    m_column_widths[index] = widths[index];
                }
            }
        }

        const pfc::string8 saved_state = g_cfg_audit_column_state.get();
        std::string state(saved_state.get_ptr());
        const size_t separator = state.find('|');
        std::vector<int> order;
        if (separator != std::string::npos) {
            try {
                const unsigned mask = static_cast<unsigned>(
                    std::stoul(state.substr(0, separator)));
                for (int index = 0; index < column_count; ++index) {
                    m_column_visible[index] = (mask & (1u << index)) != 0;
                }
                order = parse_int_list(state.substr(separator + 1).c_str());
            } catch (...) {
                order.clear();
            }
        }
        int visible_count = 0;
        for (bool visible : m_column_visible) if (visible) ++visible_count;
        if (visible_count == 0) {
            for (int index = 0; index < column_count; ++index) {
                m_column_visible[index] = k_columns[index].default_visible;
            }
        }

        for (int index = 0; index < column_count; ++index) {
            ListView_SetColumnWidth(
                m_list,
                index,
                m_column_visible[index]
                    ? MulDiv(m_column_widths[index], static_cast<int>(m_dpi), 96)
                    : 0);
        }
        if (order.size() == column_count) {
            std::vector<bool> seen(column_count, false);
            bool valid = true;
            for (int value : order) {
                if (value < 0 || value >= column_count || seen[value]) {
                    valid = false;
                    break;
                }
                seen[value] = true;
            }
            if (valid) ListView_SetColumnOrderArray(m_list, column_count, order.data());
        }
        update_sort_indicator();
    }

    void setup_filter() {
        constexpr const wchar_t* filters[] = {
            L"Problems",
            L"All",
            L"Empty",
            L"Incomplete",
            L"Mismatch",
            L"Review",
            L"OK",
        };
        for (const auto* filter : filters) {
            SendDlgItemMessageW(
                m_window, IDC_AUDIT_FILTER, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(filter));
        }
        int selected = g_cfg_audit_filter;
        if (selected < k_filter_problems || selected > k_filter_ok) selected = k_filter_problems;
        SendDlgItemMessageW(m_window, IDC_AUDIT_FILTER, CB_SETCURSEL, selected, 0);
    }

    void refresh_list(bool preserve_selection) {
        std::set<std::string> selected_paths;
        if (preserve_selection) {
            int item = -1;
            while ((item = ListView_GetNextItem(m_list, item, LVNI_SELECTED)) >= 0) {
                LVITEMW entry = {};
                entry.mask = LVIF_PARAM;
                entry.iItem = item;
                if (ListView_GetItem(m_list, &entry)) {
                    const size_t row_index = static_cast<size_t>(entry.lParam);
                    if (row_index < m_rows.size()) selected_paths.insert(m_rows[row_index].path);
                }
            }
        }

        std::vector<size_t> visible;
        visible.reserve(m_rows.size());
        const int filter = g_cfg_audit_filter;
        for (size_t index = 0; index < m_rows.size(); ++index) {
            if (filter_matches(m_rows[index], filter)) visible.push_back(index);
        }

        const int sort_column = std::clamp<int>(g_cfg_audit_sort_col, 0, column_count - 1);
        const int sort_direction = g_cfg_audit_sort_dir < 0 ? -1 : 1;
        std::stable_sort(
            visible.begin(),
            visible.end(),
            [this, sort_column, sort_direction](size_t left, size_t right) {
                const int result = compare_rows(m_rows[left], m_rows[right], sort_column);
                return sort_direction > 0 ? result < 0 : result > 0;
            });

        // ListView sends synchronous LVN_ITEMCHANGED while rows are replaced.
        // Updating selection-dependent buttons from inside that notification
        // re-enters the same control. Defer the single update until the batch
        // finishes, including during exceptions.
        struct refresh_scope {
            bool& active;
            explicit refresh_scope(bool& target) : active(target) { active = true; }
            ~refresh_scope() { active = false; }
        } rebuilding(m_refreshing_list);

        SendMessageW(m_list, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(m_list);
        for (size_t display_index = 0; display_index < visible.size(); ++display_index) {
            const size_t row_index = visible[display_index];
            const auto& row = m_rows[row_index];
            pfc::stringcvt::string_wide_from_utf8 status(physical_audit_status_name(row.status));
            LVITEMW item = {};
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.iItem = static_cast<int>(display_index);
            item.iSubItem = 0;
            item.pszText = const_cast<wchar_t*>(static_cast<const wchar_t*>(status));
            item.lParam = static_cast<LPARAM>(row_index);
            const int inserted = ListView_InsertItem(m_list, &item);
            if (inserted < 0) continue;

            const std::string values[] = {
                physical_audit_status_name(row.status),
                row.file_name,
                row.folder,
                row.format,
                row.artist,
                row.album_artist,
                row.album,
                row.date,
                row.genre,
                row.cue_artist,
                row.cue_album,
                row.cue_date,
                row.cue_genre,
                row.missing_fields,
                cue_comparison_text(row),
            };
            for (int column = 1; column < column_count; ++column) {
                pfc::stringcvt::string_wide_from_utf8 wide(values[column].c_str());
                ListView_SetItemText(
                    m_list, inserted, column,
                    const_cast<wchar_t*>(static_cast<const wchar_t*>(wide)));
            }
            if (selected_paths.count(row.path) != 0) {
                ListView_SetItemState(
                    m_list, inserted, LVIS_SELECTED, LVIS_SELECTED);
            }
        }
        SendMessageW(m_list, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(m_list, nullptr, TRUE);
        update_sort_indicator();
        update_edit_enabled();
#if defined(FB2K_ENABLE_TEST_PROBES)
        g_audit_probe_model_rows.store(m_rows.size());
        g_audit_probe_filtered_rows.store(visible.size());
        g_audit_probe_native_rows.store(ListView_GetItemCount(m_list));
        g_audit_probe_filter.store(filter);
        g_audit_probe_list_visible.store(IsWindowVisible(m_list) != FALSE);
#endif
    }

    void update_sort_indicator() {
        const HWND header = ListView_GetHeader(m_list);
        if (header == nullptr) return;
        const int sort_column = std::clamp<int>(g_cfg_audit_sort_col, 0, column_count - 1);
        const bool ascending = g_cfg_audit_sort_dir >= 0;
        for (int index = 0; index < column_count; ++index) {
            HDITEMW item = {};
            item.mask = HDI_FORMAT;
            if (!Header_GetItem(header, index, &item)) continue;
            item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
            if (index == sort_column) item.fmt |= ascending ? HDF_SORTUP : HDF_SORTDOWN;
            Header_SetItem(header, index, &item);
        }
    }

    void layout() {
        if (m_window == nullptr) return;
        RECT client = {};
        GetClientRect(m_window, &client);
        const int margin = ui::scale_for_window_dpi(m_window, 8);
        const int gap = ui::scale_for_window_dpi(m_window, 6);
        const int top_height = ui::scale_for_window_dpi(m_window, 24);
        const int progress_height = ui::scale_for_window_dpi(m_window, 8);
        const int progress_gap = ui::scale_for_window_dpi(m_window, 3);
        const int filter_label_width = ui::scale_for_window_dpi(m_window, 36);
        const int filter_width = ui::scale_for_window_dpi(m_window, 160);
        const int bottom_height = ui::scale_for_window_dpi(m_window, 30);
        const int button_height = ui::scale_for_window_dpi(m_window, 26);

        const int right = client.right - margin;
        const int filter_left = right - filter_width;
        const int label_left = filter_left - gap - filter_label_width;
        MoveWindow(
            GetDlgItem(m_window, IDC_AUDIT_STATUS),
            margin, margin,
            std::max<int>(20, label_left - margin - gap), top_height,
            TRUE);
        MoveWindow(
            GetDlgItem(m_window, IDC_AUDIT_FILTER_LABEL),
            label_left, margin + ui::scale_for_window_dpi(m_window, 3),
            filter_label_width, top_height, TRUE);
        MoveWindow(
            GetDlgItem(m_window, IDC_AUDIT_FILTER),
            filter_left, margin,
            filter_width, ui::scale_for_window_dpi(m_window, 180), TRUE);

        const int progress_top = margin + top_height + progress_gap;
        MoveWindow(
            m_progress,
            margin, progress_top,
            std::max<int>(40, client.right - 2 * margin),
            progress_height,
            TRUE);

        const int list_top = progress_top + progress_height + gap;
        const int list_bottom = client.bottom - margin - bottom_height - gap;
        MoveWindow(
            m_list,
            margin, list_top,
            std::max<int>(40, client.right - 2 * margin),
            std::max<int>(40, list_bottom - list_top),
            TRUE);

        int x = margin;
        struct button_layout { int id; int width; };
        const button_layout buttons[] = {
            {IDC_AUDIT_RESCAN, 72},
            {IDC_AUDIT_CANCEL, 88},
            {IDC_AUDIT_EDIT_PHYSICAL, 166},
            {IDC_AUDIT_RESET_COLUMNS, 104},
        };
        const int y = client.bottom - margin - button_height;
        for (const auto& button : buttons) {
            const int width = ui::scale_for_window_dpi(m_window, button.width);
            MoveWindow(GetDlgItem(m_window, button.id), x, y, width, button_height, TRUE);
            x += width + gap;
        }
        const int close_width = ui::scale_for_window_dpi(m_window, 72);
        MoveWindow(
            GetDlgItem(m_window, IDC_AUDIT_CLOSE),
            client.right - margin - close_width, y,
            close_width, button_height, TRUE);
    }

    void restore_geometry() {
        const int saved_x = g_cfg_audit_win_x;
        const int saved_y = g_cfg_audit_win_y;
        if (saved_x < 0 || saved_y < 0) return;
        int width = MulDiv(std::max<int>(g_cfg_audit_win_w, 760), static_cast<int>(m_dpi), 96);
        int height = MulDiv(std::max<int>(g_cfg_audit_win_h, 440), static_cast<int>(m_dpi), 96);
        RECT desired = {saved_x, saved_y, saved_x + width, saved_y + height};
        HMONITOR monitor = MonitorFromRect(&desired, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info = {sizeof(info)};
        if (!GetMonitorInfoW(monitor, &info)) return;
        const RECT work = info.rcWork;
        width = std::min<int>(width, work.right - work.left);
        height = std::min<int>(height, work.bottom - work.top);
        int x = std::clamp<int>(saved_x, work.left, std::max<int>(work.left, work.right - width));
        int y = std::clamp<int>(saved_y, work.top, std::max<int>(work.top, work.bottom - height));
        SetWindowPos(
            m_window, nullptr, x, y, width, height,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void save_geometry() {
        WINDOWPLACEMENT placement = {sizeof(placement)};
        if (!GetWindowPlacement(m_window, &placement)) return;
        const RECT normal = placement.rcNormalPosition;
        g_cfg_audit_win_x = normal.left;
        g_cfg_audit_win_y = normal.top;
        const UINT dpi = m_dpi == 0 ? 96 : m_dpi;
        g_cfg_audit_win_w = MulDiv(normal.right - normal.left, 96, static_cast<int>(dpi));
        g_cfg_audit_win_h = MulDiv(normal.bottom - normal.top, 96, static_cast<int>(dpi));
        g_cfg_audit_win_max = placement.showCmd == SW_SHOWMAXIMIZED;
    }

    void save_state() {
        save_geometry();
        save_columns();
    }

    void on_dpi_changed(UINT new_dpi, const RECT* suggested) {
        if (new_dpi == 0) new_dpi = 96;
        const UINT old_dpi = m_dpi == 0 ? 96 : m_dpi;
        for (int index = 0; index < column_count; ++index) {
            if (m_column_visible[index]) {
                const int width = ListView_GetColumnWidth(m_list, index);
                if (width > 0) {
                    ListView_SetColumnWidth(
                        m_list, index,
                        ui::scale_between_dpi(width, old_dpi, new_dpi));
                }
            }
        }
        m_dpi = new_dpi;
        m_tooltips.update_dpi(m_window);
        if (suggested != nullptr) {
            SetWindowPos(
                m_window, nullptr,
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        layout();
    }

    LRESULT handle_notify(NMHDR* header) {
        if (header == nullptr) return 0;
        if (header->hwndFrom == m_list) {
            switch (header->code) {
            case LVN_COLUMNCLICK: {
                const auto* info = reinterpret_cast<NMLISTVIEW*>(header);
                on_column_click(info->iSubItem);
                return 0;
            }
            case NM_DBLCLK:
                edit_selected_physical();
                return 0;
            case LVN_ITEMCHANGED:
                if (!m_refreshing_list) update_edit_enabled();
                return 0;
            case LVN_GETINFOTIPW: {
                auto* tip = reinterpret_cast<NMLVGETINFOTIPW*>(header);
                if (tip->iItem < 0 || tip->pszText == nullptr || tip->cchTextMax <= 0) return 0;
                LVITEMW entry = {};
                entry.mask = LVIF_PARAM;
                entry.iItem = tip->iItem;
                if (!ListView_GetItem(m_list, &entry)) return 0;
                const size_t row_index = static_cast<size_t>(entry.lParam);
                if (row_index >= m_rows.size()) return 0;
                const std::string utf8 = row_tooltip_text(m_rows[row_index]);
                pfc::stringcvt::string_wide_from_utf8 wide(utf8.c_str());
                wcsncpy_s(
                    tip->pszText,
                    static_cast<size_t>(tip->cchTextMax),
                    static_cast<const wchar_t*>(wide),
                    _TRUNCATE);
                return 0;
            }
            case LVN_KEYDOWN: {
                const auto* key = reinterpret_cast<NMLVKEYDOWN*>(header);
                if (key->wVKey == VK_RETURN) {
                    edit_selected_physical();
                } else if (key->wVKey == 'A' && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                    select_all();
                } else if (key->wVKey == 'C' && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                    copy_selected();
                }
                return 0;
            }
            }
        }
        if (header->hwndFrom == ListView_GetHeader(m_list)) {
            if (header->code == NM_RCLICK) {
                POINT point = {};
                GetCursorPos(&point);
                show_column_menu(point);
                return 0;
            }
            if (header->code == HDN_ENDTRACKW || header->code == HDN_ENDTRACKA ||
                header->code == HDN_ENDDRAG) {
                save_columns();
            }
        }
        return 0;
    }

    static INT_PTR CALLBACK dialog_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
        audit_window* self = reinterpret_cast<audit_window*>(
            GetWindowLongPtrW(window, DWLP_USER));
        if (message == WM_INITDIALOG) {
            self = reinterpret_cast<audit_window*>(l_param);
            SetWindowLongPtrW(window, DWLP_USER, reinterpret_cast<LONG_PTR>(self));
            g_audit_window = window;
            return self->on_init(window);
        }
        if (self == nullptr) return FALSE;

        switch (message) {
        case WM_COMMAND:
            switch (LOWORD(w_param)) {
            case IDC_AUDIT_FILTER:
                if (HIWORD(w_param) == CBN_SELCHANGE) self->on_filter_changed();
                return TRUE;
            case IDC_AUDIT_RESCAN:
                self->on_start_scan();
                return TRUE;
            case IDC_AUDIT_CANCEL:
                self->cancel_scan();
                return TRUE;
            case IDC_AUDIT_EDIT_PHYSICAL:
                self->edit_selected_physical();
                return TRUE;
            case IDC_AUDIT_RESET_COLUMNS:
                self->reset_columns();
                return TRUE;
            case IDC_AUDIT_CLOSE:
            case IDCANCEL:
                self->close();
                return TRUE;
            }
            break;
        case WM_NOTIFY:
            self->handle_notify(reinterpret_cast<NMHDR*>(l_param));
            return TRUE;
        case WM_SIZE:
            self->layout();
            return TRUE;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(l_param);
            info->ptMinTrackSize.x = ui::scale_for_window_dpi(window, 760);
            info->ptMinTrackSize.y = ui::scale_for_window_dpi(window, 440);
            return TRUE;
        }
        case WM_DPICHANGED:
            self->on_dpi_changed(
                HIWORD(w_param),
                reinterpret_cast<const RECT*>(l_param));
            return TRUE;
        case WM_CLOSE:
            self->close();
            return TRUE;
        case WM_AUDIT_START:
            self->on_start_scan();
            return TRUE;
        case WM_AUDIT_PROGRESS:
            self->on_progress(
                static_cast<size_t>(w_param),
                static_cast<size_t>(l_param));
            return TRUE;
        case WM_AUDIT_DONE:
            self->on_scan_done(w_param != 0);
            return TRUE;
        case WM_DESTROY:
            if (self->m_modeless_registered) {
                modeless_dialog_manager::g_remove(window);
                self->m_modeless_registered = false;
            }
            self->save_state();
            return TRUE;
        case WM_NCDESTROY:
            g_audit_window = nullptr;
            SetWindowLongPtrW(window, DWLP_USER, 0);
            self->m_window = nullptr;
            delete self;
            return TRUE;
        }
        return FALSE;
    }

    static void shutdown_active() {
        if (g_audit_window == nullptr || !IsWindow(g_audit_window)) return;
        auto* existing = reinterpret_cast<audit_window*>(
            GetWindowLongPtrW(g_audit_window, DWLP_USER));
        if (existing == nullptr) {
            DestroyWindow(g_audit_window);
            return;
        }
        existing->shutdown_now();
    }

    static void show(audit_scope scope, metadb_handle_list_cref seed) {
        if (g_audit_window != nullptr && IsWindow(g_audit_window)) {
            auto* existing = reinterpret_cast<audit_window*>(
                GetWindowLongPtrW(g_audit_window, DWLP_USER));
            if (existing != nullptr) existing->request_scope(scope, seed);
            if (IsIconic(g_audit_window)) ShowWindow(g_audit_window, SW_RESTORE);
            else ShowWindow(g_audit_window, SW_SHOW);
            SetForegroundWindow(g_audit_window);
            return;
        }

        auto* instance = new audit_window(scope, seed);
        HWND window = CreateDialogParamW(
            core_api::get_my_instance(),
            MAKEINTRESOURCEW(IDD_PHYSICAL_METADATA_AUDIT),
            core_api::get_main_window(),
            dialog_proc,
            reinterpret_cast<LPARAM>(instance));
        if (window == nullptr) {
            delete instance;
            popup_message::g_show(
                "Windows could not create the Physical Metadata Audit window.",
                "Metadata Isolation");
            return;
        }
        ShowWindow(window, instance->show_command());
        UpdateWindow(window);
    }

private:
    void shutdown_now() {
        if (m_scan) {
            m_scan->cancel_requested.store(true);
            m_scan->logger.finish(
                "CANCELLED",
                "FOOBAR_SHUTDOWN",
                {log_flag("read_only", true)});
            m_scan.reset();
        }
        m_pending_scan = false;
        m_pending_seed.remove_all();
        save_state();
        DestroyWindow(m_window);
    }

    audit_scope m_scope;
    metadb_handle_list m_seed;
    HWND m_window = nullptr;
    HWND m_list = nullptr;
    HWND m_progress = nullptr;
    UINT m_dpi = 96;
    fb2k::CCoreDarkModeHooks m_dark;
    dialog_tooltips m_tooltips;
    std::shared_ptr<scan_state> m_scan;
    std::vector<physical_audit_row> m_rows;
    int m_column_widths[column_count] = {};
    bool m_column_visible[column_count] = {};
    bool m_close_when_done = false;
    bool m_pending_scan = false;
    bool m_modeless_registered = false;
    bool m_refreshing_list = false;
    audit_scope m_pending_scope = audit_scope::library;
    metadb_handle_list m_pending_seed;
};

class audit_mainmenu_commands : public mainmenu_commands {
public:
    t_uint32 get_command_count() override { return 1; }
    GUID get_command(t_uint32 index) override {
        if (index != 0) uBugCheck();
        return guids::audit_mainmenu_command;
    }
    void get_name(t_uint32 index, pfc::string_base& output) override {
        if (index != 0) uBugCheck();
        output = "Audit physical metadata...";
    }
    bool get_description(t_uint32 index, pfc::string_base& output) override {
        if (index != 0) return false;
        output =
            "Scans the Media Library read-only for embedded-Cue files whose physical core tags "
            "are empty, incomplete, different from unique global Cue values, or require review.";
        return true;
    }
    GUID get_parent() override { return guids::audit_mainmenu_group; }
    void execute(t_uint32 index, service_ptr_t<service_base>) override {
        if (index != 0) uBugCheck();
        show_physical_metadata_audit_library();
    }
};

mainmenu_group_popup_factory g_audit_mainmenu_group(
    guids::audit_mainmenu_group,
    mainmenu_groups::library,
    mainmenu_commands::sort_priority_dontcare,
    "Metadata Isolation");

mainmenu_commands_factory_t<audit_mainmenu_commands> g_audit_mainmenu_commands;

} // namespace

#if defined(FB2K_ENABLE_TEST_PROBES)
void read_audit_internal_state_for_test(pfc::string_base& output) {
    output << "audit_model_rows=" << g_audit_probe_model_rows.load() << "\n"
           << "audit_filtered_rows=" << g_audit_probe_filtered_rows.load() << "\n"
           << "audit_native_rows=" << g_audit_probe_native_rows.load() << "\n"
           << "audit_filter_index=" << g_audit_probe_filter.load() << "\n"
           << "audit_list_visible="
           << (g_audit_probe_list_visible.load() ? "true" : "false") << "\n";
}
#endif

void show_physical_metadata_audit_library() {
    metadb_handle_list empty;
    audit_window::show(audit_scope::library, empty);
}

void show_physical_metadata_audit_selection(metadb_handle_list_cref selection) {
    audit_window::show(audit_scope::selection, selection);
}

void shutdown_physical_metadata_audit() {
    audit_window::shutdown_active();
}

} // namespace metadata_isolation
