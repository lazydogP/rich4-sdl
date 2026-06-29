#!/usr/bin/env python3
"""Audit that every SDL1 production source has one build rule and link entry.

The current Makefile still carries explicit dependency lists while the SDL1
split is in progress.  This guard makes that transitional representation
auditable: a new platform source must be attached to exactly one object rule,
and that object must be reachable from the SDL link variables.  The rule is
deliberately source-driven, so an omitted object cannot hide behind a passing
compile of an older build tree.
"""

from __future__ import annotations

import re
import subprocess
import sys
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAKEFILE = ROOT / "Makefile"
MAKE_WRAPPER = ROOT / "scripts" / "make_minimal.py"
PLATFORM_ROOT = ROOT / "src" / "platform" / "sdl1"


def fail(message: str) -> None:
    print(f"sdl object manifest: {message}", file=sys.stderr)
    raise SystemExit(1)


def make_lines() -> list[str]:
    return MAKEFILE.read_text(encoding="utf-8").splitlines()


def object_tokens(lines: list[str], variable: str) -> set[str]:
    tokens: set[str] = set()
    pattern = re.compile(rf"^{re.escape(variable)}\s*(?:\+=|:=|=)\s*(.*)$")
    for line in lines:
        match = pattern.match(line)
        if match:
            tokens.update(re.findall(r"\$\(BUILD_DIR\)/([^\s,\)]+\.o)", match.group(1)))
    return tokens


def manifest_rules() -> dict[str, list[str]]:
    output = subprocess.check_output(
        [sys.executable, str(ROOT / "tests" / "architecture" /
                              "list_sdl1_sources.py"), "--pairs"],
        text=True,
    )
    rules: dict[str, list[str]] = {}
    for line in output.splitlines():
        if not line.strip():
            continue
        source, separator, target = line.partition("|")
        if separator != "|" or not source or not target:
            fail("invalid source/object pair: " + line)
        rules.setdefault(target, []).append(source)
    return rules


def profile_sources(profile: str) -> list[str]:
    output = subprocess.check_output(
        [sys.executable, str(ROOT / "tests" / "architecture" /
                              "list_sdl1_sources.py"), "--profile", profile],
        text=True,
    )
    return [line.strip() for line in output.splitlines() if line.strip()]


def verify_dry_run_compile_inventory(sources: list[str], profile: str,
                                     target: str = "sdl") -> None:
    output = subprocess.check_output(
        [sys.executable, str(MAKE_WRAPPER), "--raw-output", "-n", "-B", target],
        cwd=ROOT,
        text=True,
        stderr=subprocess.STDOUT,
    )
    compiled: Counter[str] = Counter()
    source_pattern = re.compile(r"(?:^|\s)(src/platform/sdl1/[^\s]+\.c)(?:\s|$)")
    for line in output.splitlines():
        if " -c " not in line:
            continue
        match = source_pattern.search(line)
        if match:
            compiled[match.group(1)] += 1
    audio_sources = {
        "src/platform/sdl1/audio/audio_sdl_mixer.c",
        "src/platform/sdl1/audio_stub.c",
    }
    expected_sources = set(profile_sources(profile))
    missing = sorted(
        source for source in expected_sources
        if source not in audio_sources and compiled[source] != 1
    )
    if missing:
        fail("dry-run compile inventory missing or duplicated source rules: " +
             ", ".join(missing))
    if sum(compiled[source] for source in audio_sources) != 1:
        fail("dry-run compile inventory did not select exactly one audio backend")
    unexpected = sorted(
        source for source in sources
        if source not in expected_sources and compiled[source] != 0
    )
    if unexpected:
        fail("dry-run compile inventory included an unselected profile source: " +
             ", ".join(unexpected))


def rule_sources(lines: list[str]) -> dict[str, list[str]]:
    rules: dict[str, list[str]] = {}
    pattern = re.compile(r"^\$\(BUILD_DIR\)/([^: ]+\.o):\s*(.*)$")
    for line in lines:
        match = pattern.match(line)
        if not match:
            continue
        sources = re.findall(r"(src/platform/sdl1/[^\s|]+\.c)", match.group(2))
        if sources:
            rules.setdefault(match.group(1), []).extend(sources)
    return rules


