# Metadata Isolation v1.0 — real host and owner acceptance runbook

**Status:** source-only readiness checklist, **NOT RELEASED / NOT AUTHORIZED**. Do not execute on the owner's live music library, authorize a DLL upload, alter third-party plug-in settings or merge a Draft PR merely because this checklist exists. The checklist is designed to make the eventual one-time end acceptance safe and unambiguous. It is not evidence that a test ran.

## Gate 0 — freeze evidence and establish disposable boundaries (CI, autonomous)

- Record a single Draft branch **source-head SHA**, the tested GitHub **checkout/PR test-merge SHA**, and its PR-base parent. All linked CI logs must correspond to those exact SHAs; an older green E2E is historical evidence only.
- Use the pinned original, unmodified **foobar2000 2.26.1** Win32/x64 installers and their reviewed SHA-256 values. The already-existing owner-hosted byte-identical GitHub Release assets may be used only through the immutable checked, same-hash transport helper; never use unknown third-party mirrors, modified installers, or unverified caches. Host acceptance is still pending until an actual exact-HEAD CI run passes.
- Use disposable Windows runners, isolated portable foobar profiles and deterministically generated MP3/CUE fixtures. Run Windows host/smoke and production UI interaction in **both** architectures; never load unapproved components into the user's active profile.
- Protect write safety: original source file SHA-256, full physical ID3 frame payloads (including opaque PRIV), MPEG audio, raw embedded CUE, and relevant ReplayGain fields must be measured independently before and after.
- Keep runner-only candidate packages and all probe-enabled DLLs inaccessible to users and unpublished; current uploads remain disabled.

## Gate 1 — safety and actual production UI, Win32 and x64 (CI, autonomous)

Use actual foobar hosts rather than a mock or a test-driver substitute for any acceptance marked real host.

1. Confirm component load and real modeless Physical Metadata Audit, seven filters, rescan, stable list view selection and persistent window placement. The audit must never write tags.
2. Exercise physical field add/change/delete, full-reject behavior for unknown/unrepresentable edits, and permitted skip-only multivalue `STYLE` partial commits; prove there is no silent fallback to virtual write.
3. Exercise embedded Cue Track TITLE, raw `INDEX 01` change, ReplayGain virtual/physical add/change/delete and preserved opaque frame/audio postimages. For rejected/stale/conflicting/discarded changes require *whole-file* byte equality.
4. In the **real Virtual Timing modal editor**, verify exact three-track synthetic fixture identity, TRACK 2, `INDEX 01 00:00:30`, mandatory directive behavior, Cancel and the actual native Review/Discard dialog. Require Discard to preserve the entire synthetic original file SHA256.
5. Perform a **positive real Commit**, changing only TRACK 2 `INDEX 01 00:00:30 → 00:00:35`. The independent raw-Cue/ID3/MPEG oracle must prove exactly the intended CUE change, all original physical frame payloads, opaque PRIV, and MPEG bytes. Valid non-semantic ID3 re-padding must not be treated as data loss. A diagnostics-only writer result is insufficient.
6. Distinguish observed native task-dialog button clicks and MSAA rows from full UIA/screen-reader support. Existing hosted UIA TaskDialog and ListView row children are **PENDING_PROVIDER_UNEXPOSED**, not accessible PASS.

**Fail:** source write outside the accepted postimage, unhandled rollback, crash, wrong track, stale commit, incomplete real host coverage, or an observed regression. **Block:** official installer unavailable. Do not turn either into PASS.

## Gate 2 — real third-party workflow (requires genuine plug-in/account access)

- Use **real installed Discogger** with an authorized authenticated workflow and **real Masstagger**; do not claim PASS based solely on direct API/probe bypasses.
- Before any test, create a **byte-exact** secure backup of each relevant plug-in configuration and the chosen disposable media clone; record hashes and exact recovery path.
- On the *clone*, exercise a non-no-op supported metadata change in combination with a provably skippable multivalue `STYLE`, plus an independent blocking unknown/unrepresentable field scenario. Check actual visible accepted/skipped/blocked state, correct physical vs virtual target, and the independent resulting raw tag/CUE postimage.
- Restore original third-party configurations byte-exactly, and verify their final hashes against preimages. No credentials or private paths in logs, reports, Git, pull requests or user-visible artifacts.
- If authentication/provider availability is absent, **PENDING_AUTHENTICATED_E2E**, not a green simulated result.

