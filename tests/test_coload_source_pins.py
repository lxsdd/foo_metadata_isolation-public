#!/usr/bin/env python3
"""Immutable current-product 3-DLL original foobar host source/CUE contract."""
from __future__ import annotations
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
FLOW = ROOT / ".github/workflows/modified-discogger-coload.yml"
SDK_PIN = ROOT / "sdk/SDK-PIN.json"
DISCOGGER = "cf2ce792ca53379436a729e0e7e1ac810fcaed38"
STUDIO = "3b8cb71965971625842ca0290156149408b971eb"
SDK = "31a35947a7527826c2199491692802e3ce3d8bd7"
INSTALLER = "35369054c56f4c6ccb2f802944e9675852fd35ec"
ACCESSIBILITY = "3482a2beb4d91c9875bc28b91bd43287bab47bc6"

# Fail-closed original-host no-write evidence: a source-pin update must never
# silently disable the real Discogger callback, MP3/mtime no-op oracle,
# virtual write guard, or third-component load requirement.
HOST_SCRIPT = ROOT / "tests/run_isolated_virtual_write.ps1"
HOST_GATES = (
    "function Assert-EmbeddedCueNoWrite([string]$phase) {",
    "if (-not $studioCoLoad) { return }",
    "$embeddedCueBaselineHash = (Get-FileHash -LiteralPath $embeddedCueSource -Algorithm SHA256).Hash",
    "$embeddedCueBaselineTicks = (Get-Item -LiteralPath $embeddedCueSource).LastWriteTimeUtc.Ticks",
    "-cne $embeddedCueBaselineHash -or",
    "-ne $embeddedCueBaselineTicks) {",
    "throw \"Embedded-CUE source changed during three-component co-load: $phase\"",
    "'.ci_shared/scripts/windows-ui-accessibility.ps1'",
    "& ./tests/assert_real_audit_uia.ps1 -FoobarPid $process.Id",
    "Assert-EmbeddedCueNoWrite 'after-discogger-physical-write'",
    "Assert-EmbeddedCueNoWrite 'after-discogger-identical-noop'",
    "Assert-EmbeddedCueNoWrite 'after-physical-audit-classifier'",
    "Assert-EmbeddedCueNoWrite 'before-physical-write-positive-control'",
    "Assert-EmbeddedCueNoWrite 'after-direct-virtual-write-guard'",
    "throw 'Three-component host test requires modified Discogger too'",
    "throw 'Genuine foobar host did not load pinned Studio component'",
    "if (-not $qaNoOpConfirmed)",
    "(Get-FileHash -LiteralPath $discoggerQaMp3 -Algorithm SHA256).Hash -ne $qaNoOpHash",
    "(Get-Item -LiteralPath $discoggerQaMp3).LastWriteTimeUtc.Ticks -ne $qaNoOpTicks",
    "throw 'Identical Discogger SDK operation rewrote physical MP3 during co-load'",
    "MODIFIED_DISCOGGER_NATIVE_SDK_PHYSICAL_MP3_NOOP_COLOAD_",
    "REAL_HOST_DIRECT_VIRTUAL_WRITE_GUARD_",
    "throw 'Discogger-owned test write unexpectedly changed the separate embedded-Cue source'",
    "if ($qaCount -ceq '2') {",
    "throw 'No genuine Discogger SDK identical no-op completion callback'",
)

# The first original-host SDK command must write, and the second must invoke
# the identical command. Comment-only markers are not executable evidence.
HOST_QA_DISPATCH = "Start-Process -FilePath $exe -WorkingDirectory $portable -ArgumentList '/command:\"Discogger QA MP3 write\"'"

def host_issues(source: str) -> list[str]:
    # PowerShell comment-only "guards" must not qualify a host run.
    executable = re.sub(r"(?s)<#.*?#>", "", source)
    executable = "\n".join(line for line in executable.splitlines()
                           if not line.lstrip().startswith("#"))
    missing = [gate for gate in HOST_GATES if gate not in executable]
    if executable.count(HOST_QA_DISPATCH) != 2:
        missing.append("Discogger physical write and identical no-op dispatch")
    for component, pin in (("Discogger", DISCOGGER), ("Studio", STUDIO)):
        if executable.count(pin) != 1:
            missing.append(component + " real-host pin drift")
    return missing