def recipe_sources(lines: list[str]) -> dict[str, list[str]]:
    recipes: dict[str, list[str]] = {}
    target: str | None = None
    pattern = re.compile(r"^\$\(BUILD_DIR\)/([^: ]+\.o):")
    for line in lines:
        match = pattern.match(line)
        if match:
            target = match.group(1)
            continue
        if target is not None and line.startswith("\t") and " -c " in line:
            sources = re.findall(r"(?:^|\s)(src/platform/sdl1/[^\s]+\.c)(?:\s|$)", line)
            if sources:
                recipes.setdefault(target, []).extend(sources)
            target = None
    return recipes


def main() -> None:
    lines = make_lines()
    makefile_text = "\n".join(lines)
    if "tests/architecture/list_sdl1_sources.py" not in makefile_text:
        fail("Makefile is missing the recursive SDL1 source inventory")
    uses_generated_rules = all(
        marker in makefile_text
        for marker in (
            "SDL1_SOURCE_OBJECT_PAIRS",
            "define RICH4_SDL1_COMPILE_RULE",
            "$(foreach pair,$(SDL1_SOURCE_OBJECT_PAIRS),$(eval $(call RICH4_SDL1_COMPILE_RULE,$(pair))))",
        )
    )
    if uses_generated_rules:
        rules = manifest_rules()
        recipes = rules
    else:
        rules = rule_sources(lines)
        recipes = recipe_sources(lines)
    manifest_objects = {
        line.strip()
        for line in subprocess.check_output(
            [sys.executable, str(ROOT / "tests" / "architecture" /
                                  "list_sdl1_sources.py"), "--objects"],
            text=True,
        ).splitlines()
        if line.strip()
    }
    linked = set().union(
        object_tokens(lines, "PLATFORM_OBJS"),
        object_tokens(lines, "SDL_OBJS"),
        object_tokens(lines, "SDL_AUDIO_OBJ"),
    )
    if "$(SDL1_OBJECTS)" not in "\n".join(lines):
        fail("Makefile does not link the recursive SDL1 object inventory")
    linked.update(manifest_objects)
    sources = sorted(p.relative_to(ROOT).as_posix() for p in PLATFORM_ROOT.rglob("*.c"))
    verify_dry_run_compile_inventory(sources, "desktop", "sdl")
    verify_dry_run_compile_inventory(sources, "handheld", "sdl-handheld")

    occurrences = Counter(source for values in rules.values() for source in values)
    missing = sorted(set(sources) - set(occurrences))
    duplicate_sources = sorted(source for source, count in occurrences.items() if count != 1)
    duplicate_objects = sorted(target for target, count in Counter(
        target for target, values in rules.items() for _ in values
    ).items() if count > 1)
    unlinked = sorted(
        target for target, values in rules.items()
        if any(source.startswith("src/platform/sdl1/") for source in values)
        and target not in linked
    )
    recipe_mismatch = sorted(
        target for target, values in rules.items()
        if any(source.startswith("src/platform/sdl1/") for source in values)
        and set(values) != set(recipes.get(target, []))
    )

    if missing:
        fail("source files have no object rule: " + ", ".join(missing))
    if duplicate_sources:
        fail("source files have duplicate object rules: " + ", ".join(duplicate_sources))
    if duplicate_objects:
        fail("object rules are duplicated: " + ", ".join(duplicate_objects))
    if unlinked:
        fail("object rules are not reachable from SDL_OBJS: " + ", ".join(unlinked))
    if recipe_mismatch:
        fail("object rule/recipe source mismatch: " + ", ".join(recipe_mismatch))

    print(
        "sdl object manifest: ok "
        f"({len(sources)} platform sources; {len(occurrences)} unique source rules; "
        f"{len(linked)} reachable objects; "
        f"{'generated rules' if uses_generated_rules else 'explicit rules'})"
    )


if __name__ == "__main__":
    main()
