# Metadata Isolation 1.0.0 development status

Current version: `1.0.0-dev`. This document records **source-only draft CI qualification** in the public repository. It does not supersede private historical acceptance reports and does not authorize a merge, tag or release.

## Exact source-only host evidence (20 October 2026)

Latest independently tested source head before this documentation update: `efe2db2249c5e5924235c72f7edc4260cd672101`.

- [Real foobar2000 2.26.1 Win32/x64 full E2E](https://github.com/lxsdd/foo_metadata_isolation-public/actions/runs/39499739747): **PASS on both hosts**. Fast qualification and candidate-build validation also completed successfully.
- Supported virtual `TITLE` edits, qualified `STYLE` skips/partial commits, strict/unknown field rejection, forced rollback, native direct unsafe write Guard, positive physical `TITLE`, four-part virtual ReplayGain and minimal raw-Cue `INDEX 01` edits were verified with synthetic source-only fixtures, appropriate true-change evidence, raw-Cue/ID3/opaque-PRIV/audio checks, and full-byte equality for failed writes.
- 111 virtual Cue tracks were actually enumerated and all edited in two synthetic MP3 payload sizes (~0.58 MB and ~24.9 MB), with bounded shadow size and I/O amplification regression checks. This is not a performance guarantee for the user's very large continuous Mix.
- Real hover tooltips appeared and dismissed in both Light and Dark test probes, Win32/x64; logging redacted four synthetic secret values and preserved Unicode.
- Actual modeless **Physical Metadata Audit** window opened in each host; five native controls passed UIA visible geometry. Read-only scan classified exactly one synthetic source as `INCOMPLETE`; the native result list had one actual row; all **seven native filters** produced the expected counts, and a true native **Rescan** button click triggered another completed audit. Original media remained byte-identical.

The native Audit ListView had previously stored a stale child HWND after host UI initialization. Refresh now reacquires the current control handle before repopulating rows. The narrow fix and real-host result are documented in [draft PR #22](https://github.com/lxsdd/foo_metadata_isolation-public/pull/22) and the full interaction extension in [draft PR #23](https://github.com/lxsdd/foo_metadata_isolation-public/pull/23).

## Remaining release gates — explicitly NOT PASS

1. **Real third-party workflow end-to-end:** authenticated Discogger and real Masstagger with supported changes plus unsupported multivalue `STYLE`, correctly restored third-party configs, observable non-no-op commit and complete postimage; security, license and secret handling apply.
2. **Usability and real media:** actual large 111-track Mix on the user's computer; rapid intuitive operations, comprehensible accepted/skipped/blocked and pending states, no accidental physical tag changes, expected performance on real audio size.
3. **Accessibility/UIA subitems:** the hosted Windows native UIA provider exposed no `DataItem` or `ListItem` child for an otherwise proven native ListView row. This is `PENDING_PROVIDER_UNEXPOSED`, **not** a UIA row PASS. Native row/filter/Rescan interaction **is** PASS.
4. **Broader real UI qualification:** complete Preferences, timing and audit controls, varying actual DPI and screen setups, screen readers where applicable, final end-user ergonomics and visual review.
5. **Release governance:** consolidate/review stacked Draft source changes, verify licensing and source-cleanliness, test probe-free Win32/x64 delivery artifacts, confirm no private fixture/config/secrets or diagnostic DLLs are distributed, obtain explicit final release approval.

All current GitHub work is **draft / unmerged**; a successful test-build is not a published installation candidate. There is no authorization to merge, tag, publish a package or change the user's installed foobar2000 profile.
