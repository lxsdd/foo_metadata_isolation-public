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
DISCOGGER = "f3ac723aee765a335b0d7c811ff99855702b367f"
STUDIO = "fef08a4d730c0093e9cca75cc95e205218bc6b45"
SDK = "31a35947a7527826c2199491692802e3ce3d8bd7"
INSTALLER = "35369054c56f4c6ccb2f802944e9675852fd35ec"

def issues(source: str, sdk_pin: dict) -> list[str]:
    problems = []
    def refs(repo: str, expected: str, count: int):
        found = re.findall(r"(?m)^\s+repository:\s*" + re.escape(repo)
                           + r"\s*\n\s+ref:\s*([0-9a-f]{40})\s*$", source)
        if found != [expected] * count:
            problems.append("wrong exact checkout refs: " + repo)
    refs("lxsdd/foo_discogger", DISCOGGER, 1)
    refs("lxsdd/music-metadata-studio", STUDIO, 1)
    refs("lxsdd/foobar2000_component_template", SDK, 3)
    refs("lxsdd/dev-infrastructure-public", INSTALLER, 1)
    for label, exact in (
        ("discogger PowerShell assertion", "$expected='" + DISCOGGER + "'"),
        ("studio PowerShell assertion", "$studioExpected='" + STUDIO + "'"),
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
        "-RunPhysicalMetadata -RunDirectGuard",
        "MMS_ACTUAL_DEFAULT_READONLY_DLL_",
        "PINNED_STUDIO_DISCOGGER_ISOLATION_PHYSICAL_GUARDS_",
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

if __name__ == "__main__":
    unittest.main(verbosity=2)
