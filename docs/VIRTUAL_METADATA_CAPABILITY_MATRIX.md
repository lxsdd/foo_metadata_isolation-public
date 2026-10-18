# Virtual metadata capability matrix

Stand: 2026-08-22

**Document status: CURRENT — authoritative `1.0.0-dev` Virtual capability boundary.** The Physical Metadata Audit extension does not broaden Virtual write capabilities; the previously qualified `1.0.0` RC remains historical evidence, not a public release.

The [README](../README.md) provides the user overview, [Architecture](ARCHITECTURE.md) defines the transaction contract, and qualification reports provide detailed evidence.

The matrix describes the exact foobar2000 metadata state after commit and reload. `MULTI_READ` therefore means that an Embedded Cue alone reopens as the same true `file_info` value vector; merely observing multiple values on a disposable Shadow does not qualify. `SAFE_REJECT` is a supported safety outcome, not a write capability.

## v1.0 CURRENT boundary summary

### SUPPORTED FOR v1.0

- qualified single-value `TITLE`, `ARTIST`, `SONGWRITER`, `DATE`, `COMMENT` and `ISRC` track operations;
- qualified single-value `ALBUM`, `ALBUM ARTIST`, `GENRE`, `DISCNUMBER` and `TOTALDISCS` document/global operations;
- proven global inheritance, track override and delete-to-inheritance cases under complete postimage validation;
- dedicated four-component Virtual ReplayGain state;
- constrained structural TRACK renumbering and the dedicated INDEX/PREGAP timing transaction described below.

Every supported normal field has ADD/CHANGE/DELETE capability only where the detailed scope rules produce one unambiguous zero-or-one-value Cue postimage. Partial and multi-source sessions use the same per-document impact, stale, postimage and rollback gates.

### PRESERVE / READ ONLY / DEDICATED EDITOR

- unknown raw Cue directives and comments are token-preserved but are not inferred to be metadata fields;
- TRACK type, FILE and INDEX 100+ are preserve-only;
- track count/order and document-authoring operations remain with the dedicated Cue editor;
- native FLAC type-5 CUESHEET state is read/preserve-only by the Phase-13 `NO-GO FOR v1.0` decision.

### UNSUPPORTED BY FORMAT / HOST / SEMANTIC BOUNDARY

- every true multivalue transition is blocking; `STYLE` alone is a proven independently preservable `KNOWN_SKIPPABLE` change under the default policy;
- `YEAR` is ambiguous against qualified `DATE` semantics;
- `COMPOSER`, credits, label/publisher/catalog/barcode, key/BPM/lyrics, Discogs, MusicBrainz and arbitrary custom fields have no proven lossless Cue field/scope mapping and remain blocking;
- arbitrary `REM <NAME>` is not a generic custom-metadata carrier: x86/x64 Cue-only replay did not expose `REM MI_CUSTOM` as the corresponding `file_info` field;
- structural ADD/DELETE/reorder/count, POSTGAP and multi-FILE operations remain blocking or outside the generic writer model.

No unlisted field is inferred from its name or silently converted into a private REM convention. The classifier's default is `UNKNOWN -> BLOCKING`.

## Virtual carrier boundary

The field, ReplayGain and structural matrices below apply only after the physical carrier itself passes its storage gate.

