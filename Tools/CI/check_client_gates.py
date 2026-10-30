#!/usr/bin/env python3
"""Static CI gates for the UE MMORPG client (roadmap P3).

Runs without Unreal or third-party deps (stdlib only) so it works on
GitHub-hosted runners:

1. Line budget: any .cpp under Source/Prototyping with >700 lines = FAIL,
   >500 lines = warning (new code should stay ~500, see AGENTS.md).
2. No misleading stubs: Private/Tests/NPCSystemTest.cpp must not exist as an
   empty (0-line) file.
3. Honest specs: every Private/Tests/*.cpp containing an
   IMPLEMENT_SIMPLE_AUTOMATION_TEST must call production code
   (JSONParser:: or F<Rule>::) instead of re-implementing logic locally.
4. Rule coverage: every struct F*Rule in Public/Utils/NetCore/ must be
   referenced by at least one Private/Tests/*.cpp (a new pure rule with
   no spec calling it fails the gate — the fromCharacterId echo bug
   shipped exactly through this hole).
5. Codebook coverage (S3a): every Get*DisplayName codebook getter must be
   called by at least one spec, and every Config/Localization/*.csv must
   parse with unique spaceless non-empty slugs.
6. No LogTemp (S01): no UE_LOG(LogTemp in Source/Prototyping — use the
   project categories from Source/Prototyping/Prototyping.h
   (LogConnection / LogNetPacket / LogPing / LogGameplay / LogGameAudio).
7. No world scans in hot paths (S02): no GetAllActorsOfClass /
   GetAllActorsWithTag calls under Private/Gameplay/{Players,Items,Mobs} —
   use the manager registries (MOBManager / NPCManager / ItemManager) instead.
# 8. UI-sound coverage: AudioUISoundTest must drive StaticEnum<EUISoundEvent>
#    (new events covered automatically), and unmapped PlayUISound must warn
#    (silence-by-default is banned).

Exit 0 = all green, exit 1 = red (blocks the PR).
Usage: python3 Tools/CI/check_client_gates.py [--root <repo root>]
"""
import re
import sys
from pathlib import Path

FAIL_BUDGET = 700
WARN_BUDGET = 500

PROD_INCLUDE = re.compile(r'#include\s+"(Utils/(NetCore/|JSONParser\.h)|Networking/NetworkDispatcher\.h|Services/LocalizationSubsystem\.h|Audio/AudioManager\.h)')
PROD_CALL = re.compile(r"\b([A-Z]\w*)::(\w+)")
RULE_STRUCT = re.compile(r"\bstruct\s+(?:\w+\s+)?(F\w*Rule\w*)\b")
CALL_BLOCKLIST_PREFIX = ("Test",)
CALL_BLOCKLIST_NAME = {"RunTest"}
SPEC_MACRO = "IMPLEMENT_SIMPLE_AUTOMATION_TEST"


