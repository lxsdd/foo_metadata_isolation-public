"""Independent fail-closed oracle for the real foobar physical-target mapper.

The actual C++ map_to_physical_targets() runs in a disposable Win32/x64
foobar2000 host. This verifier only reads its report and synthetic fixture,
never writes music files or fabricates a mapped target.
"""
import argparse
import hashlib
from pathlib import Path


CASES = {
    "single": {"extra": 0, "target": 1, "rejected": 0, "complete": "true"},
    "duplicate": {"extra": 1, "target": 1, "rejected": 0, "complete": "true"},
    "case-alias": {"extra": 1, "target": 1, "rejected": 0, "complete": "true"},
    "unicode-case-alias": {"extra": 1, "target": 1, "rejected": 0, "complete": "true"},
    "hardlink-alias": {"extra": 1, "target": 1, "rejected": 0, "complete": "true"},
    "all-subsongs": {"extra": 0, "target": 1, "rejected": 0, "complete": "true"},
    "remote-rejected": {"extra": 1, "target": 1, "rejected": 1, "complete": "false"},
    "remote-first-rejected": {"extra": 0, "target": 1, "rejected": 1, "complete": "false"},
    "many-distinct": {"extra": 0, "target": 111, "rejected": 0, "complete": "true"},
}


def require(condition: bool, label: str) -> None:
    if not condition:
        raise AssertionError(label)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def assert_file_identity_collision_policy() -> None:
    """Static negative guard alongside the real Windows hardlink runtime oracle.

    A native file-ID collision cannot be reliably generated on hosted NTFS;
    this is not a claim of a ReFS collision runtime test.
    """
    source = (Path(__file__).resolve().parents[1] /
              "src/foo_metadata_isolation/physical_targets.cpp").read_text(encoding="utf-8")
    begin = "const auto known_identity = seen_physical_file_ids.find(file_id);"
    end = "// Add the handle before marking source as mapped."
    equivalence = "if (!same_physical_file(native, known_identity->second)) {"

    def guarded(candidate: str) -> bool:
        if candidate.count(begin) != 1 or candidate.count(end) != 1:
            return False
        region = candidate.split(begin, 1)[1].split(end, 1)[0]
        return (region.count("if (known_identity != seen_physical_file_ids.end()) {") == 1
                and region.count(equivalence) == 1
                and region.count("reject_target(") == 1
                and region.index(equivalence) < region.index("reject_target(")
                # The rejection must be reachable directly from the mismatch
                # branch. Mere presence of the reject_target token is not proof.
                and region.split(equivalence, 1)[1].lstrip().startswith("reject_target(")
                and "continue;" in region)

    require(guarded(source), "Duplicate reported file ID lacks a fail-closed equivalence gate")
    require(not guarded(source.replace(equivalence, "if (false) {", 1)),
            "Mutation control failed: equivalence gate removal went undetected")
    first, rest = source.split(begin, 1)
    region, last = rest.split(end, 1)
    missing_reject = first + begin + region.replace("reject_target(", "ignore_target(", 1) + end + last
    require(not guarded(missing_reject),
            "Mutation control failed: missing collision rejection went undetected")

    # A previously undetected, compilable C++ mutation placed the rejection
    # inside an always-false nested branch. The old token-only test went green.
    direct_reject = equivalence + "\n                    reject_target("
    require(source.count(direct_reject) == 1,
            "Collision mismatch path lacks its direct rejection")
    unreachable_reject = source.replace(
        direct_reject,
        equivalence + "\n                    if (false) {\n                        reject_target(",
        1)
    rejection_end = '"Matching file ID could not be verified as the same physical file.");'
    require(unreachable_reject.count(rejection_end) == 1,
            "Collision rejection endpoint unexpectedly changed")
    unreachable_reject = unreachable_reject.replace(
        rejection_end, rejection_end + "\n                    }", 1)
    require(not guarded(unreachable_reject),
            "Mutation control failed: unreachable collision rejection went undetected")