| FORMAT / MECHANISM | HOST SUBSONGS | RAW CARRIER | VIRTUAL WRITE | PRESERVATION | v1.0 STATUS |
|---|---:|---|---|---|---|
| MP3 Embedded Cue | PASS | one qualified ID3v2 `TXX/TXXX:CUESHEET` | PASS within capability matrices | non-target frames/tails/audio exact | `FULLY SUPPORTED` qualified slice |
| FLAC textual Cue | PASS | one Vorbis Comment `CUESHEET`, no Type-5 competitor | PASS within capability matrices | non-target blocks/audio exact | `VIRTUAL TEXTUAL-CUE SUPPORTED` qualified slice |
| WavPack textual Cue | PASS x86/x64 | one writable NUL-free UTF-8 `CUESHEET` text item in a trailing APEv2.0 tag | PASS within capability matrices | WavPack payload and all non-target APE items exact | `FULLY SUPPORTED` qualified slice |
| FLAC native Type-5 | PASS | binary Type-5 block | SAFE_REJECT | byte-exact preserve | `READ/PRESERVE ONLY` |
| Ogg Vorbis `CUESHEET` comment | NO (one physical subsong; zero chapters) | ordinary Vorbis Comment | BLOCKING | Physical writer preserves value | `PHYSICAL ONLY` |
| Opus `CUESHEET` comment | NO (one physical subsong; zero chapters) | ordinary OpusTags comment | BLOCKING | Physical writer preserves value | `PHYSICAL ONLY` |
| Monkey's Audio / Musepack textual Cue candidates | NOT QUALIFIED | possible APEv2 | BLOCKING | no v1 writer claim | `DEFERRED POST-v1.0` |
| MP4 chapters | host chapter mechanism, not Cue | MP4 atoms | BLOCKING | no private writer | `READ/PRESERVE / DEFERRED POST-v1.0` |
| Matroska chapters | complex chapter model, not Cue | EBML | BLOCKING | no private writer | `READ/PRESERVE / DEFERRED POST-v1.0` |

An unqualified carrier is never `KNOWN_SKIPPABLE`; it blocks the complete Virtual session. WavPack support does not generalize by APEv2 tag family name: the `.wv` boundary and exact trailing-tag validation are both required.

| FIELD | SCOPE | SINGLE | MULTI_READ | MULTI_ADD | MULTI_CHANGE | MULTI_REMOVE_ONE | MULTI_DELETE_ALL | ORDER | DUPLICATES | INHERITANCE | ROUNDTRIP | STATUS |
|---|---|---:|---:|---:|---:|---:|---:|---|---|---|---:|---|
| `TITLE` | track | PASS | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | lossy | lossy | n/a | NO | single-value ADD/CHANGE/DELETE only |
| `ARTIST` / Cue `PERFORMER` | track/global | PASS | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | lossy | lossy | single-value PASS | NO | single-value ADD/CHANGE/DELETE only |
| `SONGWRITER` | track/global | PASS | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | lossy | lossy | single-value PASS | NO | single-value ADD/CHANGE/DELETE only |
| `DATE` | track/global `REM DATE` | PASS | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | lossy | lossy | single-value PASS | NO | single-value ADD/CHANGE/DELETE only |
| `COMMENT` | track/global `REM COMMENT` | PASS | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | lossy | lossy | single-value PASS | NO | single-value ADD/CHANGE/DELETE only |
| `ISRC` | track | PASS | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | lossy | lossy | n/a | NO | single-value ADD/CHANGE/DELETE only |
| `ALBUM` | global `TITLE` | PASS | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | lossy | lossy | full-document only | NO | single-value ADD/CHANGE/DELETE only |
| `ALBUM ARTIST` | global `PERFORMER` | PASS | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | lossy | lossy | scope-dependent | NO | single-value ADD/CHANGE/DELETE only |
| `GENRE` | global `REM GENRE` | PASS | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | last directive wins | duplicates collapse | full-document only | NO | single-value ADD/CHANGE/DELETE only |
| `DISCNUMBER` | global `REM DISCNUMBER` | PASS | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | lossy | lossy | full-document only | NO | single-value ADD/CHANGE/DELETE only |
| `TOTALDISCS` | global `REM TOTALDISCS` | PASS | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | lossy | lossy | full-document only | NO | single-value ADD/CHANGE/DELETE only |
| `STYLE` | no proven Cue mapping | NO | NO | KNOWN_SKIP / strict reject | KNOWN_SKIP / strict reject | KNOWN_SKIP / strict reject | KNOWN_SKIP / strict reject | exact original vector retained | exact original vector retained | no Cue scope | NO | only qualified known-skippable field; never written |
| `YEAR` | ambiguous with qualified `DATE` | NO | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | not representable | not representable | ambiguous | NO | semantic boundary / blocking |
| `COMPOSER`, credits, label/publisher/catalog/barcode, BPM/key/lyrics, Discogs, MusicBrainz and custom fields | no proven Cue mapping | NO | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | not representable | not representable | unknown | NO | unsupported / blocking |
| arbitrary `REM <NAME>` as custom metadata | raw preserve only; not exposed generically | NO | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | repeated directives not a vector | duplicates not exposed losslessly | unknown | NO | format/semantic boundary / blocking |
| `TRACKNUMBER`, totals, `INDEX`, `FILE`, order/count | Cue structure | separate structural model | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | structural | structural | structural | NO | see structural matrix below |

