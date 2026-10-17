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

Exit 0 = all green, exit 1 = red (blocks the PR).
Usage: python3 Tools/CI/check_client_gates.py [--root <repo root>]
"""
import re
import sys
from pathlib import Path

FAIL_BUDGET = 700
WARN_BUDGET = 500

PROD_INCLUDE = re.compile(r'#include\s+"Utils/(NetCore/|JSONParser\.h)')
PROD_CALL = re.compile(r"\b([A-Z]\w*)::(\w+)")
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

    if failures:
        print("CLIENT GATES: FAIL")
        for failure in failures:
            print(f"  - {failure}")
        return 1
    print(f"CLIENT GATES: OK ({len(cpp_files)} .cpp checked)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
