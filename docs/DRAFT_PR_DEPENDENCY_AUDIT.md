# Draft PR dependency and release-hold audit — 21 October 2026

This is a source-only, point-in-time integration inventory. It does **not** approve a PR merge, force push, deletion, release, package upload, author license, or live media write.

## Actual checked refs

- `main`: `4d395db7390cac799d7dbaa7377c62f807aeef93` (no merge).
- Recovery/host-smoke starting point: [Draft #6](https://github.com/lxsdd/foo_metadata_isolation-public/pull/6) `9d53c545ba964defd3ce7896d5a371f48584f807`, ancestor of the latest #26 head.
- [Draft #7](https://github.com/lxsdd/foo_metadata_isolation-public/pull/7) `6702f3288cfc793d2d0789e89857ff17ad4a66ed`: *diverged* from #26; do **not** interpret its entire branch as merged. The specifically reviewed artifact hold source `scripts/verify-public-artifact-hold.py` was copied to the product Draft; the candidate artifact upload was made permanently disabled pending authorization.
- [Draft #8](https://github.com/lxsdd/foo_metadata_isolation-public/pull/8) `64cbf1cdd3a11eb087ecd838da17248801f6955a`: *diverged*, intended hourly dispatcher proposal. This dispatcher is **not** incorporated; do not silently add its scheduling or permissions.
- [Drafts #9 through #26](https://github.com/lxsdd/foo_metadata_isolation-public/pulls): every consecutive GitHub head-to-head comparison at inspection time reported `ahead > 0, behind = 0`; these are one forward ancestry chain, with #26 incorporating #6 and subsequent product/test commits. Do **not** merge each stacked PR separately to main: that can duplicate work or apply unreviewed intermediate states.
- Checked #26 SHA at time of this audit: `1287e6be27dbb7fe380b7619bc91f5a8372a3a33`, all open Draft, all target `main`.

## Consolidation and acceptance policy

1. Preserve all Drafts and their immutable CI evidence; no automatic close, merge or force-push while user approval is absent.
2. Treat the **newest Draft #26 source** as the one integrated *candidate* branch, not as a release. Review its final diff against `main` once it qualifies.
3. Verify exact-SHA real Win32 and x64 foobar host tests after official vendor connectivity recovers; an older green run does not replace new HEAD tests. Native Commit is still **NOT PASS**.
4. Keep `LICENSE/PENDING OWNER DECISION`, authenticated real Discogger/Masstagger, user's large continuous Mix and UX/accessibility/end-user sign-off unresolved.
5. Source-only historical audit, no-distribution gates, generated synthetic fixtures, probe-free DLL checks and runner-only archive checks are prerequisites, **not sufficient release authorization**.

Evidence: [PR #26](https://github.com/lxsdd/foo_metadata_isolation-public/pull/26); [green real native Discard Win32/x64 at historical SHA](https://github.com/lxsdd/foo_metadata_isolation-public/actions/runs/39566942096); [current Fast Cloud source qualification](https://github.com/lxsdd/foo_metadata_isolation-public/actions/runs/39579136691). No private source configurations, binaries, or user media are recorded in this document.
