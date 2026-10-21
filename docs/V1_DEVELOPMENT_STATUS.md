# Metadata Isolation 1.0.0 development status

Current version: `1.0.0-dev`. This document records **source-only draft CI qualification** in the public repository. It does not supersede private historical acceptance reports and does not authorize a merge, tag or release.

## Previously completed qualification (historical record)

| Phase | Scope | Verdict | Notes |
|---|---|---|---|
| 14 | additional formats and Virtual mechanisms | PASS / closed | Historical evidence retained in private recovery repository |

This historical qualification is not a claim that newer public CI branches were merged or released. The read-only Physical Metadata Audit identifies completeness issues; it does not automatically repair any tags.

## Exact source-only host evidence (20 October 2026)

Latest independently tested source head before this documentation update: `efe2db2249c5e5924235c72f7edc4260cd672101`.

- [Real foobar2000 2.26.1 Win32/x64 full E2E](https://github.com/lxsdd/foo_metadata_isolation-public/actions/runs/39499739747): **PASS on both hosts**. Fast qualification and candidate-build validation also completed successfully.
- Supported virtual `TITLE` edits, qualified `STYLE` skips/partial commits, strict/unknown field rejection, forced rollback, native direct unsafe write Guard, positive physical `TITLE`, four-part virtual ReplayGain and minimal raw-Cue `INDEX 01` edits were verified with synthetic source-only fixtures, appropriate true-change evidence, raw-Cue/ID3/opaque-PRIV/audio checks, and full-byte equality for failed writes.
- 111 virtual Cue tracks were actually enumerated and all edited in two synthetic MP3 payload sizes (~0.58 MB and ~24.9 MB), with bounded shadow size and I/O amplification regression checks. This is not a performance guarantee for the user's very large continuous Mix.
- Real hover tooltips appeared and dismissed in both Light and Dark test probes, Win32/x64; logging redacted four synthetic secret values and preserved Unicode.
- Actual modeless **Physical Metadata Audit** window opened in each host; five native controls passed UIA visible geometry. Read-only scan classified exactly one synthetic source as `INCOMPLETE`; the native result list had one actual row; all **seven native filters** produced the expected counts, and a true native **Rescan** button click triggered another completed audit. Original media remained byte-identical.

The native Audit ListView had previously stored a stale child HWND after host UI initialization. Refresh now reacquires the current control handle before repopulating rows. The narrow fix and real-host result are documented in [draft PR #22](https://github.com/lxsdd/foo_metadata_isolation-public/pull/22) and the full interaction extension in [draft PR #23](https://github.com/lxsdd/foo_metadata_isolation-public/pull/23).

Accessibility evidence: [real Win32/x64 MSAA hard-gate run 39502982702](https://github.com/lxsdd/foo_metadata_isolation-public/actions/runs/39502982702), tested SHA `82a63eed5a4fe824d8dd1297d949d9090f6511ed`. This is a stronger native accessibility assertion but does not fabricate UIA child exposure.

## Real Virtual Cue timing editor + native Discard — 21 October 2026

- Exact tested source SHA `2a05f9b914b5eb8790444b4127eba82ec5563ee9` in [Draft PR #26](https://github.com/lxsdd/foo_metadata_isolation-public/pull/26). Three workflows passed at that SHA: [real foobar2000 Win32/x64 E2E](https://github.com/lxsdd/foo_metadata_isolation-public/actions/runs/39566942096), [Fast Cloud Qualification](https://github.com/lxsdd/foo_metadata_isolation-public/actions/runs/39566942502) and [Qualified Candidate](https://github.com/lxsdd/foo_metadata_isolation-public/actions/runs/39566942498). They are source-only development qualifications, **not** a public release.
- On **both actual hosts**, a synthetically marked three-track embedded-Cue MP3 reached the real production Virtual timing dialog, loaded the exact `TRACK 02 INDEX 01 00:00:30` preimage and passed native Edit/Cancel and read-only verification. A second dialog changed the native text field to `00:00:35`, entered the actual Commit/Discard TaskDialog, clicked **Discard**, and proved the complete original MP3 byte-identical by SHA-256 (`PRODUCTION_VIRTUAL_TIMING_REVIEW_DISCARD_BYTE_EXACT=PASS`). This deliberately does **not** claim an actual UI-driven Commit.
- The original run's official foobar2000 2.26.1 installer request failed on both architectures before testing. Retrying **the identical original SHA** successfully downloaded and hash-validated both pinned installers, exposing test-only cross-process text input and nested TaskDialog button discovery defects; those were corrected in the existing Draft branch. The canonical direct official `/files/` paths match the shared host-smoke script and Microsoft's verified winget package URLs; SHA-256 checks and no-untrusted-fallback remain mandatory.
- Native TaskDialog buttons are functional; hosted UIA button enumeration still yields `PENDING_PROVIDER_UNEXPOSED`. Audit ListView UIA children remain similarly pending. Native success is **not** full UIA or Screenreader acceptance.

### CI publication governance (source-only Draft)

Draft [PR #7](https://github.com/lxsdd/foo_metadata_isolation-public/pull/7) contains an independent source-only candidate-artifact hold and repository-wide workflow audit. The later PRs #9–#26 are stacked without PR #7's original commits; PR #8's hourly migration dispatcher is likewise **not** part of the product head. To avoid silently losing this safety requirement, the reviewed PR #7 audit source is reused on the current Draft head, and both manual `workflow_dispatch` and PR qualification retain packages only on ephemeral Windows runners. The audit runs at Fast Cloud Qualification and rejects newly introduced upload/release surfaces. Re-enabling public candidate artifacts requires the owner's explicit approval; this is not merge/release authorization. Prior versions were not audited by this new gate.

### Source provenance audit (development check, not release authorization)

A fail-closed, mutation-tested tracked-file gate verifies the absence of committed user audio, databases, credentials-by-filename, packaged DLLs and installer archives. The three bundled foobar SDK \`.lib\` files are the *only* permitted tracked prebuilt libraries and are pinned to their reviewed Git object hashes. The SDK, pfc, libPPUI, WTL and combined third-party notices must all remain present and linked. This is **not** a full binary license review or a historical secrets scan. There is no explicit top-level license for the project's own code in the checked Draft snapshot; license selection/authorization is **PENDING OWNER DECISION**, not inferred from the vendor SDK's licenses.

### Historical Git/secret audit — distinct from tracked-tree check

The tracked source/no-user-media gate is **not** a full-history or secrets clearance. The separate `source-history-triage.yml` reads the existing immutable, source-only history and Gitleaks scanners from `lxsdd/dev-infrastructure-public@4a562946975b610651a0a4e71a422699a7a604d9`, scans public reachability including available PR refs, and emits redacted categories/counts only. It does not request secrets or credentials, upload artifacts, or approve redistributability. After the first scan, the workflow was hardened: it now **fails closed** unless all reachable blobs were read, PR refs were scanned, the sole binary matches are exactly the three pinned SDK libraries, and the independent pinned Gitleaks scan has zero findings. Unexpected historical material blocks qualification pending triage. This is still heuristic and not full legal clearance. No full release sign-off is implied.

### Probe-free release binary hard gate

Both Fast Cloud Qualification and Qualified Candidate explicitly call `validate_release_binaries.ps1` immediately after successful **Release** builds for Win32 and x64. It rejects known test-probe/diagnostic strings and absolute or directory-qualified CodeView PDB paths and requires one portable PDB record per binary. This adds repeatable **binary source hygiene** even when the live foobar installer is unavailable; it does not substitute for the currently blocked actual host launch or for an end-user release sign-off.

### Runner-only component archive integrity (strict ZIP layout)

The existing candidate ZIP verifier now inspects archive entries *before extraction*: exactly one root Win32 DLL, exactly one `x64/` DLL of the same name, optional `x64/` directory, no unexpected entries, duplicate/case-colliding names, path traversal, backslash aliases or members above 64 MiB. Architecture-machine checks remain mandatory. A separate synthetic mutation test runs without an installed foobar host on Windows CI and rejects unauthorized payloads. A passing archive test verifies **structure**, not licensing, runtime, or user acceptance.

### Native timing Commit oracle — strict contents, not false positives

The positive UI Commit oracle now checks the **exact raw Cue postimage** (only `TRACK 02 INDEX 01 00:00:30 → 00:00:35`), every physical ID3 frame payload including opaque `PRIV`, and all MPEG audio bytes independently. It tolerates non-semantic ID3 padding changes or frame reordering by a legitimate host tag writer. Cancel/Discard/rejected writes still require complete original-file byte identity. The offline mutation regression tests both unchanged-container and padded-container valid commits plus five invalid postimages. **This is test-oracle correction only; actual positive host UI Commit is not yet PASS.**

### Stacked Draft consolidation evidence

The checked GitHub HEAD dependencies, the intentionally diverged and unmerged Drafts #7/#8, and the no-merge/no-release plan are recorded in [DRAFT_PR_DEPENDENCY_AUDIT.md](DRAFT_PR_DEPENDENCY_AUDIT.md). Current CI source checks are not authorization to consolidate into `main`.

### Candidate SHA provenance on Pull Requests

For PR CI, `GITHUB_SHA` is the **checked-out synthetic test-merge commit**, not automatically the Draft branch HEAD. The runner-only packaging step now requires full Git ancestry, verifies that the test-merge parents are exactly the event's base and PR-head SHAs, and records both `built_checkout_sha` and `source_head_sha` in the manifest. Manual direct-branch runs record their checked-out commit as both. This eliminates misleading single-SHA labels without authorizing a release, artifact upload, or replacing real host validation.

### Candidate archive bytes verified against exact compiled DLLs

The isolated candidate ZIP verifier can require an **ordered pair of original Win32/x64 build paths** and SHA-256 compares each extracted archive DLL to its compiled counterpart. The packager makes that comparison mandatory, and the manifest includes both verified DLL digests plus `PayloadMatchesBuild=true`. A disposable Windows mutation test verifies one matched pair and rejects four forgery/incomplete-provenance cases, including same-architecture modified DLL bytes. This closes a gap where correctly named and architecturally plausible, but wrong, DLLs could otherwise pass structural checks. Packages remain on ephemeral CI runners.

### Ready-to-run acceptance specification (not yet executed)

The [real-host and owner acceptance runbook](V1_REAL_HOST_AND_OWNER_ACCEPTANCE_RUNBOOK.md) records the precise Win32/x64 host, synthetic safety, authenticated Discogger/Masstagger, original-media-copy/111-track, accessibility, performance and final license/approval gates. These are explicitly **PENDING**, not implied by successful source CI. Historical-secret scanning has its own fail-closed workflow; the tracked-tree provenance test must not print an outdated combined `FULL_HISTORY_SECRET_AUDIT=PENDING` label.

### Unblocked official host installer transport — existing immutable installer assets

The formerly unavailable direct `www.foobar2000.org/files/` download caused external infrastructure failures in actual Win32/x64 E2E. A newer [shared owner-controlled infrastructure revision](https://github.com/lxsdd/dev-infrastructure-public/commit/35369054c56f4c6ccb2f802944e9675852fd35ec) has already launched **both real foobar hosts** from existing versioned, **unmodified original installer files** in the owner's existing GitHub Releases, enforcing the same independently pinned Microsoft winget SHA-256 digests. PR #26 now uses this reviewed exact revision for candidate host-smoke and checks out its source-only installer helper for E2E. Every installer is freshly SHA-256 verified, is job-local and ephemeral; this PR **does not create, upload or distribute a component DLL or any new installer asset**. Fail closed on mirror/network/checksum mismatch. The actual native timing Commit remains **NOT PASS until this Draft's new Win32/x64 run completes successfully**. This is a transport-only change, not a production C++ alteration.

### Separate native TaskDialog MSAA accessibility qualification — experimental Draft

The existing actual Win32/x64 UI tests confirm the native Commit/Discard captions and real mouse interaction, but the hosted UIA provider did not enumerate its buttons. A separate, disposable test branch checks the **independent oleacc/MSAA name and pushbutton role** for both actual review controls and refuses a native default action of Commit. This is an *experimental additional gate*, not proof of full UIA or screen-reader behavior until the exact branch completes real host CI. Production C++ and the previously qualified PR #26 remain unchanged.

### Real native default-action discrepancy and safety remediation (experimental Draft #28)

An independent Windows/MSAA experiment exposed **exact accessible names and pushbutton roles** for both real TaskDialog actions on Win32 and x64. However, `DM_GETDEFID` returned **IDOK / Commit** as the current native default on both hosts, even though the source already set `config.nDefaultButton=IDCANCEL`. This is a measured native-dialog discrepancy, not proof by itself that Enter caused an actual write. As defense in depth, the review now **orders non-writing Discard first**, followed by explicit Commit, while retaining `nDefaultButton=IDCANCEL` and strict return semantics. A separate Windows E2E must prove the default is safe and that Cancel/Discard/explicit positive Commit still work. Until such a run is green this fix is **NOT QUALIFIED** and PR #26 remains unchanged. Microsoft documents that an invalid or missing default ID falls back to the first button; a non-writing first button is safe under both interpretations.

## Remaining release gates — explicitly NOT PASS

1. **Real third-party workflow end-to-end:** authenticated Discogger and real Masstagger with supported changes plus unsupported multivalue `STYLE`, correctly restored third-party configs, observable non-no-op commit and complete postimage; security, license and secret handling apply.
2. **Usability and real media:** actual large 111-track Mix on the user's computer; rapid intuitive operations, comprehensible accepted/skipped/blocked and pending states, no accidental physical tag changes, expected performance on real audio size.
3. **Accessibility/UIA subitems:** MSAA now **PASS** in actual foobar2000 Win32/x64: from three native accessible children, exactly one is `ROLE_SYSTEM_LISTITEM` with a readable synthetic `Incomplete` status; no COM role/name errors. Hosted Windows UIA nevertheless exposes no `DataItem` or `ListItem` child for that same native row (`PENDING_PROVIDER_UNEXPOSED`). Do **not** claim direct UIA or screen-reader end-to-end acceptance; native rows/filters/Rescan **PASS**.
4. **Broader real UI qualification:** complete Preferences, timing and audit controls, varying actual DPI and screen setups, screen readers where applicable, final end-user ergonomics and visual review.
5. **Release governance:** consolidate/review stacked Draft source changes, verify licensing and source-cleanliness, test probe-free Win32/x64 delivery artifacts, confirm no private fixture/config/secrets or diagnostic DLLs are distributed, obtain explicit final release approval.

All current GitHub work is **draft / unmerged**; a successful test-build is not a published installation candidate. There is no authorization to merge, tag, publish a package or change the user's installed foobar2000 profile.