## Prepared read-only media clone command (not executed on owner data)

A safe local PowerShell preflight is now available at `scripts/Prepare-MetadataIsolationAcceptanceCopy.ps1`. It requires the owner to explicitly choose an **existing original Mix file** and an **independent existing destination directory**; it never edits the source, overwrites an existing clone, follows reparse points, uploads anything, installs a foobar component or authorizes any metadata write. It checks the original SHA-256 before and after copying, checks the clone SHA-256 and length, and creates a local source-path-free verification manifest marked `write_acceptance=NOT_AUTHORIZED`. Its separate CI job uses **only invented bytes**, not the user's Mix; synthetic copy PASS is not real media/cuesheet/performance PASS.

After local paths are selected by the owner, the basic command is `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\Prepare-MetadataIsolationAcceptanceCopy.ps1 -SourceMix "X:\path\actual-mix.mp3" -DestinationRoot "C:\independent-existing-test-folder"`. This is documentation, **not a request to execute now**. A separately authorized run would still need to open the copied file in genuine foobar2000 and validate 111 indexed subsongs, UI and behavior; do not touch the original to run it.

## Copyright-free full-length 111-track Mix substitute

The owner cannot upload the real 111-track recording because of file size and copyright; **do not request an upload of that source**. The real Windows host test now has a separate fully synthetic 111-track MP3 substitute: reproducible MPEG-1 Layer III frames at 44.1 kHz / **320 kbit/s**, approximately **84.4 minutes**, roughly **193 MiB**, and 111 embedded Cue INDEX entries 45 seconds apart. The test generates the file inside disposable Windows runner temporary directories, executes real foobar2000 Win32/x64 subtrack inventory and a non-no-op 111-track virtual TITLE commit, and uses streaming hashes to verify every MPEG payload byte remained unchanged, all physical ID3/opaque PRIV fields are preserved, and only intended CUE TITLE values changed. This is an independent scale/reliability test, not a claim that the user's unreleased copyrighted work was obtained or examined. Read-only performance/ergonomics comparisons on synthetic content can be completed autonomously. **Actual user-specific sound, metadata peculiarities and personal UX feedback remain outside CI's evidence**.

## Gate 3 — owner's real large 111-track Mix (deferred until owner can safely validate)

- The owner must explicitly authorize this step, identify the appropriate source and decide an **independent disposable copy** location. Never mutate, reorganize or replace the live Mix implicitly.
- Verify full copy SHA-256 and physical tags/audio/CUE state before testing. Run read-only inventory and compare all 111 cue tracks/segment mappings, then perform explicitly authorized accepted/rejected edits **on the clone only**.
- Measure real document size, time to open/scan/review/commit, maximum memory and temporary I/O, and time to refresh controls. Record actual metrics; do not invent pass thresholds or claim synthetic 111-track benchmarks represent the user's continuous Mix.
- Show that no physical `TITLE/ARTIST/TRACKNUMBER`, user-defined tag, ReplayGain or unrelated cue token was silently lost. Confirm duplicate/subsong distinctions and physical vs virtual scope remain understandable.
- Keep source untouched and preserve a tested rollback path. Obtain explicit user ergonomic/performance verdict instead of inferring acceptance from CI.

### Independent native audit keyboard/MSAA bridge qualification (synthetic CI)

A real Win32/x64 accessibility regression now verifies the original audit ListView can obtain **keyboard focus using native Windows dialog focus routing**, consume an actual HOME keypress from the isolated GitHub Windows desktop, select the single synthetic INCOMPLETE row, and expose that selected/focused row through the **original Win32/MSAA accessibility object**. The test checks exact native parent/child process ownership and foreground focus, uses only runner-generated MP3 and does not call tag-writing actions; a failure cannot be promoted to PASS. This narrows the gap between mouse-only Audit and keyboard usability but is **not** a claim that the hosted UIA provider exposes descendants, that a genuine screen reader was exercised, or that owner display configurations were covered. Qualified only after exact-HEAD Win32/x64 E2E completes.

