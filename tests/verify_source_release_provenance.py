"""Source-only release provenance guard. Not a license/secret clearance.

Reject tracked user audio, config/databases, distributable binaries and new
unreviewed binary libraries. Preserve exact third-party vendor notices.
Proves only this limited set of release prerequisites, not overall compliance.
"""
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NOTICES = {
    "sdk/sdk-license.txt",
    "sdk/pfc/pfc-license.txt",
    "sdk/libPPUI/libPPUI-license.txt",
    "wtl/MS-PL.txt",
    "THIRD_PARTY_NOTICES.md",
}
PINNED_VENDOR_LIBS = {
    "sdk/foobar2000/shared/shared-ARM64EC.lib": "ea9bbe91d4aa504b44601a2df9ce7fde6e92a2b9",
    "sdk/foobar2000/shared/shared-Win32.lib": "da0ccb0a90350e2a2c71db58b40028f1dffb3f31",
    "sdk/foobar2000/shared/shared-x64.lib": "ce643a80efff8c999dbf64ec3d49f95d8ae873f6",
}
FORBIDDEN = re.compile(
    r"\.(?:mp3|flac|wv|ogg|opus|wav|aiff?|cue|mp4|m4a|db|sqlite|"
    r"sqlite3|dll|exe|fb2k-component|zip|7z|rar|pfx|p12|pem|"
    r"key|kdbx|pdb|obj|ilk|idb|cache)$", re.I,
)
FORBIDDEN_NAMES = {".env", "id_rsa", "id_ed25519", "credentials.json", "secrets.json"}

def issues(paths: set[str], libs: dict[str, str], notice_text: str) -> list[str]:
    errors = []
    for path in paths:
        if "\\" in path or path.startswith("/") or ".." in path.split("/"):
            errors.append("unsafe tracked path")
        if FORBIDDEN.search(path) or Path(path).name.lower() in FORBIDDEN_NAMES:
            errors.append(f"blocked tracked binary, media or credential filename: {path}")
        if path.lower().endswith(".lib") and path not in PINNED_VENDOR_LIBS:
            errors.append(f"unreviewed binary library: {path}")
    missing = NOTICES - paths
    errors.extend(f"missing original third-party notice: {path}" for path in sorted(missing))
    for name, sha in PINNED_VENDOR_LIBS.items():
        if name not in paths or libs.get(name) != sha:
            errors.append(f"missing or modified vendor library: {name}")
    for name in sorted(NOTICES - {"THIRD_PARTY_NOTICES.md"}):
        if name not in notice_text:
            errors.append(f"third-party notices do not reference: {name}")
    return errors

def git(*args: str) -> bytes:
    return subprocess.check_output(("git", *args), cwd=ROOT)

def main() -> None:
    paths = {x.decode("utf-8") for x in git("ls-files", "-z").split(b"\0") if x}
    libs = {name: git("hash-object", "--", name).decode("ascii").strip()
            for name in PINNED_VENDOR_LIBS if name in paths}
    text = (ROOT / "THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8") if (
        "THIRD_PARTY_NOTICES.md" in paths) else ""
    found = issues(paths, libs, text)
    if found:
        raise SystemExit("SOURCE_PROVENANCE_FAIL:\n- " + "\n- ".join(found))
    assert issues(paths | {"fixtures/user-dj-mix.mp3"}, libs, text)
    assert issues(paths | {"sdk/unknown-executable.lib"}, libs, text)
    assert issues(paths - {"sdk/sdk-license.txt"}, libs, text)
    assert issues(paths, {**libs, "sdk/foobar2000/shared/shared-x64.lib": "0" * 40}, text)
    print("SOURCE_PROVENANCE_TRACKED_MEDIA_AND_BINARY_HOLD=PASS")
    print("SOURCE_PROVENANCE_VENDOR_NOTICE_AND_BLOB_PINS=PASS")
    print("SOURCE_PROVENANCE_MUTATION_CASES=PASS")
    print("OWN_SOURCE_LICENSE=PENDING_OWNER_DECISION")
    print("HISTORICAL_SECRET_AUDIT=SEPARATE_SOURCE_ONLY_WORKFLOW")

if __name__ == "__main__":
    main()