No field qualified as true multivalue-capable or multivalue read-only. A vector on the projected Shadow is nevertheless retained exactly for classification and preview; it is never converted to a separator string.

## Dedicated ReplayGain capability matrix

ReplayGain is not part of the normal metadata-field matrix. It is compared as the exact four-float `file_info::get_replaygain()` state, with the SDK missing sentinels kept distinct from numeric zero.

| STATE | PHYSICAL_READ | PHYSICAL_WRITE | VIRTUAL_READ | VIRTUAL_ADD | VIRTUAL_CHANGE | VIRTUAL_DELETE | TRACK_SCOPE | ALBUM_SCOPE | ROUNDTRIP | STATUS |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| Track Gain | PASS | PASS | PASS | PASS | PASS | PASS | per-subsong or Cue-global inherited | n/a | exact float bits after native text formatting | supported |
| Track Peak | PASS | PASS | PASS | PASS | PASS | PASS | per-subsong or Cue-global inherited | n/a | exact float bits after native text formatting | supported |
| Album Gain | PASS | PASS | PASS | PASS | PASS | PASS | track override permitted | Cue-global inherited | exact float bits after native text formatting | supported |
| Album Peak | PASS | PASS | PASS | PASS | PASS | PASS | track override permitted | Cue-global inherited | exact float bits after native text formatting | supported |

Physical ID3 ReplayGain is not inherited into virtual subsongs. Embedded-Cue `REM REPLAYGAIN_*` directives form an independent virtual state. All four components may occur globally or in a track block; a track directive overrides the global value for that component. Partial records are valid, and missing is not represented as zero.

For a changed component the planner derives the desired state of every selected and unselected subsong. One common value is represented globally; differing or partially missing states are represented with explicit track directives after removing the global directive. The complete set of relevant spans is patched atomically and reopened before commit. Native gain formatting uses two decimals plus `dB`; peak uses six decimals. If formatting and parsing do not reproduce the exact Shadow float, the entire session is rejected rather than rounded by Metadata Isolation.

## Dedicated structural capability matrix

Structure is not treated as normal metadata. `TRACKNUMBER` is the only projected-shadow structural intent; timing uses a separate validated raw-Cue transaction and UI.

