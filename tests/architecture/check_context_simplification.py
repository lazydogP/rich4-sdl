#!/usr/bin/env python3
"""Check the structural gates for the lean-context refactor.

Every syntactic forwarder must have an individually reviewed boundary reason.
Exceptions identify the file, function and callee; removing one never creates
room for an unrelated wrapper. All preprocessor branches are scanned.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import NamedTuple


ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
# Exact identities, not directory exemptions or a transferable count budget.
# Keep reasons tied to the boundary contract. Remove entries when the matching
# forwarder disappears; a changed callee requires another review.
FORWARDER_EXCEPTIONS: dict[tuple[str, str, str], str] = {
    ("src/platform/sdl1/audio_stub.c", "rich4_audio_shutdown", "free"):
        "The no-mixer backend implements the shared audio shutdown API and owns the allocation.",
    ("src/platform/sdl1/game_loop_scene_update.c", "update_fallback_menu_scene",
     "game_draw_scene_fallback_menu"):
        "Adapts the typed menu renderer to the scene service's void* callback contract.",
    ("src/platform/sdl1/game_loop_scene_update.c", "update_fallback_options_menu_scene",
     "game_draw_scene_fallback_options_menu"):
        "Adapts the typed menu-options renderer to the scene service's void* callback contract.",
    ("src/platform/sdl1/game_loop_scene_update.c", "update_fallback_options_game_scene",
     "game_draw_scene_fallback_options_game"):
        "Adapts the typed game-options renderer to the scene service's void* callback contract.",
    ("src/platform/sdl1/game_loop_update.c", "update_game_session",
     "rich4_sdl_update_game_session_adapter"):
        "The SDL frame entry delegates platform inputs to the session update adapter.",
    ("src/platform/sdl1/test_adapter/diff_adapter.c", "r4diff_active", "ls_active"):
        "The differential-test API hides the lockstep transport's activation state.",
    ("src/platform/sdl1/test_adapter/diff_adapter.c", "r4diff_ticks", "ls_ticks"):
        "The differential-test clock API delegates to the lockstep clock in test builds.",
    ("src/platform/sdl1/test_adapter/diff_adapter.h", "r4diff_poll", "SDL_PollEvent"):
        "Normal builds retain native SDL input behind the differential-test input API.",
    ("src/platform/sdl1/test_adapter/diff_adapter.h", "r4diff_localtime", "localtime"):
        "Normal builds retain host time behind the differential-test calendar API.",
}


class Forwarder(NamedTuple):
    path: str
    name: str
    callee: str
    line: int

    @property
    def identity(self) -> tuple[str, str, str]:
        return self.path, self.name, self.callee


def mask_comments_and_literals(text: str) -> str:
    """Replace comments and literals with spaces while preserving newlines."""

    result: list[str] = []
    index = 0
    state = "code"
    while index < len(text):
        char = text[index]
        next_char = text[index + 1] if index + 1 < len(text) else ""
        if state == "code":
            if char == "/" and next_char == "/":
                result.extend("  ")
                index += 2
                state = "line-comment"
                continue
            if char == "/" and next_char == "*":
                result.extend("  ")
                index += 2
                state = "block-comment"
                continue
            if char == '"':
                result.append(" ")
                index += 1
                state = "string"
                continue
            if char == "'":
                result.append(" ")
                index += 1
                state = "character"
                continue
            result.append(char)
            index += 1
            continue
        if state == "line-comment":
            if char == "\n":
                result.append("\n")
                state = "code"
            else:
                result.append(" ")
            index += 1
            continue
        if state == "block-comment":
            if char == "*" and next_char == "/":
                result.extend("  ")
                index += 2
                state = "code"
            else:
                result.append("\n" if char == "\n" else " ")
                index += 1
            continue
        if char == "\\" and index + 1 < len(text):
            result.extend("  ")
            index += 2
        elif char == "\n":
            result.append("\n")
            index += 1
            state = "code"
        else:
            result.append(" ")
            index += 1
    return "".join(result)


def matching_brace(text: str, opening: int) -> int | None:
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    return None


def split_arguments(text: str) -> list[str]:
    parts: list[str] = []
    start = 0
    depth = 0
    for index, char in enumerate(text):
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        elif char == "," and depth == 0:
            parts.append(text[start:index].strip())
            start = index + 1
    tail = text[start:].strip()
    if tail or text.strip():
        parts.append(tail)
    return parts


FUNCTION_RE = re.compile(
    r"(?:^|\n)[ \t]*(?P<header>(?:(?![;{}]).)*?\b"
    r"(?P<name>[A-Za-z_]\w*)\s*\((?P<params>[^()]*)\))\s*\{",
    re.S,
)


def parameter_names(parameters: str) -> list[str] | None:
    names: list[str] = []
    for parameter in split_arguments(parameters):
        if not parameter or parameter == "void":
            continue
        if "..." in parameter or "(" in parameter:
            return None
        identifiers = re.findall(r"\b[A-Za-z_]\w*\b", parameter)
        if not identifiers:
            return None
        names.append(identifiers[-1])
    return names


def semantic_forwarders(path: Path) -> list[Forwarder]:
    masked = mask_comments_and_literals(
        path.read_text(encoding="utf-8", errors="replace")
    )
    findings: list[Forwarder] = []
    for match in FUNCTION_RE.finditer(masked):
        body_start = match.end() - 1
        body_end = matching_brace(masked, body_start)
        if body_end is None:
            continue
        body = masked[body_start + 1:body_end].strip()
        call = re.fullmatch(
            r"(?:return\s+)?(?P<callee>[A-Za-z_]\w*)\s*\((?P<args>.*)\)\s*;",
            body,
            re.S,
        )
        if call is None:
            continue
        names = parameter_names(match.group("params"))
        if names is None:
            continue
        args = split_arguments(call.group("args"))
        normalized_args = [re.sub(r"\s+", "", arg) for arg in args]
        if call.group("callee") == match.group("name"):
            continue
        if normalized_args == names:
            findings.append(Forwarder(
                path.relative_to(ROOT).as_posix(), match.group("name"),
                call.group("callee"), masked.count("\n", 0, match.start("name")) + 1,
            ))
    return findings


def member_path_aliases(path: Path) -> list[tuple[str, str]]:
    aliases: list[tuple[str, str]] = []
    for line_number, line in enumerate(
        path.read_text(encoding="utf-8", errors="replace").splitlines(), 1
    ):
        match = re.match(
            r"\s*#\s*define\s+(?P<name>[A-Za-z_]\w*)\s+"
            r"(?P<value>[A-Za-z_]\w*(?:\.|->)[A-Za-z_]\w*)\s*$",
            line,
        )
        if match:
            aliases.append(
                (
                    f"{path.relative_to(ROOT).as_posix()}:{line_number}",
                    f"{match.group('name')} -> {match.group('value')}",
                )
            )
    return aliases


def main() -> int:
    forwarders: list[Forwarder] = []
    aliases: list[tuple[str, str]] = []
    for path in sorted(SRC.rglob("*"), key=lambda item: item.as_posix().lower()):
        if not path.is_file() or path.suffix not in {".c", ".h"}:
            continue
        forwarders.extend(semantic_forwarders(path))
        if path.suffix == ".h":
            aliases.extend(member_path_aliases(path))

    errors: list[str] = []
    unexpected = [f for f in forwarders if f.identity not in FORWARDER_EXCEPTIONS]
    if unexpected:
        errors.append(f"new unreviewed semantic forwarders: {len(unexpected)}")
        errors.extend(
            f"  {f.path}:{f.line} {f.name} -> {f.callee}" for f in unexpected
        )
        errors.append("Remove redundant wrappers or record a specific boundary reason in FORWARDER_EXCEPTIONS.")
    found = {f.identity for f in forwarders}
    for identity, reason in sorted(FORWARDER_EXCEPTIONS.items()):
        path, name, callee = identity
        if not reason.strip():
            errors.append(f"boundary exception requires a reason: {path} {name} -> {callee}")
        if identity not in found:
            errors.append(f"stale boundary exception must be removed: {path} {name} -> {callee}")
    if aliases:
        errors.append(f"member-path aliases remain: {len(aliases)}")
        errors.extend(f"  {path} {value}" for path, value in aliases)

    if errors:
        print("context simplification check failed", file=sys.stderr)
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(
        "context simplification: ok "
        f"({len(forwarders)} reviewed boundary forwarders; "
        f"0 unreviewed forwarders; {len(aliases)} member-path aliases)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
