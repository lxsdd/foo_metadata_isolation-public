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

Public build inputs include the foobar2000 SDK and WTL under their respective
upstream license notices. The component's original source is not automatically
licensed for third-party redistribution or modification merely because this
repository is visible; licensing must be decided separately.

Historical private test reports, runtime configurations, screenshots,
archival branches, release binaries and recovery commits are intentionally
not part of this clean public snapshot.