| STRUCTURE | READ | EXPOSED BY FOOBAR | GENERIC DIFF | ADD | CHANGE | DELETE | IMPACT MODEL | ROUNDTRIP | STATUS |
|---|---|---|---|---|---|---|---|---|---|
| TRACK number | full | subsong ID + synthesized `TRACKNUMBER` | YES | SAFE_REJECT | constrained PASS | SAFE_REJECT | complete document + exact playlist-handle remap | exact | `SAFE_CHANGE_ONLY` |
| TRACK type | raw only | discarded by embedded record | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | track | lossy | `PRESERVE_ONLY` |
| INDEX 00 | raw provenance + technical | `pregap` duration | NO | PASS | PASS | PASS | selected track pregap state | exact 75 Hz | `FULL / STRUCTURAL_TRANSACTION_ONLY` |
| INDEX 01 | raw + technical | start/duration | NO | SAFE_REJECT | PASS | SAFE_REJECT | selected + previous duration, complete document | exact 75 Hz | `SAFE_CHANGE_ONLY / STRUCTURAL_TRANSACTION_ONLY` |
| INDEX 02..99 | raw + technical | relative `index NN` | NO | constrained PASS | PASS | constrained PASS | selected track, complete index sequence | exact 75 Hz | `FULL / STRUCTURAL_TRANSACTION_ONLY` |
| INDEX 100+ | raw only | not losslessly exposed | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | unknown | NO | `PRESERVE_ONLY` |
| PREGAP | raw provenance + technical | `pregap` duration | NO | PASS | PASS | PASS | selected track pregap state | exact 75 Hz | `FULL / STRUCTURAL_TRANSACTION_ONLY` |
| POSTGAP | parser reject | NOT_EXPOSED | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | unknown | NO | `UNSUPPORTED` |
| FILE | raw partial | one physical decoder; no routing | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | document | FILE type lost by rebuild | `DEDICATED_EDITOR_ONLY / PRESERVE_ONLY` |
| track order | raw block order | not a metadata diff | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | complete document + explicit identity/playlist policy | representable but not inferable | `DEDICATED_EDITOR_ONLY / UNSUPPORTED_GENERIC` |
| track count | chapter inventory | not a metadata diff | NO | SAFE_REJECT | n/a | SAFE_REJECT | complete document + new/removed identity and playlist policy | representable but not inferable | `DEDICATED_EDITOR_ONLY / UNSUPPORTED_GENERIC` |
| multi-FILE | raw partial | one embedded physical source | NO | SAFE_REJECT | SAFE_REJECT | SAFE_REJECT | document/filesystem | NO | `UNSUPPORTED` |

A supported TRACK change requires one canonical decimal Shadow value in `1..999`, a preimage matching the original subsong ID, and unique strictly increasing numbers in unchanged raw block order for the complete document. Only existing decimal number tokens are replaced. ADD/DELETE, alphanumeric positions, conflicts and reordering reject atomically. Partial playlists are updated by exact old-handle to new-handle replacement; locked affected playlists cause a byte-exact file rollback. See the [structural qualification report](reports/generated/structural-cue-qualification-20260810.md).

A supported timing change preserves raw provenance: INDEX 00 is never inferred from PREGAP or vice versa. Optional directives are inserted at a deterministic track-local anchor, changed only in their time token, or removed as exactly one line. Additional indices must remain contiguous from 02 and strictly increasing between INDEX 01 and the following track. The staged file supplies the exact expected technical state and durations for full post-commit validation. See the [timing qualification report](reports/generated/structural-timing-qualification-20260814.md).

Raw track addition/removal can reopen as a different public inventory, but no SDK metadata command expresses the required new/removed block identity or playlist policy. Nonmonotone raw order is unreadable; duplicate TRACK keys collapse to the later block and can leave `TOTALTRACKS` inconsistent with the visible count. A coherently renumbered/re-timed permutation reopens, but its postimage alone cannot distinguish move from replacement. Metadata Isolation therefore never infers Count/Order intent. See the [track-count/order qualification](reports/generated/track-count-order-qualification-20260814.md).

## CUESHEET frame encoding matrix

| ID3 TEXT ENCODING | FOOBAR EXPOSURE | READ | WRITE | ORIGINAL ENCODING | ROUNDTRIP | STATUS |
|---|---|---:|---:|---:|---:|---|
| 0 | Windows-1252, including C1 mappings | PASS | representable values PASS | preserved | exact | supported / non-representable `SAFE_REJECT` |
| 1 | UTF-16 with per-string BOM | PASS | PASS | BOM + LE/BE preserved | exact | supported |
| 2 | UTF-16BE without BOM | PASS | PASS | preserved | exact | supported for ID3v2.4 |
| 3 | UTF-8 | PASS | PASS | preserved | exact | supported for ID3v2.4 |
| malformed/ambiguous text | not trusted | SAFE_REJECT | SAFE_REJECT | unchanged | no lossy decode | unsupported |

