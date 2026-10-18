"""Static contract for the Phase-7 structured Standard/Verbose logging."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src/foo_metadata_isolation"
LOGGER_H = (SRC / "operation_log.h").read_text(encoding="utf-8")
LOGGER = (SRC / "operation_log.cpp").read_text(encoding="utf-8")
SESSION = (SRC / "virtual_edit_session.cpp").read_text(encoding="utf-8")
CONTEXT = (SRC / "context_menu.cpp").read_text(encoding="utf-8")
AUDIT_UI = (SRC / "physical_metadata_audit_ui.cpp").read_text(encoding="utf-8")
DIAGNOSTICS = (SRC / "diagnostics.cpp").read_text(encoding="utf-8")
PROJECT = (SRC / "foo_metadata_isolation.vcxproj").read_text(encoding="utf-8")
PREFERENCES = (SRC / "preferences.cpp").read_text(encoding="utf-8")


# One central, transaction-safe logger owns formatting, correlation and level
# selection. Logging must never be able to alter transaction control flow.
assert '<ClCompile Include="operation_log.cpp">' in PROJECT
assert '<ClInclude Include="operation_log.h" />' in PROJECT
assert "std::atomic<uint64_t> g_session_sequence" in LOGGER
assert "std::mutex g_console_mutex" in LOGGER
assert "std::chrono::steady_clock" in LOGGER
assert "[METAISO S" in LOGGER
assert "level=STANDARD event=session.complete" in LOGGER
assert "operation_logger(const char* operation, const char* layer) noexcept" in LOGGER_H
for method in ("standard", "verbose", "critical", "finish"):
    assert f"void operation_logger::{method}(" in LOGGER
assert LOGGER.count("catch (...)") >= 5
assert "if (m_state == nullptr) return" in LOGGER
for helper in ("log_count", "log_number", "log_duration", "log_flag", "log_token"):
    assert f"operation_log_field {helper}(" in LOGGER_H
    assert "noexcept;" in LOGGER_H.split(f"operation_log_field {helper}(", 1)[1].splitlines()[0]
    assert "noexcept {" in LOGGER.split(f"operation_log_field {helper}(", 1)[1].splitlines()[0]

# The existing persisted preference is the only production level switch.
assert "cfg_verbose_logging(guids::cfg_verbose_logging, false)" in PREFERENCES
assert "bool verbose = cfg_verbose_logging" in LOGGER
assert "cfg_verbose_logging" not in SESSION
assert "cfg_verbose_logging" not in CONTEXT
assert "cfg_verbose_logging" not in AUDIT_UI
assert "if (m_state == nullptr || !m_state->verbose) return" in LOGGER

# Lines use structured fields and privacy-safe escaping/redaction. No product
# call site is permitted to log raw file paths or before/after metadata values.
for marker in (
    "access_token", "refresh_token", "authorization", "password", "passwd",
    "secret", "cookie", "credential", "oauth", "auth_token", "token", "auth",
):
    assert f'"{marker}"' in LOGGER
assert "<redacted>" in LOGGER
assert "safe_identifier" in LOGGER and "safe_value" in LOGGER
assert 'log_token("path"' not in SESSION + CONTEXT + AUDIT_UI
assert 'log_token("before"' not in SESSION
assert 'log_token("after"' not in SESSION

# Standard mode covers lifecycle, aggregate counts and truthful terminal
# outcomes. Verbose mode adds timings, classification, scope and planning.
for event in (
    "session.start", "projection.complete", "mapping.complete", "command.begin",
    "diff.classified", "commit.begin", "commit.complete",
    "postvalidation.complete", "rollback.complete", "cleanup.complete",
):
    assert f'"{event}"' in SESSION + CONTEXT
for event in (
    "timing.summary", "source.document", "scope.resolved", "plan.complete",
    "accepted_postimage.built", "postimage.precommit_validation",
    "stale.check", "commit.backup", "commit.stage", "commit.install",
):
    assert f'"{event}"' in SESSION
for outcome in (
    "SUCCESS", "SUCCESS_WITH_SKIPPED_CHANGES", "NO_CHANGE", "CANCELLED",
    "REJECTED", "FAILED_AND_ROLLED_BACK",
):
    assert f'"{outcome}"' in SESSION
for reason in (
    "COMMAND_CANCELLED", "COMMAND_NO_CHANGE", "COMMAND_INVOCATION_FAILED",
    "STALE_SOURCE", "POSTVALIDATION_FAILED", "ROLLBACK_FAILED", "COMMIT_FAILED",
):
    assert f'"{reason}"' in SESSION

# The performance contract includes the expensive projected-shadow and commit
# stages without adding a per-track source-analysis log storm.
for metric in (
    "source_analysis_us", "source_hash_us", "virtual_inventory_us",
    "storage_inspection_us", "projection_copy_us", "projection_write_us",
    "projection_read_us", "projection_total_us", "command_us",
    "diff_classification_us", "planning_us", "accepted_postimage_us",
    "stale_check_us", "backup_us", "staging_us", "install_us",
    "postvalidation_us", "rollback_us", "commit_total_us", "cleanup_us",
):
    assert f'"{metric}"' in SESSION
assert SESSION.count('"source.document"') == 1

# Relevant Physical and Dedicated-Cue routing is correlated as well.
for operation in (
    "physical_properties", "physical_context", "embedded_cuesheet_editor",
):
    assert f'"{operation}"' in CONTEXT

# The read-only Physical Metadata Audit is correlated by counts/status only.
assert '"physical_metadata_audit"' in AUDIT_UI
assert '"audit.begin"' in AUDIT_UI
assert 'log_flag("read_only", true)' in AUDIT_UI
for sensitive in ('log_token("artist"', 'log_token("album"', 'log_token("genre"', 'log_token("file"'):
    assert sensitive not in AUDIT_UI

# Credential/Unicode runtime qualification is diagnostic-only and therefore
# absent from probe-free release compilation.
assert 'diagnostics.cpp" Condition="\'$(EnableTestProbes)\'==\'true\'"' in PROJECT
assert "/metadata-isolation-probe-logging-contract" in DIAGNOSTICS
assert "/metadata-isolation-probe-logging-outcomes" in DIAGNOSTICS
for sentinel in (
    "ACCESS_TOKEN_SENTINEL_DO_NOT_LOG", "REFRESH_TOKEN_SENTINEL_DO_NOT_LOG",
    "PASSWORD_SENTINEL_DO_NOT_LOG", "COOKIE_SENTINEL_DO_NOT_LOG",
):
    assert sentinel in DIAGNOSTICS
assert "credential_sentinels_absent" in DIAGNOSTICS
assert "unicode_preserved" in DIAGNOSTICS
assert "session_prefix_present" in DIAGNOSTICS
capture_guard = LOGGER.index(
    "#if defined(FB2K_ENABLE_TEST_PROBES)\nstd::string format_operation_log_line_for_test")
capture_end = LOGGER.index("#endif", capture_guard)
assert capture_guard < LOGGER.index("void clear_operation_log_capture_for_test", capture_guard) < capture_end
assert capture_guard < LOGGER.index("snapshot_operation_log_capture_for_test", capture_guard) < capture_end

print("Phase-7 structured logging static contract assertions passed.")
