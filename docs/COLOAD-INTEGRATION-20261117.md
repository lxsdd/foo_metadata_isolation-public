# Current three-component qualification

Metadata Isolation baseline: `849a9a9a715b151e13804ae65f8959bad3ceb922`.
Studio pin: `d179d2aabd5cb16450343c47748184cc2d1a1a9a` (combined Precision CUE,
marked MP3 scope plus mandatory original mastergrid regressions; exact-head
partner qualification is required before publication of this pin).
Discogger pin: `a12a6c6770515a032992d82540d7106b36e9ca74` (selective master
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