Value equality is exact UTF-8 after foobar decoding: case, NFC/NFD form, combining marks, non-BMP scalars, leading/trailing spaces and punctuation remain significant. The patcher never normalizes text, substitutes replacement characters or upgrades the source frame encoding. The TXXX description bytes, terminal NUL convention and UTF-16 byte order are retained. Quotes/newlines cannot be represented by the qualified Cue-field token patch and reject atomically. See the [encoding qualification report](reports/generated/cuesheet-encoding-qualification-20260814.md).

### External Cue decoding boundary

External `.cue` files are decoded by foobar2000 before Metadata Isolation receives virtual metadata. This is distinct from the embedded ID3 CUESHEET-frame storage matrix above.

| EXTERNAL SOURCE | FOOBAR 2.26 PREVIEW x86/x64 | METADATA ISOLATION STATUS |
|---|---|---|
| UTF-8 with BOM | exact Unicode | `SUPPORTED` host input |
| UTF-8 without BOM | exact Unicode | `SUPPORTED` host input |
| Windows-1251 | interpreted as Windows-1252; mojibake | `HOST-LIMITED` |
| CP932 / Shift-JIS family | interpreted as Windows-1252; mojibake | `HOST-LIMITED` |

Metadata Isolation neither guesses the source codepage nor re-decodes host strings. Once a Cue is correctly exposed as Unicode and embedded in a qualified carrier, exact NFC/NFD form, combining sequences, bidi code points, CJK and non-BMP values follow the normal capability and raw-preservation rules. See the [Phase 10 report](reports/generated/internationalization-unicode-cuesheet-encoding-20260822.md).

## ID3 tag-structure capability matrix

| TAG STRUCTURE | FOOBAR EXPOSURE | CUESHEET PATCH | PRESERVATION | STATUS |
|---|---|---|---|---|
| plain ID3v2.2 `TXX` | PASS | ADD/CHANGE/DELETE through Cue transaction | original v2.2 frame model and tail retained | supported |
| plain ID3v2.3/v2.4 `TXXX` | PASS | ADD/CHANGE/DELETE through Cue transaction | exact non-target frames and tail | supported |
| v2.3 extended header, no flags/CRC | PASS | padding declaration validated and updated | extended header retained | supported |
| v2.4 extended header, no flags | PASS | header retained | exact | supported |
| v2.4 footer | PASS | header/footer sizes updated together | footer retained; padding rejected | supported |
| tag-level unsynchronisation | may expose virtual tracks | SAFE_REJECT | source byte-identical | unsupported |
| ID3v2.2 tag compression | no usable virtual inventory in tested runtime | SAFE_REJECT | source byte-identical | unsupported |
| CRC/restriction/update extended header | may expose virtual tracks | SAFE_REJECT | source byte-identical | unsupported |
| transformed `TXX`/`TXXX` candidate | may expose virtual tracks | SAFE_REJECT | source byte-identical | unsupported/ambiguous |
| transformed non-target frame | PASS | target-only patch | frame retained byte-identically | supported if no tag-alter-discard flag |
| target status flag | version-dependent exposure | SAFE_REJECT | source byte-identical | unsupported |
| non-target read-only frame | PASS | target-only patch | frame retained byte-identically | supported |
| any frame marked discard-on-tag-alter | PASS | SAFE_REJECT | source byte-identical | unsupported |

Reserved flags, malformed sizes, mismatched footer/header state and ambiguous CUESHEET candidates also reject before installation. A supported and unsupported document in one session remains an atomic whole-session reject. See the [ID3 transformation qualification](reports/generated/id3-transform-qualification-20260814.md).

## Empirical multivalue boundary

