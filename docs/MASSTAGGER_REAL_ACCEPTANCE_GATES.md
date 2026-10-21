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

## Strict historical Gitleaks false-positive triage

An actual independent Gitleaks scan flagged **one** newly introduced test
literal from the first genuine Masstagger experiment (commit
`f355c31a991fc802f6cf8983ef2a3380ddfbf898`, test script line 120).
The line is exactly a fixed synthetic `COMMENT` value on disposable
copyright-free media, *not* a user credential or live API key.

History security is not disabled: the scanner still examines the complete
reachable history and all PR heads; a narrow conditional checks one
exact redacted result fingerprint (commit prefix, file path hash, line
number, category) **and** independently checks out that exact immutable
historical commit and validates its full literal line. Unexpected
findings, a different historical line, any additional finding, or incomplete
PR scan still produce **FAIL**. A reviewed synthetic false positive is
reported accurately, not mislabelled as zero raw matches. No scan rules
have been disabled or broadened.

## Exact original Masstagger command on real embedded-CUE MP3

A second distinct *real* Masstagger action now targets a freshly generated
two-track embedded-Cue MP3 in the same isolated Windows host alongside
Metadata Isolation. The physical file is passed to the original documented
Masstagger command-line interface, asking to set a synthetic COMMENT.
An independent oracle requires the physical COMM postimage to contain
that value and **every other ID3 frame (including opaque PRIV and full
TXXX CUESHEET raw text)** and full MPEG audio to match preimage exactly.
If the action is rejected, prompts for an unsupported workflow, or
rewrites the virtual Cue or unrelated physical frame, **the test fails**.
A positive physical MP3 outcome without a Cue alone cannot qualify this
more demanding scenario. No private user presets or authorization are inferred.
