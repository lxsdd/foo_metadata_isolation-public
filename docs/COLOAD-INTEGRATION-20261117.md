# Current three-component qualification

Metadata Isolation baseline: `849a9a9a715b151e13804ae65f8959bad3ceb922`.
Studio pin: `bcf5d456cdf86fae7d986849468d78f9edb5e995` (combined Precision CUE,
marked MP3 scope plus mandatory original mastergrid regressions; exact-head
partner qualification is required before publication of this pin).
Discogger pin: `e67f9c97423e2f5a7736e91b3399ac531e05d844` (selective master
integration: all-failed optional discovery cannot publish an empty success).

The co-load workflow builds these immutable sources together with this exact
Isolation revision. The existing original-host physical audit, no-op, embedded
CUE preservation, UIA and virtual write guards remain required. Companion builds
do not activate Studio writers or copy partner source into this component.

The inventory timestamp baseline now precedes the original inventory probe.
Every embedded-CUE inventory invocation also checks bytes and timestamp against
its own pre-probe snapshot. Disposable negative tests independently change only
the timestamp and only the bytes (restoring timestamp), and must both reject.

Combined Win32/x64 co-load, original Masstagger and SDK-host CI must pass on
this resulting commit; individual partner green runs are not co-load evidence.
No publication, merge, owner-media access or final GUI acceptance is implied.

The five required Isolation jobs now explicitly check out and verify the product head instead of relying on the synthetic PR merge. This co-load rerun includes the current Studio marked-source implementation and regression additions. Historical successful runs of the older pins do not qualify this new combination.

## Audit column-state hardening (18 November 2026)

The native Physical Metadata Audit restores persisted widths, visibility and order
only from complete numeric tokens and a valid column permutation. A malformed
visibility mask or order is rejected atomically before the ListView visibility
is changed; the default visible columns remain available. The original native
Win32/x64 audit and host workflows must qualify this source together with the
existing immutable Studio/Discogger pins. This is not a new partner pin or a
release acceptance.

## Native persisted-column regression

The FB2K_ENABLE_TEST_PROBES build injects valid and corrupt cfg-backed column states in the actual Audit dialog, calls the production restore_columns implementation and checks ListView widths and header order. It covers a valid reversed order and visibility mask, trailing junk in mask/order/widths, empty/out-of-range masks, duplicate indices and extra separators. Original cfg values and native defaults are restored before normal initialization. The existing original-host UIA run now requires audit_column_restore_pass=true from the existing read-only diagnostic command. Production builds omit the injection.

Partner pins advance to Studio bcf5d456 (six required jobs PASS including original dialog Win32/x64) and Discogger e67f9c97 (complete native CI PASS). New joint host qualification remains pending on this commit.

The new original-host regression exposed a production lifecycle defect on both architectures: theme initialization replaced the ListView and its current header had zero columns. Row refresh already rebound the HWND, but column initialization still used the earlier handle. Column setup now resolves the current control after theme hooks and repeats after a later replacement, validates the existing schema and rejects failed column creation. This preserves real widths/order and prevents duplicating existing columns.

## Audit replacement-scan invalidation (19 November 2026)

The existing empty-scope invalidation remains intact. A subsequent nonempty
read-only scan now invalidates the prior physical target rows, native selection,
and accessible detail **before** starting the asynchronous worker. The original
foobar host regression checks the live rescan transition when the scanning
state is observable and requires the existing completed synthetic classification.
The static safety contract also asserts the pre-worker ordering. This does not
claim full UIA row-provider exposure, Preset/Rollback acceptance, or media writes.

## Library-enumeration failure invalidation (20 November 2026)

The original audit scan previously returned from the Media Library enumeration
exception handler before invalidating rows from the prior selection/scope.
The native error path now clears the physical-target model, refreshes the
ListView and accessible detail, disables the stale Properties eligibility
through the existing selection gate, and restores Rescan/Cancel availability.
A source regression checks the exception-path ordering. Original-host
fault-injection for failed enumeration and the complete selection/recovery
sequence remain separate acceptance work; no production metadata is written.

Integration checklist: existing empty/nonempty scan invalidation retained;
enumeration failure cleanup integrated on this Draft; immutable Studio/Discogger
pins unchanged; original-host exception injection and full UIA row provider
not yet accepted.
