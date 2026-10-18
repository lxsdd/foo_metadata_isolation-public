# Physical metadata capability matrix

Stand: 2026-08-22

**Document status: CURRENT — authoritative `1.0.0-dev` Physical capability boundary while the Physical Metadata Audit extension is being requalified.** The previously qualified `1.0.0` RC remains historical evidence, not a public release.

The [README](../README.md) provides the user overview, [Architecture](ARCHITECTURE.md) defines routing ownership, and qualification reports provide detailed evidence.

Metadata Isolation owns source mapping, subsong-0 projection, deduplication and command routing. The selected foobar2000 input writer or third-party command owns tag serialization. `SUPPORTED` below therefore means that the Metadata Isolation route is safe and the qualified host writer produced the stated semantic postimage; it does not promise a private serializer or a cross-writer byte layout.

| Container / mechanism | Routing | ADD | CHANGE | DELETE | True vector | Custom / MusicBrainz | Unicode | Artwork | Embedded Cue | ReplayGain | Multi-file | Status / boundary |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| MP3 / ID3v2 | subsong 0, deduplicated | PASS | PASS | PASS | order, duplicates and Unicode PASS; empty member removed by host | PASS as ordinary fields | PASS, including Unicode path | exact APIC preservation for text/RG operations | exact raw `TXXX:CUESHEET` preservation | four physical components ADD/CHANGE/DELETE PASS | PASS, including mixed FLAC | `SUPPORTED THROUGH HOST WRITER`; current runtime emits ID3v2.4 |
| MP3 / trailing ID3v1 | same physical source | not independently selected | not independently selected | not independently selected | n/a | n/a | format-limited | n/a | ID3v2 Cue unchanged | coexistence PASS | PASS | existing ID3v1 remained byte-identical; independent update/removal policy is `HOST-CONFIG-DEPENDENT` / `HOST-LIMITED` |
| MP3 / trailing APEv2 | same physical source | not independently selected | not independently selected | not independently selected | existing binary/vector items preserved exactly | existing items preserved exactly | existing items preserved exactly | n/a | ID3v2 Cue unchanged | coexistence PASS | PASS | existing APEv2 remained byte-identical; independent APEv2 writer selection is `HOST-CONFIG-DEPENDENT` |
| MP3 / ID3v2 + ID3v1 | one source | through ID3v2 | through ID3v2 | through ID3v2 | through ID3v2 | through ID3v2 | through ID3v2 | PASS | PASS | PASS | PASS | `SUPPORTED`; ID3v1 `PRESERVE ONLY` in qualified path |
| MP3 / ID3v2 + APEv2 | one source | through ID3v2 | through ID3v2 | through ID3v2 | through ID3v2 | through ID3v2 | through ID3v2 | PASS | PASS | PASS | PASS | `SUPPORTED`; APEv2 `PRESERVE ONLY` in qualified path |
| MP3 / ID3v2 + APEv2 + ID3v1 | one source | through ID3v2 | through ID3v2 | through ID3v2 | through ID3v2 | through ID3v2 | through ID3v2 | PASS | PASS | PASS | PASS | `SUPPORTED`; both trailing containers preserved byte-identically |
| FLAC / Vorbis Comments | subsong 0, deduplicated | PASS | PASS | PASS | order, duplicates and Unicode PASS; empty member removed by host | PASS as ordinary comments | PASS, including Unicode path | exact PICTURE preservation | textual `CUESHEET` exact semantic/raw value preservation | four physical components ADD/CHANGE/DELETE PASS | PASS, including mixed MP3 | `SUPPORTED THROUGH HOST WRITER` |
| FLAC / native type-5 CUESHEET present | same physical source | ordinary comments only | ordinary comments only | ordinary comments only | through Vorbis Comments | through Vorbis Comments | PASS | PASS | textual Cue and type-5 block exact | physical RG PASS | PASS | physical metadata `SUPPORTED`; Type-5 block `PRESERVE ONLY`; native editing is `NO-GO FOR v1.0` |
| WavPack / APEv2 | subsong 0, deduplicated | PASS | PASS | PASS | order, duplicates and Unicode PASS; empty member omitted by reloaded `file_info` | PASS as ordinary APE text items | PASS | binary front cover exact for text/RG operations | raw Cue value exact; host may normalize APE key casing | four physical components ADD/CHANGE/DELETE PASS | PASS | `SUPPORTED THROUGH HOST WRITER` x86/x64 |
| Ogg Vorbis / Vorbis Comments | subsong 0, deduplicated | PASS | PASS | PASS | order, duplicates and Unicode PASS; empty member omitted by reloaded `file_info` | PASS | PASS | `METADATA_BLOCK_PICTURE` exact | `CUESHEET` comment exact but not virtualized | four physical components ADD/CHANGE/DELETE PASS | PASS | `SUPPORTED THROUGH HOST WRITER` x86/x64; non-comment Ogg packets exact |
| Opus / OpusTags | subsong 0, deduplicated | PASS | PASS | PASS | order, duplicates and Unicode PASS; empty member omitted by reloaded `file_info` | PASS | PASS | `METADATA_BLOCK_PICTURE` exact | `CUESHEET` comment exact but not virtualized | PASS through host `replaygain_info`; host uses `R128_*` plus OpusHead output gain | PASS | `SUPPORTED THROUGH HOST WRITER` x86/x64; audio packets exact |
| Monkey's Audio / APEv2 | generic mapper available | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | host input support only | NOT TESTED | carrier not qualified | NOT TESTED | NOT TESTED | `DEFERRED POST-v1.0`; no v1 preservation oracle or Virtual carrier |
| Musepack / APEv2 | generic mapper available | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | host input support only | NOT TESTED | carrier not qualified | NOT TESTED | NOT TESTED | `DEFERRED POST-v1.0`; no result inferred from WavPack |
| MP4 / M4A | generic mapper available | NOT TESTED | NOT TESTED | NOT TESTED | host-specific | host-specific | host input support only | NOT TESTED | not a textual-Cue carrier | NOT TESTED | NOT TESTED | `DEFERRED POST-v1.0`; no private atom writer |
| Matroska / WebM | generic mapper available | NOT TESTED | NOT TESTED | NOT TESTED | host-specific | host-specific | host input support only | NOT TESTED | not a textual-Cue carrier | NOT TESTED | NOT TESTED | `DEFERRED POST-v1.0`; no private EBML writer |

