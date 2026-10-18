# Architecture

**Document status: CURRENT — technical architecture and authoritative safety contract for the reopened `1.0.0-dev` scope.**

User workflows are described in the [README](../README.md). Exact capabilities belong to the [Virtual](VIRTUAL_METADATA_CAPABILITY_MATRIX.md) and [Physical](PHYSICAL_METADATA_CAPABILITY_MATRIX.md) matrices; qualification reports are evidence rather than competing architecture documents.

## Layer model

Metadata Isolation treats one selected item as two possible targets:

```text
selected metadb handles
        |
        +-- physical layer -> canonical local source -> deduplicated subsong 0 handles
        |
        +-- virtual layer  -> isolated physical projections -> capability-resolved raw-Cue transaction
                            -> dedicated editor for free-form/unsupported structure
```

The component routes existing foobar2000 commands. Its virtual transaction owns strict token-preserving ID3v2, textual-FLAC and WavPack/APEv2 CUESHEET patch backends; it does not replace foobar2000's general tagger or file-operation engines.

## Physical target mapper

`physical_targets.cpp` is the single mapping implementation used by Properties, generic context routing, file operations and test probes.

For every input handle it:

1. asks foobar2000 to canonicalize the source path;
2. resolves the responsible filesystem service;
3. rejects remote or non-native targets;
4. compares canonical identities through `filesystem::g_compare_paths`, with `metadb::path_compare` fallback;
5. creates exactly one `metadb_handle` with subsong 0 per physical source.

Any rejected item makes the complete operation fail before a command starts. This avoids silently applying a batch to only part of the user's selection.

## Physical command routing

`context_menu.cpp` loads physical metadata asynchronously before presenting Properties or the routed command tree. Hydration is required because newly created subsong-0 handles may otherwise have empty cached metadata; File Operations title formatting would then derive incorrect paths.

The generic path calls `contextmenu_manager::init_context_ex()` with the physical handle list and `caller_undefined`. Its menu tree is therefore bound to those handles. A thread-local scope hides Metadata Isolation's own entries while the nested tree is being built and prevents recursive menus.

This proves correct routing only for SDK-conforming `contextmenu_item` implementations. The public SDK cannot force a third-party command to use `p_data`; mainmenu commands receive no handle list, and shortcut execution resolves the global selection again. Compatibility is a runtime property, not a registry/name property.

## Physical Metadata Audit

The audit is a separate read-only consumer of the existing Physical mapping boundary. Library or selection handles are reduced to canonical local physical sources, then the qualified MP3/FLAC/WavPack textual-Cue carriers are reopened through `input_info_reader` at subsong 0. The audit path contains no metadata writer, raw-Cue writer, commit, copy or remove operation.

Unlike a mutating Physical command, a Library inventory may safely report and skip a non-local or unmappable item while continuing to inspect other local sources because no partial write can occur. Mutating Physical commands retain the all-or-nothing mapping gate.

The audited physical core is intentionally small and objective: an artist group is present when either `ALBUM ARTIST` or `ARTIST` is non-empty, and `ALBUM`, `DATE` and `GENRE` are required separately. `TITLE` and `TRACKNUMBER` are not source-file requirements because one Continuous-Mix source represents multiple Cue tracks.

When the physical core is complete, the audit can additionally compare physical Album, Album Artist/Artist, Date and Genre with exactly one global Cue TITLE, PERFORMER, REM DATE and REM GENRE respectively. It performs no case folding, separator splitting, Unicode normalization or inferred mapping. Ambiguous/multivalue/malformed comparisons are classified `REVIEW`, not corrected. Exact differences are `MISMATCH`.

The modeless result view uses the centralized native-Windows UX contract: background scanning, same-window progress/cancel/result state, stable sortable multi-selection, copy support, persistent filter/sort/columns/geometry, dark mode, DPI handling and full-row information tips. The explicit **Edit physical metadata...** action leaves the audit path and opens foobar2000 Physical Properties for the chosen physical rows.

## Virtual layer boundary

The dedicated `Edit cuesheet` command is located dynamically in the context tree for one representative original virtual handle. The physical mapper is used only to prove that the selection belongs to one source document. Passing subsong 0 to this command would make it unavailable and is deliberately avoided.

The public Cue wrapper cannot be used as the original commit path: it reconstructs the whole `file_info`, is not token preserving and loses information such as unknown directives and original FILE type. Metadata Isolation therefore runs compatible commands on disposable physical shadows, resolves the observed diff through mechanism capabilities and commits only a validated raw token plan. Host chapter mechanisms remain separate from textual-Cue carriers and never acquire Cue semantics by inference.

The current v1 development implementation uses an explicit capability resolver and a raw Cue directive patcher. It distinguishes raw directives from effective inherited metadata and can insert a supported directive at a deterministic anchor, replace only its value span, or remove exactly its line. The fully staged source must reopen to the accepted complete state for every selected and unselected subsong. Unknown, dependent, ambiguous, malformed, stale or postimage-mismatching plans reject the entire session.

