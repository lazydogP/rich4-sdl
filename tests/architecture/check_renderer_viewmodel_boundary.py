#!/usr/bin/env python3
"""Check display input structs by recursively inspecting their field types."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
FORBIDDEN = {"GameState", "GamePresentationState", "GameSession"}


def mask_comments_and_literals(text: str) -> str:
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
            elif char == "/" and next_char == "*":
                result.extend("  ")
                index += 2
                state = "block-comment"
            elif char == '"' or char == "'":
                result.append(" ")
                index += 1
                state = "string" if char == '"' else "character"
            else:
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


def split_top_level_statements(body: str) -> list[str]:
    statements: list[str] = []
    start = 0
    depth = 0
    for index, char in enumerate(body):
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
        elif char == ";" and depth == 0:
            statements.append(body[start:index].strip())
            start = index + 1
    return statements


def parse_structs(text: str) -> dict[str, str]:
    masked = mask_comments_and_literals(text)
    structs: dict[str, str] = {}
    pattern = re.compile(
        r"\btypedef\s+struct(?:\s+(?P<tag>[A-Za-z_]\w*))?\s*\{"
    )
    for match in pattern.finditer(masked):
        end = matching_brace(masked, match.end() - 1)
        if end is None:
            continue
        alias = re.match(r"\s*(?P<name>[A-Za-z_]\w*)\s*;", masked[end + 1:])
        name = alias.group("name") if alias else match.group("tag")
        if name:
            structs[name] = masked[match.end():end]
        if match.group("tag") and name:
            structs[match.group("tag")] = masked[match.end():end]
    return structs


def parse_all_structs() -> dict[str, str]:
    structs: dict[str, str] = {}
    for path in SRC.rglob("*.h"):
        structs.update(
            parse_structs(path.read_text(encoding="utf-8", errors="replace"))
        )
    for path in SRC.rglob("*.c"):
        structs.update(
            parse_structs(path.read_text(encoding="utf-8", errors="replace"))
        )
    return structs


def field_type(statement: str, known_types: set[str]) -> tuple[str | None, bool]:
    if not statement or statement.startswith(("typedef ", "enum ", "union ")):
        return None, False
    match = re.match(
        r"(?P<decl>.*?)\b(?P<name>[A-Za-z_]\w*)\s*"
        r"(?:\[[^\]]*\])?\s*$",
        statement,
        re.S,
    )
    if match is None:
        return None, False
    declaration = match.group("decl")
    pointer = "*" in declaration
    candidates = [
        name
        for name in known_types
        if re.search(rf"\b{re.escape(name)}\b", declaration)
    ]
    if not candidates:
        return None, pointer
    return max(candidates, key=len), pointer


def function_parameter_lists(text: str) -> list[str]:
    masked = mask_comments_and_literals(text)
    pattern = re.compile(
        r"(?:^|\n)[ \t]*(?:(?![;{}]).)*?\b[A-Za-z_]\w*\s*"
        r"\((?P<params>[^()]*)\)\s*\{",
        re.S,
    )
    return [match.group("params") for match in pattern.finditer(masked)]


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


def parameter_type(parameter: str, known_types: set[str]) -> tuple[str | None, bool]:
    if not parameter or parameter == "void" or "..." in parameter:
        return None, False
    pointer = "*" in parameter
    candidates = [
        name
        for name in known_types
        if re.search(rf"\b{re.escape(name)}\b", parameter)
    ]
    if not candidates:
        return None, pointer
    return max(candidates, key=len), pointer


def renderer_sources() -> list[Path]:
    platform = SRC / "platform" / "sdl1"
    paths = set((platform / "render").rglob("*.c"))
    paths.add(platform / "runtime_main.c")
    paths.update(platform.glob("game_event_presentation*.c"))
    return sorted(paths, key=lambda path: path.as_posix().lower())


def main() -> int:
    structs = parse_all_structs()
    known_types = set(structs) | FORBIDDEN
    violations: list[str] = []
    visited: set[tuple[str, tuple[str, ...]]] = set()

    def inspect_type(type_name: str, path: list[str]) -> None:
        if type_name in FORBIDDEN:
            violations.append(" -> ".join(path + [type_name]))
            return
        body = structs.get(type_name)
        if body is None:
            return
        key = (type_name, tuple(path))
        if key in visited:
            return
        visited.add(key)
        for statement in split_top_level_statements(body):
            nested, pointer = field_type(statement, known_types)
            if nested is not None and not pointer:
                inspect_type(nested, path + [f"{type_name}.{nested}"])

    entries = 0
    for source in renderer_sources():
        text = source.read_text(encoding="utf-8", errors="replace")
        for parameters in function_parameter_lists(text):
            entries += 1
            for parameter in split_arguments(parameters):
                type_name, pointer = parameter_type(parameter, known_types)
                if type_name is None:
                    continue
                if type_name in FORBIDDEN:
                    if not pointer:
                        violations.append(
                            f"{source.relative_to(ROOT).as_posix()} parameter -> {type_name}"
                        )
                elif type_name in structs:
                    inspect_type(type_name, [source.relative_to(ROOT).as_posix()])

    if violations:
        print("renderer ViewModel boundary check failed", file=sys.stderr)
        for violation in sorted(set(violations)):
            print(
                f"display input embeds domain aggregate: {violation}",
                file=sys.stderr,
            )
        return 1

    print(
        f"renderer ViewModel boundary: ok "
        f"({entries} display entry signatures; 0 violations)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