## Physical Metadata Audit

The current v1 development scope adds a read-only audit on top of the same canonical source mapper. It is limited to the already qualified textual-Cue carriers MP3, FLAC and WavPack.

For each deduplicated physical source with exactly one textual `CUESHEET`, the audit reads subsong 0 directly and evaluates:

- artist group: at least one non-empty `ALBUM ARTIST` or `ARTIST`;
- `ALBUM`;
- `DATE`;
- `GENRE`.

All four missing produces `EMPTY`; a proper subset missing produces `INCOMPLETE`. Complete physical core tags can be compared exactly with unique global Cue Album/Performer/Date/Genre state; exact differences produce `MISMATCH`, while multivalue, ambiguous, malformed or unreadable cases produce `REVIEW`. `TITLE` and `TRACKNUMBER` are intentionally not completeness requirements.

The scan itself is no-write. Opening normal Physical Properties from a selected result row is an explicit subsequent user action, not part of audit classification.

## Physical routing contract

```text
selected physical and/or virtual handles
→ canonical local physical source
→ one subsong-0 handle per unique source
→ intended host command once per source
```

The Phase-12 mixed corpus exposed 14 command-line handles and 48 expanded physical/virtual handles across six MP3/FLAC sources. Both Win32 and x64 mapped them to exactly six targets, rejected none, and wrote every source once. A non-local or otherwise unmappable member makes the complete mapping fail before any delegated command starts.

## Qualified semantic operations

- Standard fields: missing `DISCNUMBER` was added, `TITLE` changed and physical `COMMENT` deleted.
- Custom fields: one missing field was added, one changed and a true-vector field deleted.
- MusicBrainz: `MUSICBRAINZ_RELEASEID` roundtripped as ordinary physical custom metadata; no plugin-specific product rule exists.
- True vectors: `['α', 'β', 'α', '😀', '']` reopened as `['α', 'β', 'α', '😀']` on every qualified carrier. WavPack/Ogg/Opus store the trailing empty raw member, but foobar omits it from reloaded `file_info`; Metadata Isolation records the semantic host postimage and performs no separator conversion.
- ReplayGain: Track Gain, Track Peak, Album Gain and Album Peak were added/changed and then independently removed on all six sources.

## Preservation and host boundaries

For the targeted operations, independent raw-container oracles prove unchanged MPEG/FLAC/WavPack payload, non-comment Vorbis packets, Opus audio packets, MP3 APEv2 and ID3v1 tails, artwork, private/binary items, unrelated fields, raw embedded Cue values, FLAC STREAMINFO and native Type-5 state. The Opus host writer deliberately changes only the OpusHead output-gain bytes and `R128_*`/ReplayGain fields when the requested numeric `replaygain_info` changes. Tag containers may otherwise be reserialized or key-normalized by the host; only data with an explicit raw-preservation contract is claimed byte-identical.

Physical command completion, file-level atomicity and rollback are owned by the invoked host or third-party writer. Metadata Isolation guarantees complete pre-command mapping and does not manufacture the Virtual transaction's rollback semantics for Physical commands. Explicit Physical `Remove tags from file(s)` and destructive third-party options remain intentional host operations; Discogger preservation requires `Remove any pre-existing tags` to stay disabled.
