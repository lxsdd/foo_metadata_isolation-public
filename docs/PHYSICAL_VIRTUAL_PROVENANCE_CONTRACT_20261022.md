# Physical/virtual provenance contract — 2026-10-22

Status: architecture decision / source-only documentation; not a cross-project API change.

- A physical media file is identified by its canonical file path and **subsong 0** for a physical-write operation. Physical ID3/other container tags are not interchangeable with virtual cuesheet track metadata.
- An embedded text CUESHEET may yield virtual subsongs. Preserve their **(path, subsong)** identities and embedded CUE text; never silently rewrite physical tags from virtual Properties or infer a physical write from a virtual track selection.
- The existing Bridge / DJ Library snapshot identity **(path, subsong)** remains compatible. Virtual CUE tracks can appear there with explicit virtual/provenance labeling; this project does not introduce or require a new snapshot field.
- **rekordbox MyTagSync** must not match virtual **subsong != 0** entries to rekordbox ContentIDs. This is a consumer-side exclusion, not a reason to hide embedded-CUE tracks from foobar or the DJ Library. This component does not write rekordbox databases or My Tags.
- The original Masstagger must use an *explicit physical scope*, tested on disposable synthetic media in original foobar Win32/x64 hosts. A positive write requires the genuine original third-party UI and independent verification that physical COMMENT changed, while raw CUESHEET, opaque PRIV and MPEG audio remain unchanged. CLI no-op on virtual Cue selections is not a physical-write PASS.
- Discogger acceptance likewise requires genuinely authorized authenticated live use within its own credential boundary, no synthetic replacement for live Discogs and no cross-repository token copy.
- Changes to cross-project schemas/behavior belong in the respective projects; protect the isolation boundary here. The approved MIT + Commons Clause applies only to this project's own code, not vendor dependencies.

Release gates remain separate: full hosted accessibility, real configured third-party actions and cancel/restore, owner-specific UX/presets, distribution rights, and explicit owner authorization. No merge, tag, release, published component binary or productive media edits.
