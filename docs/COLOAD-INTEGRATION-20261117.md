# Current three-component qualification

Metadata Isolation baseline: `849a9a9a715b151e13804ae65f8959bad3ceb922`.
Studio pin: `ddf3b5a5b2483dcc5c36b99ec382b76e67be58d9` (combined Precision CUE,
all six required source jobs and original Win32/x64 dialog dispatch passed).
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
