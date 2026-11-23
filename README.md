# Metadata Isolation for foobar2000

Native foobar2000 2.x component (Win32/x64) for keeping physical tags separate
from embedded textual-CUESHEET virtual metadata.

Current status: **1.0.0-dev**. This is an unqualified public-build source
snapshot with an integrated read-only **Physical Metadata Audit**.
It is **not** a release or user-accepted binary.

## Physical Metadata Audit

Scan the Media Library or selected tracks to find MP3/FLAC/WavPack sources with
embedded textual CUESHEETs that may mask missing or inconsistent physical tags.
The audit reports OK, EMPTY, INCOMPLETE, MISMATCH or REVIEW and never writes tags.
An explicit user choice can open the existing physical Properties editor.

## Build and qualification

Use the GitHub Actions FAST workflow for Win32/x64 compilation and static tests.
The manual CANDIDATE workflow produces an SHA-bound package for real foobar2000
runtime testing. Candidate success alone is not user acceptance.

The project-owned source code and documentation are licensed under **MIT
License with "Commons Clause" License Condition v1.0**, copyright (c) 2026
`lxsdd`, as separately adopted from
[`foo_dj_library_bridge`](https://github.com/lxsdd/foo_dj_library_bridge/blob/main/LICENSE).
See [LICENSE](LICENSE). **The combined license is source-available, not
OSI-approved open source:** the Commons Clause restricts selling the software
as defined in its text. This license **does not relicense** the foobar2000 SDK,
pfc, libPPUI, WTL, original player/installer, tools or other third-party
materials; [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) preserves their
independent notices. A project-owned source-license decision is **not** a
binary distribution, download, signing, merge or release authorization.
All GitHub Actions component binaries remain runner-only.

Historical private test reports, runtime configurations, screenshots,
archival branches, release binaries and recovery commits are intentionally
not part of this clean public snapshot.