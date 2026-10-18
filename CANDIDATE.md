# Manual candidate

Dispatch `Qualified manual candidate` explicitly after FAST succeeds.
Use the exact generated Win32/x64 component bytes and SHA-256 for real foobar2000
runtime acceptance. Do not publish a candidate automatically.

## Visibility and licensing boundary

**A GitHub Actions artifact on this public repository is publicly obtainable.**
Manual dispatch does not make its downloadable candidate private, and an Actions
artifact must not be used as a workaround for a blocked public binary release.

The candidate workflow must only package files whose distribution and required
attribution are permitted (including the foobar2000 SDK and WTL components).
Never include personal media, user databases, local settings, authentication
material, private research or third-party binaries without clearance. This
public candidate is intended solely for qualified functional testing, **not**
as an endorsed release or proof of production fitness.

If a candidate contains material that cannot lawfully be made public, do not
run the upload-producing workflow. A truly private hand-off requires a
private-access destination with adequate permissions; renaming or hiding a
public Actions artifact does not make it private.

Final user acceptance, provenance review and any applicable publication
rights are separate gates before promotion to a public release.