- The 344-case, 43-field writer/reopen lab found no Cue-only true vector roundtrip. Six native fields serialized a vector into one comma-joined Cue value; 37 fields did not represent the requested vector in Cue. A joined string is one value and is not preservation.
- Raw repeated directives also do not form vectors. On x86 and x64, repeated track `TITLE`, `SONGWRITER`, `REM COMMENT` and global `REM GENRE` exposed one last value; track `PERFORMER` exposed one first value; duplicate `REM GENRE` collapsed; arbitrary repeated `REM MI_CUSTOM` was not exposed.
- Reversing repeated directives changed which single value survived. Ordering is therefore lossy rather than a supported reorder operation. Duplicate values are not preserved.
- The ordinary foobar ID3 writer discarded an empty second vector member before Shadow reopen. This is recorded as foobar normalization, never promoted to an Embedded-Cue capability.
- Exact vectors containing Unicode, punctuation, quotes, leading/trailing spaces, 10 values and 100 values were observed and previewed without custom normalization. `['House; Techno', 'Ambient']` remained exactly two Shadow values; no separator heuristic exists.

## Planner and scope rules

No true vector transition is writable. `STYLE` alone is classified `KNOWN_SKIPPABLE`: under the default policy its exact original vector remains unchanged while independent committable changes may proceed; strict all-or-nothing rejects the same session. Every other true vector transition—including missing→multi, single→multi, multi→multi, multi→single, remove one, delete all, reorder and duplicate creation—is `BLOCKING` and rejects the complete session. Unknown/custom fields are never inferred to be skippable.

Single-value operations retain the already-qualified rules:

- track insert/replace/remove is limited to the native mappings above;
- global insert/replace/remove requires selection of every virtual subsong, one identical zero-or-one-value postimage and an unambiguous field-specific inheritance state;
- removing an override is allowed only when the inherited effective postimage exactly equals the Shadow postimage;
- accepted postimages are built from Shadow targets for committable diffs and exact preimages for known skips;
- all selected and unselected affected subsongs are validated from the staged source before commit;
- duplicate/malformed directives, explicit empty values, structural changes and unresolved inheritance reject atomically.

Stable reject codes include `UNSUPPORTED_FIELD`, `AMBIGUOUS_SCOPE`, `INHERITANCE_CONFLICT`, `PARTIAL_GLOBAL_SELECTION`, `STRUCTURAL_CHANGE`, `MULTIVALUE_UNSUPPORTED`, `EMPTY_VALUE_UNSAFE`, `DELETE_SEMANTICS_UNSAFE`, `MALFORMED_CUE` and `POSTIMAGE_MISMATCH`.

Accepted MP3 commits replace only the qualified ID3v2.2 `TXX:CUESHEET` or ID3v2.3/v2.4 `TXXX:CUESHEET` payload. Unknown Cue text, ordering, unrelated whitespace and structural directives, `FILE` type, artwork, physical ReplayGain, unrelated Cue ReplayGain components, opaque frames, APEv2, ID3v1 and MPEG payload remain protected by the existing staged-postimage, stale-fingerprint and recovery-copy transaction.

## FLAC textual-CUESHEET storage slice

