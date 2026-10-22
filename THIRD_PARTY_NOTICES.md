# Third-party source and license notices

The foobar2000 SDK **2026-10-01** is centrally pinned to Git commit `31a35947a7527826c2199491692802e3ce3d8bd7` and SDK subtree `2ba8caeb73f72633ad7f60ee5f72ed8074a0eb17` in [sdk/SDK-PIN.json](sdk/SDK-PIN.json). All Windows builds stage this reviewed source before compilation; the old vendored SDK source and compiled .lib files are absent from the repository's current tree.

The SDK, PFC and libPPUI copyright/license notices remain at `sdk/sdk-license.txt`, `sdk/pfc/pfc-license.txt`, `sdk/libPPUI/libPPUI-license.txt`. WTL is still tracked with `wtl/MS-PL.txt`.

The upstream SDK .7z SHA in the pin is metadata; CI validates exact Git commit and source tree, **not** the downloaded original archive. **Project-owned original source and documentation** are separately licensed
under [LICENSE](LICENSE): **MIT plus "Commons Clause" v1.0**,
`Copyright (c) 2026 lxsdd`, `Licensor: lxsdd`. This restriction-bearing
combined license is **source-available**, not an OSI-approved open-source
license. The root LICENSE has **no relicensing effect** on independent SDK,
pfc, libPPUI, WTL or original foobar2000 player and installer material.
The SDK subtree is pinned independently; provenance and license notices do
not by themselves authorize a public component DLL, GitHub release,
distribution or third-party component test installation.