def issues(source: str, sdk_pin: dict) -> list[str]:
    problems = []
    def refs(repo: str, expected: list[str]):
        found = re.findall(r"(?m)^\s+repository:\s*" + re.escape(repo)
                           + r"\s*\n\s+ref:\s*([0-9a-f]{40})\s*$", source)
        if found != expected:
            problems.append("wrong exact checkout refs: " + repo)
    refs("lxsdd/foo_discogger", [DISCOGGER])
    refs("lxsdd/music-metadata-studio", [STUDIO])
    refs("lxsdd/foobar2000_component_template", [SDK] * 3)
    refs("lxsdd/dev-infrastructure-public", [INSTALLER, ACCESSIBILITY])
    for label, exact in (
        ("discogger PowerShell assertion", "$expected='" + DISCOGGER + "'"),
        ("studio PowerShell assertion", "$studioExpected='" + STUDIO + "'"),
        ("observer PowerShell assertion", "$observerExpected='" + ACCESSIBILITY + "'"),
        ("discogger runtime env", "DISCOGGER_MODIFIED_SOURCE_SHA: " + DISCOGGER),
        ("studio runtime env", "MMS_PINNED_SOURCE_SHA: " + STUDIO),
    ):
        if source.count(exact) != 1:
            problems.append("wrong source identity: " + label)
    if sdk_pin.get("source_commit") != SDK or sdk_pin.get("sdk_version") != "2026-10-01":
        problems.append("wrong original SDK 2026-10-01 pin")
    for marker in (
        "./scripts/build-component-dll.ps1 -Platforms",
        "./scripts/build-foobar.ps1 -Platform",
        "./tests/run_isolated_virtual_write.ps1 -Architecture",
        "-RunPhysicalMetadata -RunDirectGuard -RunUiContracts -RunRealAuditUia",
        "MMS_ACTUAL_DEFAULT_READONLY_DLL_",
        "PINNED_STUDIO_DISCOGGER_ISOLATION_PHYSICAL_GUARDS_",
        "THREE_COMPONENT_NATIVE_PHYSICAL_AUDIT_UIA_",
        "path: .ci_shared",
        "[System.Management.Automation.Language.Parser]::ParseFile(",
    ):
        if marker not in source:
            problems.append("missing original native host/CUE safety step: " + marker)
    for marker in ("-EnablePhysicalWrites", "actions/upload-artifact@", "gh release create", "gh release upload"):
        if marker in source:
            problems.append("unsafe source-only workflow operation: " + marker)
    return problems

class ThreeComponentCurrentSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = FLOW.read_text(encoding="utf-8")
        cls.pin = json.loads(SDK_PIN.read_text(encoding="utf-8"))

    def test_exact_current_source_and_original_host_contract(self):
        self.assertEqual(issues(self.source, self.pin), [])

    def mutated(self, old: str, new: str, label: str):
        self.assertIn(old, self.source)
        self.assertTrue(any(label in x for x in issues(self.source.replace(old, new, 1), self.pin)),
                        (old, label))

    def test_stale_discogger_checkout_rejected(self):
        self.mutated("ref: " + DISCOGGER, "ref: " + "0"*40, "checkout")

    def test_stale_studio_checkout_rejected(self):
        self.mutated("ref: " + STUDIO, "ref: " + "0"*40, "checkout")

    def test_stale_discogger_assertion_rejected(self):
        self.mutated("$expected='" + DISCOGGER + "'", "$expected='" + "0"*40 + "'", "identity")

    def test_stale_studio_assertion_rejected(self):
        self.mutated("$studioExpected='" + STUDIO + "'", "$studioExpected='" + "0"*40 + "'", "identity")

    def test_stale_discogger_runtime_env_rejected(self):
        self.mutated("DISCOGGER_MODIFIED_SOURCE_SHA: " + DISCOGGER,
                     "DISCOGGER_MODIFIED_SOURCE_SHA: " + "0"*40, "identity")

    def test_stale_studio_runtime_env_rejected(self):
        self.mutated("MMS_PINNED_SOURCE_SHA: " + STUDIO,
                     "MMS_PINNED_SOURCE_SHA: " + "0"*40, "identity")

    def test_stale_sdk_checkout_rejected(self):
        self.mutated("ref: " + SDK, "ref: " + "0"*40, "checkout")

    def test_stale_installer_transport_rejected(self):
        self.mutated("ref: " + INSTALLER, "ref: " + "0"*40, "checkout")

    def test_missing_direct_guard_rejected(self):
        self.mutated("-RunPhysicalMetadata -RunDirectGuard", "-RunPhysicalMetadata", "host/CUE")

    def test_missing_physical_scope_rejected(self):
        self.mutated("-RunPhysicalMetadata -RunDirectGuard", "-RunDirectGuard", "host/CUE")

    def test_missing_original_component_build_rejected(self):
        self.mutated("./scripts/build-component-dll.ps1 -Platforms", "# skipped build", "host/CUE")

    def test_missing_readonly_studio_marker_rejected(self):
        self.mutated("MMS_ACTUAL_DEFAULT_READONLY_DLL_", "MMS_UNVERIFIED_DLL_", "host/CUE")

    def test_enable_studio_physical_writes_rejected(self):
        self.assertTrue(any("unsafe" in x for x in issues(self.source + "\n-EnablePhysicalWrites", self.pin)))

    def test_binary_artifact_upload_rejected(self):
        self.assertTrue(any("unsafe" in x for x in issues(self.source + "\nactions/upload-artifact@v7", self.pin)))

    def test_sdk_pin_drift_rejected(self):
        self.assertTrue(any("SDK" in x for x in issues(self.source, {**self.pin, "source_commit": "0"*40})))

    def test_native_three_component_no_write_evidence_required(self):
        self.assertEqual(host_issues(HOST_SCRIPT.read_text(encoding="utf-8")), [])

    def test_missing_real_host_guard_or_noop_evidence_rejected(self):
        host = HOST_SCRIPT.read_text(encoding="utf-8")
        for marker in HOST_GATES:
            self.assertIn(marker, host)
            self.assertIn(marker, host_issues(host.replace(marker, "# removed gate", 1)))

    def test_commented_original_host_guards_rejected(self):
        host = HOST_SCRIPT.read_text(encoding="utf-8")
        for marker in HOST_GATES:
            active = next(line for line in host.splitlines()
                          if marker in line and not line.lstrip().startswith("#"))
            self.assertIn(marker, host_issues(host.replace(active, "# " + active, 1)))
            self.assertIn(marker, host_issues(host.replace(active, "<# " + active + " #>", 1)))

    def test_discogger_real_write_and_identical_noop_both_required(self):
        host = HOST_SCRIPT.read_text(encoding="utf-8")
        self.assertEqual(host.count(HOST_QA_DISPATCH), 2)
        active = next(line for line in host.splitlines() if HOST_QA_DISPATCH in line)
        self.assertTrue(any("dispatch" in x for x in
                            host_issues(host.replace(active, "# " + active, 1))))
        self.assertTrue(any("dispatch" in x for x in
                            host_issues(host.replace(HOST_QA_DISPATCH,
                                                     HOST_QA_DISPATCH.replace("MP3 write", "MP3 read"), 1))))

    def test_stale_accessibility_observer_rejected(self):
        self.mutated("ref: " + ACCESSIBILITY, "ref: " + "0"*40, "checkout")

    def test_missing_original_three_component_audit_ui_rejected(self):
        self.mutated("-RunRealAuditUia", "-NoAuditUia", "host/CUE")
        self.mutated("path: .ci_shared", "path: .ci_untrusted", "host/CUE")

    def test_embedded_cue_original_carrier_rewrite_detection(self):
        host = HOST_SCRIPT.read_text(encoding="utf-8")
        self.assertEqual(host_issues(host), [])
        for marker in (
            "if (-not $studioCoLoad) { return }",
            "-cne $embeddedCueBaselineHash -or",
            "-ne $embeddedCueBaselineTicks) {",
            "Assert-EmbeddedCueNoWrite 'after-discogger-physical-write'",
            "Assert-EmbeddedCueNoWrite 'after-discogger-identical-noop'",
            "Assert-EmbeddedCueNoWrite 'after-physical-audit-classifier'",
            "Assert-EmbeddedCueNoWrite 'before-physical-write-positive-control'",
            "Assert-EmbeddedCueNoWrite 'after-direct-virtual-write-guard'",
        ):
            self.assertIn(marker, host)
            self.assertIn(marker, host_issues(host.replace(marker, "# disabled guard", 1)))

    def test_host_runtime_pin_drift_rejected(self):
        host = HOST_SCRIPT.read_text(encoding="utf-8")
        for pin in (DISCOGGER, STUDIO):
            self.assertIn(pin, host)
            self.assertTrue(host_issues(host.replace(pin, "0"*40, 1)))

if __name__ == "__main__":
    unittest.main(verbosity=2)
