# Real Masstagger 1.9.3 acceptance — source-only safe gates

The owner reports that the official foobar2000 site may have recovered after
its temporary PHP security shutdown. This PR repeats download testing on a
**new isolated GitHub Windows runner** and uses **only official HTTPS
foobar2000.org endpoints**, with no fallback to third-party binaries.

Gate 1: fetch official package, bound the archive size, inspect path traversal,
duplicates and DLL presence; log SHA-256. No package bytes are uploaded,
committed or cached, and no DLL is executed. If official source is down, mark
BLOCKED_OFFICIAL_SOURCE, not a Masstagger functional PASS.

Gate 2 (only after independently pinning/reviewing exact package SHA-256):
install the exact package on disposable foobar2000 Win32/x64 profiles, load
both real components, and execute real non-no-op Masstagger add/change/remove
on copyright-free physical/cue-enabled media. Verify physical-vs-virtual
postimage and actual rolled back or rejected mutations. Preserve and
restore exact configuration state with SHA-256.

Gate 3: user-specific presets/settings may only be tested after a legitimate
user-authorized, sanitized configuration copy exists. Successful generic
Masstagger tests do not prove the user's preset behavior.

Release/merge, installer publication and user audio access remain separately
forbidden by the existing source-only policy.