def assert_path_alias_collision_policy() -> None:
    """Reject ambiguous comparator/Unicode aliases rather than silently skip them.

    Real Windows runtime tests exercise *valid* case aliases and hardlinks.
    These negative mutations independently guard the two rejection branches
    for different physical files that cannot be manufactured reliably in CI.
    This is static reachability evidence, not a case-sensitive-volume test.
    """
    source = (Path(__file__).resolve().parents[1] /
              "src/foo_metadata_isolation/physical_targets.cpp").read_text(encoding="utf-8")
    canonical_start = "const auto original = unique_physical_sources.find(key);"
    canonical_end = "const std::wstring unicode_native = unicode_path_from_utf8(native);"
    case_start = "const auto collision = canonical_case_lookup.find(unicode_native);"
    case_end = "// Distinct filenames and hardlinks can refer to one physical file,"
    canonical_outer = "if (*original != key) {\n                    const auto previously_mapped"
    canonical_reject = (
        "if (previously_mapped == canonical_native_lookup.end() ||\n"
        "                        !same_physical_file(native, previously_mapped->second)) {\n"
        "                        reject_target(")
    case_outer = (
        "if (collision != canonical_case_lookup.end()) {\n"
        "                if (!same_physical_file")
    case_reject = (
        "if (!same_physical_file(native, collision->second)) {\n"
        "                    reject_target(")

    def guarded(candidate: str) -> bool:
        anchors = (canonical_start, canonical_end, case_start, case_end)
        if any(candidate.count(anchor) != 1 for anchor in anchors):
            return False
        canonical = candidate.split(canonical_start, 1)[1].split(canonical_end, 1)[0]
        # The canonical-alias and byte-identical-repeat branches have separate
        # rejection gates. Count only the alias branch here; the second is
        # independently mutation-tested below.
        canonical_alias = canonical.split("} else {", 1)[0]
        case = candidate.split(case_start, 1)[1].split(case_end, 1)[0]
        return (
            canonical_alias.count(canonical_outer) == 1
            and canonical_alias.count(canonical_reject) == 1
            and canonical_alias.count("reject_target(") == 1
            and canonical.count("continue;") == 1
            and "Equivalent source path could not be verified as the same physical file." in canonical
            and case.count(case_outer) == 1
            and case.count(case_reject) == 1
            and case.count("reject_target(") == 1
            and case.count("continue;") == 1
            and "Case-only source alias could not be verified as the same physical file." in case
        )

    require(guarded(source), "Physical path aliases lack both direct fail-closed rejection gates")
    mutations = (
        ("canonical comparator outer branch", canonical_outer,
         canonical_outer.replace("if (*original != key)", "if (false)")),
        ("canonical comparator equivalence", canonical_reject,
         canonical_reject.replace("!same_physical_file(native, previously_mapped->second)", "false")),
        ("canonical comparator rejection", canonical_reject,
         canonical_reject.replace("reject_target(", "ignore_target(")),
        ("canonical unreachable nested rejection", canonical_reject,
         canonical_reject.replace("reject_target(", "if (false) { reject_target(")),
        ("Unicode case collision outer branch", case_outer,
         case_outer.replace("collision != canonical_case_lookup.end()", "false")),
        ("Unicode case collision equivalence", case_reject,
         case_reject.replace("!same_physical_file(native, collision->second)", "false")),
        ("Unicode case collision rejection", case_reject,
         case_reject.replace("reject_target(", "ignore_target(")),
        ("Unicode case unreachable nested rejection", case_reject,
         case_reject.replace("reject_target(", "if (false) { reject_target(")),
    )
    for label, original, mutation in mutations:
        require(source.count(original) == 1, f"{label}: source anchor not unique")
        require(not guarded(source.replace(original, mutation, 1)),
                f"{label}: dangerous mutation was not detected")


def assert_repeated_canonical_path_identity_policy() -> None:
    """A repeated path must not hide a retargeted file during selection.

    The positive same-file case is exercised by the genuine Win32/x64
    duplicate-selection host oracle. Repointing the file *during* mapping
    is not deterministic on CI, so the negative branch is mutation-tested.
    """
    source = (Path(__file__).resolve().parents[1] /
              "src/foo_metadata_isolation/physical_targets.cpp").read_text(encoding="utf-8")
    start = "const auto original = unique_physical_sources.find(key);"
    end = "const std::wstring unicode_native = unicode_path_from_utf8(native);"
    storage = "canonical_file_id_lookup.emplace(key, file_id);"
    direct_reject = (
        'if (previous_id == canonical_file_id_lookup.end() ||\n'
        '                        !(current_duplicate_id == previous_id->second)) {\n'
        '                        reject_target('
    )
    # The exact source is a safety contract: a second identical canonical
    # spelling must be checked against the *first* file identity, not just
    # filesystem::equivalent(path, path), which always checks the latest file.
    def guarded(candidate: str) -> bool:
        if any(candidate.count(anchor) != 1 for anchor in (start, end, storage)):
            return False
        region = candidate.split(start, 1)[1].split(end, 1)[0]
        if region.count("} else {") != 1:
            return False
        repeat = region.split("} else {", 1)[1]
        return (
            "canonical_file_id_lookup.find(*original)" in repeat
            and "read_physical_file_identity(unicode_path_from_utf8(native))" in repeat
            and repeat.count(direct_reject) == 1
            and repeat.count("reject_target(") == 1
            and "Repeated canonical path no longer identifies the original physical file." in repeat
            and region.count("continue;") == 1
            and candidate.index(storage) > candidate.index("result.handles.add_item(")
        )

    require(guarded(source), "Repeated canonical path lacks a physical-identity recheck")
    mutations = (
        ("identity comparison removed",
         "!(current_duplicate_id == previous_id->second)",
         "false"),
        ("read-only identity probe removed",
         "read_physical_file_identity(unicode_path_from_utf8(native))",
         "physical_file_identity{}"),
        ("identity recording removed",
         storage,
         "canonical_file_id_lookup.clear();"),
        ("identity rejection removed",
         direct_reject,
         direct_reject.replace("reject_target(", "ignore_target(")),
        ("identity rejection made unreachable",
         direct_reject,
         direct_reject.replace("reject_target(", "if (false) { reject_target(")),
    )
    for label, original, mutated in mutations:
        require(source.count(original) == 1, f"{label}: source anchor not unique")
        require(not guarded(source.replace(original, mutated, 1)),
                f"{label}: dangerous mutation was not detected")