`STYLE` is currently the only known-skippable metadata field because real Discogger/Masstagger vectors and the independent Cue labs prove both its exact Shadow state and the absence of a lossless Embedded-Cue representation. No arbitrary custom or MusicBrainz field inherits that classification. The default preference applies the independently committable subset and reports STYLE skips; strict all-or-nothing rejects the same session. Skips are never written, and the accepted subset remains one atomic multi-document transaction with the existing recovery and rollback contract.

One `input_info_reader` is retained while the complete initial virtual inventory of a Cue document is loaded. The selected Shadow projection then uses that cached `file_info`, and full staged validation reuses one reader across all affected and unaffected subsongs. This removes repeated full-Cue reconstruction without reducing the document-wide postimage.

Projection uses one disposable ordinary backing-file copy per source document, not per selected track. External commands receive native handles for the real subsong numbers of that shared copy. The write guard recognizes only the exact registered temporary path, captures each SDK writer's complete `file_info` postimage and suppresses destructive Cue serialization on the shared copy. Analysis consumes those captured value vectors and ReplayGain states. A writer that bypasses the filter can still mutate only the disposable copy; the session rereads it as a conservative fallback and retains all capability, complete-postimage and atomic-reject gates. Registration is removed before temporary cleanup, and original source paths are never registered or exposed to the routed command.

For the 111-track qualification fixture, the former per-track design needed about five seconds and 445.4/1333.4 MiB of temporary copies for 4/12-MiB sources. The current shared-source design used one 4.01/12.01-MiB backing file and completed the measured internal session in 0.327–0.397 seconds. Detailed benchmark methodology remains in the [Phase-9 report](reports/generated/projection-performance-ux-hardening-20260822.md).

The portable byte-copy backend remains the universal strategy: one source-sized copy is required per document so plugins continue to see a real local file with valid tags, subsongs, decoder and audio semantics. Hardlinks or symlinks would not isolate writes; metadata-only dummy files do not preserve the external-command contract; block cloning is not required and was unavailable on the tested NTFS volume. Parallelizing the former amplification was therefore deliberately avoided.

The public `contextmenu_node::execute()` and `contextmenu_item::context_command()` contracts return no completion object. Returning from an arbitrary command therefore cannot prove that a modeless or queued writer has finished. The normal UX uses foobar2000's non-blocking, dark-mode-aware query API: after the routed menu returns, the user finishes the external editor and explicitly chooses OK to apply safely or Cancel. On OK, Metadata Isolation acquires ordered write locks for every Shadow before reading its postimage, then analyzes, validates and commits without exposing the internal pending/review state. Legacy review/discard GUID actions remain hidden for diagnostics and recovery only.

Raw installation is selected by a storage backend. MP3 uses the strict ID3v2 CUESHEET-frame patcher, so unrelated APEv2/ID3v1 containers never pass through foobar's general metadata writer. The FLAC backend accepts only one Vorbis Comment block containing exactly one NUL-free UTF-8 `CUESHEET` entry and no native FLAC CUESHEET block. WavPack accepts only a local `.wv` with one trailing APEv2.0 extent and exactly one writable, NUL-free UTF-8 text item named `CUESHEET`; unknown flags, reserved bytes, duplicates, binary/external/read-only targets and oversized tags reject before staging. The adapter changes only the target item bytes, its little-endian length and matching header/footer tag sizes; the full WavPack payload and every non-target APE item byte remain exact. The common capability policy admits independently qualified single-value metadata, ReplayGain, constrained TRACK-renumbering and INDEX/PREGAP timing plans. Known-skippable STYLE state never enters a storage patch; every blocking diff still rejects atomically.

Native FLAC Type-5, MP4 Chapters and Matroska Chapters/Editions are not textual-Cue carrier variants. foobar2000 can expose playable subsongs from a native Type-5 block, and qualified Physical operations preserve that block, but the public SDK provides no complete lossless Virtual write/postimage model. Metadata Isolation therefore has no productive Type-5 writer in v1.0 and rejects Virtual mutation. MP4 and Matroska chapter mechanisms are deferred post-v1.0 rather than forced through the Cue transaction.

## Virtual safety contract

This is the authoritative diff policy:

```text
COMMITTABLE
→ use the requested target state

KNOWN_SKIPPABLE
→ retain the exact original state and report the skip

BLOCKING
→ reject the complete session
```

`UNKNOWN` is never skippable. The accepted postimage contains the requested state for every committable capability, the original state for qualified skips and the original state for everything unrelated. All documents are planned and staged before installation. The transaction then rechecks source fingerprints, installs in deterministic order, compares installed files with their validated staging artifacts, reopens semantic state and rolls every installed document back if installation or postvalidation fails.