| Storage/state | Read | Virtual operation | Roundtrip | Status |
|---|---:|---|---:|---|
| one text `CUESHEET` Vorbis Comment; no native block | PASS | track `TITLE`, `ARTIST`, `SONGWRITER`, `DATE`, `COMMENT`, `ISRC` ADD/CHANGE/DELETE | PASS x86/x64 | supported |
| same storage, complete document | PASS | global `ALBUM`, `ALBUM ARTIST`, `GENRE`, `DISCNUMBER`, `TOTALDISCS` ADD/CHANGE/DELETE | PASS x86/x64 | supported with normal scope rules |
| same storage, inherited text state | PASS | global change and explicit override → inherited value | PASS x86/x64 | supported when complete postimage is unambiguous |
| same storage, partial selected tracks | PASS | track-local metadata and ReplayGain | PASS x86/x64 | supported with full impact validation |
| same storage | PASS | Track/Album Gain/Peak ADD/CHANGE/DELETE, global/local reshaping | exact numeric PASS x86/x64 | supported |
| global metadata change on partial selection | PASS | any | SAFE_REJECT x86/x64 | `PARTIAL_GLOBAL_SELECTION` |
| remove all local PERFORMER overrides while global PERFORMER exists | PASS | coupled ARTIST/ALBUM ARTIST scope change | SAFE_REJECT x86/x64 | `POSTIMAGE_MISMATCH` |
| supported metadata or ReplayGain plus unknown custom field | PASS | mixed | source byte-identical | atomic reject |
| true multivalue `STYLE` diff | observed exactly on Shadow | any | KNOWN_SKIP / strict reject | original vector retained; no Cue write |
| other true multivalue diff | observed exactly on Shadow | any | SAFE_REJECT | blocking unchanged multivalue boundary |
| same storage | synthesized subsong identity | canonical `TRACKNUMBER` CHANGE | exact PASS x86/x64 | constrained renumbering; ADD/DELETE/conflict/reorder reject |
| same storage | raw provenance + technical state | INDEX 00, INDEX 02..99, PREGAP ADD/CHANGE/DELETE; INDEX 01 CHANGE | exact 75 Hz PASS x86/x64 | dedicated structural transaction |
| more than one text CUESHEET value | ambiguous | any | SAFE_REJECT | unsupported |
| more than one Vorbis Comment metadata block | carrier-dependent | any | SAFE_REJECT | unsupported |
| native FLAC CUESHEET block beside text CUESHEET | text Cue foobar-visible; native block raw-detected | any | SAFE_REJECT | preserve/read-only; dual-carrier merge is undefined |
| native-only FLAC CUESHEET block | binary structure exposed as subsongs; normal metadata inherited from physical Vorbis Comments | metadata or ReplayGain write | SAFE_REJECT | no independent virtual metadata/RG carrier |
| native-only FLAC CUESHEET block | public subsong ID and sample-offset structure | any structural mutation | static binary representation PASS; no complete generic intent or safe transaction | `READ_ONLY`; `NO-GO FOR v1.0` |
| native-only FLAC CUESHEET block | public chapterizer | TITLE edit | destructive boundary observed x64 | unsupported: adds text CUESHEET and mutates physical metadata/layout |
| NUL-tailed text CUESHEET | foobar exposes three tracks | any | SAFE_REJECT x86/x64 | backend rejects non-NUL-free UTF-8 |
| trailing Comment byte, after-Cue second Comment, corrupt block length | Core rejects storage | any | no transaction starts | foobar input boundary |

The FLAC patch changes only the exact text-CUESHEET field bytes and the enclosing field/block length values. STREAMINFO, PICTURE, PADDING, every physical metadata/ReplayGain Vorbis Comment, all unrelated Cue spans and the complete audio-frame tail remain byte-identical. Track/global metadata, ReplayGain, constrained TRACK renumbering and exact INDEX/PREGAP timing passed partial selection, mixed MP3+FLAC preparation, full postimage validation, stale rejection and injected rollback on both architectures. Playlist subset/order/duplicates/focus/selection survived renumbering; a locked playlist rolled the file back and the original FLAC inventory reopened. The existing numeric ReplayGain precision gate and exact value-vector multivalue rejection apply unchanged across MP3 and FLAC.

See the [FLAC structural qualification](reports/generated/flac-structural-cuesheet-qualification-20260815.md), the [native FLAC CUESHEET research](reports/generated/flac-native-cuesheet-research-20260815.md) and the [Phase-13 go/no-go decision](reports/generated/native-flac-type5-go-no-go-20260822.md). Native type-5 editing is explicitly out of v1 scope and fail-closed even when foobar exposes playable virtual tracks. Multiple Vorbis Comment blocks and multiple textual CUESHEET entries remain unsupported.