## Gate 4 — UI, accessibility and ordinary DJ workflow

- Review at different available Windows DPI scaling, display positions/multimonitor configurations, light/dark/high-contrast themes, and maximize/minimize/modeless behavior where applicable.
- Check keyboard traversal, logical reading order, visible focus, label/input alignment, stable column widths, overflow/scrollbar reachability, tooltips on hover and disappearance, row selection, and 111-track review density.
- Check screen reader announcements and real UIA tree elements for ListView rows and TaskDialog Commit/Discard **separately** from MSAA/natively clickable controls; do not promote native interaction evidence to UIA PASS.
- Confirm the workflow clearly distinguishes physical metadata, inherited virtual Cue, pending changes, skipped only-safe fields, blocking conflicts, Reset/Cancel/Discard, and rollback confirmation.

## Gate 5 — exact candidate and release authorization

- The strict combined component ZIP must contain only the original Win32 DLL and its corresponding x64 DLL, pass exact **built-binary SHA-256 comparisons**, no unexpected path entries, probe/diagnostic markers, unportable PDB paths or credentials, and report build-head/checkout SHAs separately.
- Historical Git-source and independent Gitleaks audits must remain green for the exact qualified source. Include the SDK/WTL notices. The owner has now **selected MIT plus "Commons Clause" v1.0** for
the component's own project code/docs, by adopting the existing Bridge grant.
Check [LICENSE](../LICENSE) and [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)
for distinct proprietary/project and independently licensed vendor scopes.
This is **source-available rather than OSI open source**. License selection
is **PASS**; actual rights/provenance review and final public component
distribution remain **PENDING**, independently of the chosen license.
- Release authorization needs **all mandatory gates genuinely PASS**, a final source/CI audit, and an **explicit separate approval by the owner**. Until then **no merge, release, tag, publication, DLL artifact or new background task** is authorized.

## Evidence vocabulary

Use `PASS` only for a measured real run, `FAIL` for observed behavior that violates a contract, `BLOCKED_INSTALLER` for inaccessible official host binaries, `PENDING_PROVIDER_UNEXPOSED` for unavailable UIA elements, `PENDING_AUTHENTICATED_E2E` for unavailable real provider tests, and `PENDING_OWNER_ACCEPTANCE` for the final personal media/ergonomics/license decision. Each result must carry its SHA, architecture, fixture type and bounded non-private evidence.

**Current truth (23 October 2026; exact source HEAD `fa6f0b3857e849618233609728d55094f842b798`):** The latest fully qualified stacked Draft is [#56](https://github.com/lxsdd/foo_metadata_isolation-public/pull/56), not the earlier Draft #32. All ten [exact-head Actions workflows](https://github.com/lxsdd/foo_metadata_isolation-public/commit/fa6f0b3857e849618233609728d55094f842b798/checks) completed SUCCESS, including genuine original foobar2000 2.26.1 Win32/x64, SDK 2026-10-01, physical/virtual writes and rollback, candidate integrity, historical security, and the 84.4-minute 111-track synthetic MP3. The [genuine original Masstagger 1.9.3 native run](https://github.com/lxsdd/foo_metadata_isolation-public/actions/runs/39847514078) passed non-no-op physical embedded-CUE/ID3 writes with an independent audio/tag oracle in both architectures. The [modified Discogger co-load run](https://github.com/lxsdd/foo_metadata_isolation-public/actions/runs/39847513962), pinned to Discogger source `2d4780a402c4faf62481a1c867e6d8db83cf4c80`, passed both architecture host loads, genuine SDK physical MP3 writes, direct virtual-write rejection and independent original tag/CUE/audio preservation. These are real disposable Windows host tests, but they are **not** an authenticated, ordinary Discogger GUI tagging session on the owner's account. The central protected [Discogs OAuth1 live test](https://github.com/lxsdd/dev-infrastructure-public/actions/runs/39920726555) separately passed read-only identity and all five direct releases; it never supplies credentials to the native co-load job. Original owner 111-track media/ergonomics, true UIA provider/screen-reader behavior, authenticated third-party GUI tagging, final provenance/distribution rights and owner acceptance remain independently unverified. Do not infer release permission from any CI PASS. No merge, release, tag, DLL upload, public binary or user-library write is authorized.