def main() -> int:
    root = Path(sys.argv[sys.argv.index("--root") + 1]) if "--root" in sys.argv else Path(__file__).resolve().parents[2]
    src = root / "Source" / "Prototyping"
    tests_dir = src / "Private" / "Tests"
    failures: list[str] = []

    # 1. Line budget over all module .cpp files.
    cpp_files = sorted(src.rglob("*.cpp"))
    if not cpp_files:
        failures.append(f"no .cpp files found under {src}")
    for path in cpp_files:
        try:
            lines = sum(1 for _ in path.open(encoding="utf-8", errors="replace"))
        except OSError as exc:
            failures.append(f"cannot read {path}: {exc}")
            continue
        rel = path.relative_to(root).as_posix()
        if lines > FAIL_BUDGET:
            failures.append(f"line budget FAIL: {rel} has {lines} lines (> {FAIL_BUDGET}) — split into modules")
        elif lines > WARN_BUDGET:
            print(f"::warning file={rel}::line budget warning: {lines} lines (> {WARN_BUDGET}), consider splitting")

    # 2. Empty NPC stub must be gone (or filled with a real spec).
    stub = tests_dir / "NPCSystemTest.cpp"
    if stub.exists():
        try:
            text = stub.read_text(encoding="utf-8", errors="replace").strip()
        except OSError as exc:
            failures.append(f"cannot read {stub}: {exc}")
            text = "unreadable"
        if not text:
            failures.append(
                "misleading stub: Source/Prototyping/Private/Tests/NPCSystemTest.cpp "
                "is empty — delete it or add a real NPC spec"
            )

    # 3. Honest specs: each spec file must include a prod header and call
    # production code through it (aliases like `using R = FSampleRule` count
    # as `R::IsNearBest`; only *Test:: helpers are excluded).
    for path in sorted(tests_dir.glob("*.cpp")):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            failures.append(f"cannot read {path}: {exc}")
            continue
        if SPEC_MACRO not in text:
            continue
        if not PROD_INCLUDE.search(text):
            failures.append(
                f"dishonest spec: {path.name} defines {SPEC_MACRO} but includes "
                "no production header (Utils/NetCore/ or Utils/JSONParser.h)"
            )
            continue
        honest = any(
            not name.endswith("Test") and not name.startswith(CALL_BLOCKLIST_PREFIX) and call not in CALL_BLOCKLIST_NAME
            for name, call in PROD_CALL.findall(text)
        )
        if not honest:
            failures.append(
                f"dishonest spec: {path.name} defines {SPEC_MACRO} but never calls "
                "production code — specs must pin prod code, "
                "local re-implementations are reject-on-review"
            )

    # 4. Rule coverage: every pure rule struct in NetCore/ must be called
    # by at least one spec.
    netcore_dir = src / "Public" / "Utils" / "NetCore"
    test_texts: list[str] = []
    for spec in sorted(tests_dir.glob("*.cpp")):
        try:
            test_texts.append(spec.read_text(encoding="utf-8", errors="replace"))
        except OSError:
            pass
    if netcore_dir.is_dir():
        rules: set[str] = set()
        for header in sorted(netcore_dir.glob("*.h")):
            try:
                rules.update(RULE_STRUCT.findall(header.read_text(encoding="utf-8", errors="replace")))
            except OSError as exc:
                failures.append(f"cannot read {header}: {exc}")
        for rule in sorted(rules):
            if not any(rule in text for text in test_texts):
                failures.append(
                    f"uncovered rule: {rule} has no spec calling it — "
                    "new NetCore rules must ship with an honest MMO.* spec"
                )

    # 5. Codebook coverage (S3a): every Get*DisplayName codebook getter must
    # be called by at least one spec, and every Config/Localization/*.csv
    # must parse with unique non-empty slugs (+ name_ru column).
    for getter in ("GetAttributeDisplayName", "GetItemTypeDisplayName",
                   "GetRarityDisplayName", "GetEquipSlotDisplayName",
                   "GetItemSetDisplayName"):
        if not any(getter in text for text in test_texts):
            failures.append(
                f"uncovered getter: {getter} has no spec calling it"
            )
    loc_dir = root / "Config" / "Localization"
    if loc_dir.is_dir():
        import csv as _csv
        for csv_path in sorted(loc_dir.glob("*.csv")):
            try:
                with csv_path.open(encoding="utf-8-sig", newline="") as handle:
                    rows = list(_csv.DictReader(handle))
            except OSError as exc:
                failures.append(f"cannot read {csv_path.name}: {exc}")
                continue
            if not rows or "slug" not in (rows[0].keys() if rows else []):
                failures.append(f"codebook csv: {csv_path.name} has no slug column/rows")
                continue
            seen: set[str] = set()
            for row in rows:
                slug = (row.get("slug") or "").strip()
                if not slug:
                    failures.append(f"codebook csv: {csv_path.name} has an empty slug")
                    break
                if " " in slug:
                    failures.append(f"codebook csv: {csv_path.name} slug has spaces: {slug!r}")
                    break
                if slug in seen:
                    failures.append(f"codebook csv: {csv_path.name} duplicate slug: {slug!r}")
                    break
                seen.add(slug)

    # 6. No LogTemp (S01): every UE_LOG must use a project category.
    logtemp_re = re.compile(r"UE_LOG\s*\(\s*LogTemp\b")
    for path in sorted(src.rglob("*.cpp")) + sorted(src.rglob("*.h")):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            failures.append(f"cannot read {path}: {exc}")
            continue
        if logtemp_re.search(text):
            failures.append(
                f"LogTemp banned: {path.relative_to(root).as_posix()} uses "
                "UE_LOG(LogTemp — use LogConnection/LogNetPacket/LogPing/LogGameplay/LogGameAudio"
            )

    # 7. No world scans in hot paths (S02): GetAllActorsOfClass/WithTag are
    # banned under Private/Gameplay/{Players,Items,Mobs} — use the registries.
    scan_re = re.compile(r"GetAllActors(OfClass|WithTag)\s*\(")
    hot_dirs = [src / "Private" / "Gameplay" / d for d in ("Players", "Items", "Mobs")]
    for hot in hot_dirs:
        if not hot.is_dir():
            continue
        for path in sorted(hot.rglob("*.cpp")) + sorted(hot.rglob("*.h")):
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except OSError as exc:
                failures.append(f"cannot read {path}: {exc}")
                continue
            if scan_re.search(text):
                failures.append(
                    f"world scan banned: {path.relative_to(root).as_posix()} calls "
                    "GetAllActors* — use MOBManager/NPCManager/ItemManager registries (S02)"
                )

    # 8. UI-sound coverage: silence-by-default is banned.
    # (a) every EUISoundEvent value must be exercised by the router spec
    #     (AudioUISoundTest drives StaticEnum<EUISoundEvent>, future-proof);
    # (b) the unmapped path must warn (pinned string), so a missing
    #     DA_AudioConfig entry surfaces in logs instead of silent buttons.
    audio_mgr_h = src / "Public" / "Audio" / "AudioManager.h"
    try:
        mgr_text = audio_mgr_h.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        failures.append(f"cannot read {audio_mgr_h}: {exc}")
        mgr_text = ""
    enum_block = re.search(r"enum class EUISoundEvent[^{]*\{(.*?)\};", mgr_text, re.S)
    if not enum_block:
        failures.append("audio gate: EUISoundEvent enum not found in Audio/AudioManager.h")
    else:
        values = re.findall(r"^\s*(\w+)\s+UMETA", enum_block.group(1), re.M)
        if not values:
            failures.append("audio gate: no EUISoundEvent values parsed")
    ui_test = tests_dir / "AudioUISoundTest.cpp"
    try:
        ui_test_text = ui_test.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        failures.append(f"cannot read {ui_test}: {exc}")
        ui_test_text = ""
    if "StaticEnum<EUISoundEvent>" not in ui_test_text:
        failures.append(
            "audio gate: AudioUISoundTest must drive StaticEnum<EUISoundEvent> "
            "so new events are covered automatically"
        )
    music_cpp = src / "Private" / "Audio" / "AudioManager.Music.cpp"
    try:
        music_text = music_cpp.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        failures.append(f"cannot read {music_cpp}: {exc}")
        music_text = ""
    if "no UISound mapped for event" not in music_text:
        failures.append(
            "audio gate: PlayUISound must warn on unmapped events "
            "(silence-by-default is banned)"
        )

    if failures:
        print("CLIENT GATES: FAIL")
        for failure in failures:
            print(f"  - {failure}")
        return 1
    print(f"CLIENT GATES: OK ({len(cpp_files)} .cpp checked)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