def main() -> None:
    assert_file_identity_collision_policy()
    assert_path_alias_collision_policy()
    assert_repeated_canonical_path_identity_policy()
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--scenario", choices=sorted(CASES), required=True)
    options = parser.parse_args()
    report = options.report
    fixture = options.fixture
    baseline = options.baseline
    require(fixture.name == "track-01.mp3", "Unexpected media fixture")
    require(baseline.name == "track-01.mp3.before", "Unexpected baseline")
    require(report.name == fixture.name + ".metadata-isolation-probe.txt", "Unexpected report")
    require(report.parent == fixture.parent == baseline.parent, "Cross-directory report refused")
    require(digest(fixture) == digest(baseline), "Read-only physical mapping mutated MP3")

    data = {}
    for line in report.read_text(encoding="utf-8-sig").splitlines():
        if not line:
            continue
        require("=" in line, "Malformed mapping report")
        key, value = line.split("=", 1)
        require(key not in data, "Duplicate mapping evidence")
        data[key] = value

    expected = CASES[options.scenario]
    # foobar's real command-line parser may expand a Cue file argument into
    # multiple virtual handles. Derive the expected input cardinality from
    # that independently emitted launcher count; never assume it is one.
    source_handles = int(data.get("handle_count", "0"))
    require(1 <= source_handles <= 3, "Unexpected original Cue launcher handle count")
    expected_inputs = (224 if options.scenario == "many-distinct"
                       else 2 if options.scenario == "remote-first-rejected"
                       else source_handles * 3 if options.scenario == "all-subsongs"
                       else source_handles + expected["extra"])
    for key, value in (
        ("mapping_input_count", expected_inputs),
        ("mapping_target_count", expected["target"]),
        ("mapping_rejected_count", expected["rejected"]),
        ("mapping_complete", expected["complete"]),
        ("mapping_target_0_subsong", "0"),
        ("write_result", "map_only"),
    ):
        require(data.get(key) == str(value), f"{options.scenario}: wrong {key}")
    if options.scenario == "case-alias":
        alias = data.get("mapping_alias_path", "")
        require(alias.endswith("/TRACK-01.MP3") or alias.endswith("\\TRACK-01.MP3"),
                "The genuine distinct-case alias was not exercised")
    elif options.scenario == "unicode-case-alias":
        alias = data.get("mapping_alias_path", "").replace("\\", "/")
        require(alias.endswith("/ähre/track-01.mp3"),
                "Actual UTF-8 case-variant Windows source path was not exercised")
        require(fixture.parent.name == "Ähre", "Unexpected Unicode source fixture directory")
    elif options.scenario == "hardlink-alias":
        alias = data.get("mapping_alias_path", "").replace("\\", "/")
        require(alias.endswith("/track-01-hardlink.mp3"),
                "Actual hardlink target path was not exercised")
        link = fixture.parent / "track-01-hardlink.mp3"
        require(link.is_file() and link.samefile(fixture),
                "Windows hardlink is not genuinely the same physical file")
        require(digest(link) == digest(baseline),
                "Read-only mapping unexpectedly mutated the hardlink target")
    else:
        require("mapping_alias_path" not in data, "Unexpected alias evidence")
    if options.scenario == "many-distinct":
        # The real SDK mapper returns the 111 unique source handles in the
        # selection's original descending order, despite using a sorted set
        # internally for de-duplication.
        require(len(list(fixture.parent.glob("track-*.mp3"))) == 111,
                "Expected exactly 111 distinct disposable MP3 carriers")
        baseline_digest = digest(baseline)
        for index in range(111):
            number = 111 - index
            leaf = f"track-{number:02d}.mp3"
            mapped = data.get(f"mapping_target_{index}_path", "")
            require(mapped.lower().endswith(leaf), f"Physical selection order changed at {index}")
            require(data.get(f"mapping_target_{index}_subsong") == "0",
                    f"Virtual subsong leaked into physical target {index}")
            require(digest(fixture.parent / leaf) == baseline_digest,
                    f"Synthetic source {number} was altered")
        require("mapping_target_111_path" not in data, "Additional duplicate source leaked")
    else:
        path = data.get("mapping_target_0_path", "")
        require(path.lower().endswith("track-01.mp3"), "Unexpected physical target path")
        require("mapping_target_1_path" not in data, "Duplicated physical target")

    if options.scenario in ("remote-rejected", "remote-first-rejected"):
        rejected = data.get("mapping_first_rejected_path", "")
        require(rejected.startswith("http://127.0.0.1/metadata-isolation-unsupported.mp3"),
                "Missing known unsupported remote selection")
        require(bool(data.get("mapping_first_rejected_reason")), "Missing rejection reason")
    else:
        require("mapping_first_rejected_path" not in data, "Unexpected rejected source")

    print("REAL_HOST_PHYSICAL_TARGET_MAPPING_" +
          options.scenario.upper().replace("-", "_") + "=PASS")


if __name__ == "__main__":
    main()
