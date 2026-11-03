#!/usr/bin/env python3
"""Metadata Isolation's source-only publication gate, pinned to shared source.

The canonical scanner is additive to product-specific provenance, license,
candidate packaging and physical/virtual/CUE-write safety rules.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
EXPECTED_PIN = {
    "source_repo": "lxsdd/dev-infrastructure-public",
    "source_commit": "4ea764a2e586ebe3c4274634666f77b3370dc246",
    "source_path": "shared/ci/publication_guard.py",
    "source_git_blob_sha1": "ff55f22ac0aafc500e8ee5235e253d0bb4ad8d69",
    "license_and_scope": "Source-only reusable CI scanner; no permission for component binary publication; product-specific physical/virtual/CUE and release gates remain mandatory"
} 
# The JSON-formatted literal above is intentionally a Python dict: JSON strings
# are valid Python literals, and all values are immutable reviewed strings.

def git_blob_sha1(raw: bytes) -> str:
    raw = raw.replace(b"\r\n", b"\n")
    return hashlib.sha1(b"blob " + str(len(raw)).encode("ascii") + b"\0" + raw).hexdigest()

def verify_pin(pin_data: dict, source: bytes) -> list[str]:
    errors = []
    if pin_data != EXPECTED_PIN:
        errors.append("exact shared guard source pin differs from reviewed owner commit/path/blob")
    if git_blob_sha1(source) != EXPECTED_PIN["source_git_blob_sha1"]:
        errors.append("canonical shared guard bytes differ from reviewed Git blob")
    return errors

def main() -> int:
    try:
        pin_data = json.loads((SCRIPTS / "SHARED_PUBLICATION_GUARD.pin.json").read_text(encoding="utf-8"))
        source = (SCRIPTS / "_shared_publication_guard.py").read_bytes()
        policy = json.loads((SCRIPTS / "SHARED_PUBLICATION_GUARD.policy.json").read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        print("FAIL_ISOLATION_SHARED_PUBLICATION_FILES: " + str(exc))
        return 1
    errors = verify_pin(pin_data, source)
    if errors:
        print("FAIL_ISOLATION_SHARED_PUBLICATION_PIN:\n- " + "\n- ".join(errors))
        return 1
    from _shared_publication_guard import audit_repository
    errors = audit_repository(ROOT, policy)
    if errors:
        print("FAIL_ISOLATION_PUBLICATION_SURFACE:\n- " + "\n- ".join(errors))
        return 1
    print("PASS_ISOLATION_SHARED_PUBLICATION_GUARD_EXACT_OWNER_BLOB")
    print("PASS_ISOLATION_PUBLICATION_SURFACE_SOURCE_ONLY")
    print("ISOLATION_BINARY_PUBLICATION_AUTHORIZED=NO")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