The raw frame patch keeps the original ID3v2 text-encoding byte, TXXX description bytes, value terminator and UTF-16 byte order. foobar2000's empirical interpretation of ID3 encoding 0 is represented by a fixed Windows-1252 table rather than the process locale; all 32 C1 byte positions have a reversible mapping. Encodings 1, 2 and 3 remain UTF-16-with-BOM, UTF-16BE and UTF-8 respectively. Invalid sequences, missing/ambiguous BOMs, embedded NULs and values not exactly representable in the original encoding reject before installation. No Unicode normalization, replacement-character decoding or implicit encoding upgrade is performed.

External `.cue` source decoding belongs to foobar2000. Metadata Isolation receives the host's decoded Unicode state and does not run a second codepage detector. Empirically, foobar2000 2.26 Preview decodes external UTF-8 with and without BOM exactly, while Windows-1251 and CP932 inputs are interpreted as Windows-1252. Those legacy cases are host-input limits. The embedded transaction remains independent: it compares exact Unicode code-point sequences and preserves the qualified source carrier encoding, BOM/byte order and unrelated raw Cue representation.

The frame scanner has an explicit version/structure model rather than treating every ID3 tag as a v2.3/v2.4 frame stream. ID3v2.2 uses its three-byte `TXX` identifier, six-byte frame header and 24-bit frame size; v2.3 and v2.4 retain their respective integer and synchsafe sizes. A minimal flag-free extended header is retained for v2.3/v2.4, the v2.3 declared padding count is validated and updated, and a matching v2.4 footer is retained. Tag-level unsynchronisation, v2.2 compression, CRC/restriction/update extended headers, transformed `TXX`/`TXXX` candidates and status-flagged target frames reject atomically. Transformed or read-only non-target frames are never decoded or rewritten and may be retained byte-for-byte, except that any frame marked for discard on tag alteration makes the whole patch unsafe.

ReplayGain bypasses the normal metadata-field classifier. A dedicated diff records exact presence and float values for the four SDK components. The planner resolves each component across the complete Cue document: one common state uses the global `REM REPLAYGAIN_*` directive, while differing or partially missing states use track directives. Gain and peak are serialized only with the SDK's native formatting, then parsed back and compared exactly before a patch is accepted. Physical ReplayGain remains part of the immutable physical postimage during a virtual transaction.

Timing structure bypasses projected shadows entirely. A dedicated dialog reads the raw Cue provenance for one stable subsong identity and produces an exact 75-Hz INDEX/PREGAP edit. The token-preserving timing patcher validates the complete document, stages the patched MP3, qualified textual-CUESHEET FLAC or qualified WavPack/APEv2 carrier, reopens every virtual subsong and records exact expected technical info and durations. Commit then reuses the same full-file stale fingerprint, multi-document backup/rollback and postimage machinery as metadata and ReplayGain. INDEX 01 is mandatory/change-only; optional INDEX 00, PREGAP and contiguous INDEX 02..99 support insert/replace/remove.

Track count and block order are intentionally not inferred from projected Shadows or timing edits. The embedded wrapper keys public subsongs by numeric TRACK and silently replaces an earlier duplicate key with the later raw block, while a nonmonotone raw block sequence can make the complete Cue unreadable. A coherent reordered Cue is representable, but its final metadata/timing inventory cannot prove whether the user intended a permutation, replacement or new identity. Adding/removing tracks likewise needs explicit new/removed identity and playlist policy. The generic and timing transactions therefore require unchanged count/order; the existing dedicated foobar2000 Cue editor remains the only v1 path for these document-authoring operations.

## Write guard

`write_guard.cpp` registers an opt-in `input_info_filter`. With the local policy enabled, it suppresses metadata writes for `.mp3`, `.flac` and `.wv` locations whose subsong is not zero. It does not transform metadata and has no restoration phase.

The component cannot query foobar2000's global Track Info Filters checkbox. It therefore reports:

- `ACTIVE` only after an actual filter callback was observed;
- `INACTIVE` when the local policy is off;
- `UNKNOWN` when the local policy is on but no callback has yet proved global activation.

`on_info_remove()` has no veto return value, so explicit tag removal is outside the guard's protection contract.

## Logging

`operation_log.cpp` emits concise Standard events by default and adds technical phase timings and diagnostics in Verbose mode. Both modes use operation identifiers, counts, classifications and durations without logging paths, metadata values, hashes, credentials or secrets. This privacy boundary applies only to Metadata Isolation output, not to foobar2000 or third-party logs.

## Test-only diagnostics

`diagnostics.cpp` is compiled only when `EnableTestProbes=true`. Probes exercise destructive writer paths exclusively on generated fixtures inside isolated portable runtimes. The standard Release project omits the translation unit entirely.

The independent Python dumper parses ID3v2, trailing APEv2 and ID3v1 extents separately and hashes the MPEG payload between them. This prevents APE tags from being mistaken for audio and permits preservation assertions for binary and multi-value APE items.

Historical writer-flow research remains in [Embedded-cuesheet metadata analysis](embedded-cuesheet-metadata-analysis.md). Current release-gate state is tracked in [v1 development status](V1_DEVELOPMENT_STATUS.md).
