# Third-party source and license notices

The foobar2000 SDK **2026-10-01** is centrally pinned to Git commit `31a35947a7527826c2199491692802e3ce3d8bd7` and SDK subtree `2ba8caeb73f72633ad7f60ee5f72ed8074a0eb17` in [sdk/SDK-PIN.json](sdk/SDK-PIN.json). All Windows builds stage this reviewed source before compilation; the old vendored SDK source and compiled .lib files are absent from the repository's current tree.

The SDK, PFC and libPPUI copyright/license notices remain at `sdk/sdk-license.txt`, `sdk/pfc/pfc-license.txt`, `sdk/libPPUI/libPPUI-license.txt`. WTL is still tracked with `wtl/MS-PL.txt`.

The upstream SDK .7z SHA in the pin is metadata; CI validates exact Git commit and source tree, **not** the downloaded original archive. No component-source license or distribution authorization is implied.
