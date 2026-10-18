"""Static release gates for the non-technical virtual-edit completion UX."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONTEXT = (ROOT / "src/foo_metadata_isolation/context_menu.cpp").read_text(encoding="utf-8")
SESSION = (ROOT / "src/foo_metadata_isolation/virtual_edit_session.cpp").read_text(
    encoding="utf-8"
)
GUARD = (ROOT / "src/foo_metadata_isolation/write_guard.cpp").read_text(encoding="utf-8")
SDK_MANAGER = (ROOT / "sdk/foobar2000/SDK/contextmenu_manager.h").read_text(
    encoding="utf-8"
)


assert "virtual_edit_session_pending()) return false" not in CONTEXT
assert (
    "if (index == command_virtual_review || index == command_virtual_discard) return false;"
    in CONTEXT
)
assert 'output = "Virtual metadata: Run context command safely...";' in CONTEXT

assert "popup_message_v3::query_t query;" in SESSION
assert "query.reply = fb2k::makeCompletionNotify" in SESSION
assert "query.show();" in SESSION
assert "show_finish_virtual_edit_prompt();" in SESSION
assert "show_commit_dialog(session->summary())" not in SESSION
assert "file_lock_manager::get()->acquire_write(\n                    current_path" in SESSION
assert "same_path(previous_path, current_path)" in SESSION
assert "No unsafe or unsupported changes were written." in SESSION
assert "The operation failed and the original files were restored." in SESSION
assert "CRITICAL: Metadata Isolation could not confirm complete recovery." in SESSION
assert (
    'show_transaction_failure(session, "The virtual changes could not be validated safely.");'
    in SESSION
)
assert "popup_message::g_showToast(success);" in SESSION

assert "Direct virtual metadata writing was blocked" in GUARD
assert "Virtual metadata: Run context command safely..." in GUARD
assert "No change was written." in GUARD

assert "virtual void execute()=0" in SDK_MANAGER

print("Virtual-edit UX contract assertions passed.")
