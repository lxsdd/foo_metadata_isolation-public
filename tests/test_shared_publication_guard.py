#!/usr/bin/env python3
"""Synthetic positive/negative regressions for pinned shared publication guard."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from _shared_publication_guard import scan_workflows
from importlib.util import spec_from_file_location, module_from_spec

spec = spec_from_file_location("isolation_publication_wrapper", ROOT / "scripts" / "verify-public-artifact-hold.py")
wrapper = module_from_spec(spec)
spec.loader.exec_module(wrapper)

class SharedPublicationGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        folder = ROOT / ".github" / "workflows"
        cls.files = {p.name: p.read_text(encoding="utf-8") for p in folder.iterdir()
                     if p.suffix.lower() in (".yml", ".yaml")}
        cls.policy = json.loads((ROOT / "scripts" / "SHARED_PUBLICATION_GUARD.policy.json").read_text(encoding="utf-8"))
        cls.candidate = cls.files["candidate.yml"]

    def errors(self, changes=None):
        return scan_workflows(
            {**self.files, **(changes or {})},
            self.policy["expected_artifact_actions"],
            required_disabled_steps=self.policy["required_disabled_steps"],
            allowed_workflows=self.policy["allowed_workflows"],
        )

    def expect(self, changed, marker):
        errors = self.errors(changed)
        self.assertTrue(any(marker in item for item in errors), errors)

    def extra(self, name, body, marker):
        self.expect({name: "on: push\njobs:\n  publish:\n    runs-on: ubuntu-latest\n    steps:\n" + body}, marker)

    def test_existing_workflows_are_source_only(self):
        self.assertEqual(self.errors(), [])

    def test_expected_workflow_inventory(self):
        self.assertEqual(set(self.files), set(self.policy["allowed_workflows"]))

    def test_exact_immutable_pin(self):
        pin = json.loads((ROOT / "scripts" / "SHARED_PUBLICATION_GUARD.pin.json").read_text(encoding="utf-8"))
        raw = (ROOT / "scripts" / "_shared_publication_guard.py").read_bytes()
        self.assertEqual(wrapper.verify_pin(pin, raw), [])
        self.assertTrue(wrapper.verify_pin({**pin, "source_commit": "0"*40}, raw))
        self.assertTrue(wrapper.verify_pin(pin, raw + b"\n# unauthorized"))

    def test_enabled_candidate_upload_rejected(self):
        old = "if: false # No public native component Actions artifact"
        self.assertEqual(self.candidate.count(old), 1)
        self.expect({"candidate.yml": self.candidate.replace(old, "if: true # unsafe", 1)}, "enabled artifact upload")

    def test_removed_candidate_upload_rejected(self):
        old = "uses: actions/upload-artifact@v7"
        self.assertEqual(self.candidate.count(old), 1)
        self.expect({"candidate.yml": self.candidate.replace(old, "uses: actions/checkout@v7", 1)}, "artifact action count")

    def test_renamed_required_disabled_step_rejected(self):
        old = "- name: Upload candidate (held pending separate redistribution permission)"
        self.assertEqual(self.candidate.count(old), 1)
        self.expect({"candidate.yml": self.candidate.replace(old, "- name: Upload renamed", 1)}, "required disabled step")

    def test_unreviewed_workflow_rejected(self):
        self.extra("unexpected.yml", "      - uses: actions/checkout@v7\n", "Unreviewed")

    def test_quoted_upload_rejected(self):
        self.extra("quoted-upload.yml", "      - uses: 'actions/upload-artifact@v7'\n", "enabled artifact upload")

    def test_quoted_release_rejected(self):
        self.extra("quoted-release.yml", '      - uses: "softprops/action-gh-release@v2"\n', "enabled release publication")

    def test_gh_release_cli_rejected(self):
        self.extra("gh-release.yml", "      - run: gh release create v1\n", "enabled release publication")

    def test_alias_rejected(self):
        self.extra("alias.yml", "      - uses: *danger\n", "YAML action alias")

    def test_dynamic_uses_rejected(self):
        self.extra("dynamic.yml", "      - uses: ${{ matrix.action }}\n", "dynamic action")

    def test_block_scalar_rejected(self):
        self.extra("block.yml", "      - uses: >-\n          actions/upload-artifact@v7\n", "noncanonical YAML")

    def test_flow_mapping_rejected(self):
        self.extra("flow.yml", "      - {name: publish, uses: actions/upload-artifact@v7}\n", "noncanonical YAML")

    def test_spaced_key_rejected(self):
        self.extra("spaced.yml", "      - uses : actions/upload-artifact@v7\n", "noncanonical YAML")

    def test_tagged_key_rejected(self):
        self.extra("tagged.yml", "      - uses: !!str actions/upload-artifact@v7\n", "noncanonical YAML")

if __name__ == "__main__":
    unittest.main(verbosity=2)
