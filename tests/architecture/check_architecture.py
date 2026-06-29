#!/usr/bin/env python3
"""Recursive SDL architecture guard.

The checker intentionally has no baseline-generation mode.  Existing findings
are recorded by hand in ``known_debt.tsv`` and may only disappear or decrease.
New source files are discovered from ``src`` on every invocation.
"""

from __future__ import annotations

import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
DEBT = Path(__file__).with_name("known_debt.tsv")


@dataclass(frozen=True)
class Finding:
    rule_id: str
    path: str
    normalized_match: str


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def source_files() -> list[Path]:
    return sorted(
        (p for p in SRC.rglob("*") if p.is_file() and p.suffix in {".c", ".h"}),
        key=lambda p: p.as_posix().lower(),
    )


def layer(path: Path) -> str:
    parts = path.relative_to(SRC).parts
    if not parts:
        return "root"
    if parts[0] == "platform" and len(parts) >= 2 and parts[1] == "sdl1":
        return "platform"
    return parts[0]


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//.*", "", text)


def resolve_include(path: Path, include: str) -> Path | None:
    candidates = [path.parent / include, SRC / include]
    candidates.extend(SRC.rglob(Path(include).name))
    for candidate in candidates:
        if candidate.is_file() and candidate.suffix in {".c", ".h"}:
            return candidate.resolve()
    return None


def dependency_findings(path: Path, text: str) -> list[Finding]:
    owner = layer(path)
    forbidden = {
        "app": {"platform"},
        "core": {"app", "view", "resource", "platform"},
        "data": {"core", "app", "view", "resource", "platform"},
        "resource": {"core", "app", "view", "platform"},
        "view": {"app", "platform"},
    }.get(owner, set())
    findings: list[Finding] = []
    for include_match in re.finditer(r"^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]", text, re.M):
        target = resolve_include(path, include_match.group(1))
        if target is None:
            continue
        target_layer = layer(target)
        if target_layer in forbidden:
            findings.append(
                Finding("dependency-direction", rel(path), f"include:{rel(target)}")
            )
    return findings


def source_findings(path: Path, text: str) -> list[Finding]:
    findings: list[Finding] = []
    scan_text = strip_comments(text)
    owner = layer(path)
    if owner in {"app", "core", "data", "resource", "view"}:
        for token in re.findall(r"\b(?:SDL_[A-Za-z0-9_]*|SDLKey|Uint32)\b", scan_text):
            findings.append(Finding("platform-type-leak", rel(path), token))
        if re.search(r"#\s*include\s*[<\"]SDL(?:/|\\)", scan_text):
            findings.append(Finding("platform-type-leak", rel(path), "include:SDL"))
    if "platform/sdl1/render" in path.as_posix().replace("\\", "/"):
        for _ in re.finditer(r"\bGameSession\b", scan_text):
            findings.append(Finding("renderer-direct-session", rel(path), "GameSession"))
    for macro in re.finditer(
        r"^\s*#\s*define\s+(free|rich4_mkf_read_resource|game_preview_borrow_resource)\b",
        text,
        re.M,
    ):
        findings.append(Finding("ownership-macro", rel(path), f"macro:{macro.group(1)}"))
    if re.search(r"\b(?:read_le16|read_le32|read_le16_value)\s*\(", scan_text):
        findings.append(
            Finding("byte-reader-duplicate", rel(path), "legacy-little-endian-reader")
        )
    if owner == "platform" and re.search(
        r"\b(?:session|context->session)->(?:state|flow|presentation)"
        r"[^;\r\n]*?(?<![=!<>])=(?!=)",
        scan_text,
    ):
        findings.append(
            Finding("platform-state-write", rel(path), "session-state-write")
        )
    if owner in {"app", "platform"} and re.search(
        r"\b(?:command|out_command)->(?:value|value2|flags|target2|turn_token)"
        r"\s*=(?!=)",
        scan_text,
    ):
        findings.append(
            Finding("command-payload-write", rel(path), "generic-command-payload-write")
        )
    return findings


def root_findings(path: Path) -> list[Finding]:
    if path.parent == SRC and path.name not in {"main_sdl.c", "api_contract.h"}:
        return [Finding("src-root-boundary", rel(path), path.name)]
    return []


def load_debt() -> dict[tuple[str, str, str], int]:
    if not DEBT.exists():
        return {}
    rows = DEBT.read_text(encoding="utf-8").splitlines()
    if not rows:
        return {}
    if rows[0] != "rule_id\tpath\tnormalized_match\toccurrence_count\tremoval_phase":
        raise SystemExit(f"invalid architecture debt header: {DEBT}")
    debt: dict[tuple[str, str, str], int] = {}
    for line in rows[1:]:
        if not line.strip():
            continue
        fields = line.split("\t")
        if len(fields) != 5:
            raise SystemExit(f"invalid architecture debt row: {line}")
        key = tuple(fields[:3])
        count = int(fields[3])
        if count < 1 or key in debt:
            raise SystemExit(f"invalid architecture debt entry: {line}")
        debt[key] = count
    return debt


def main() -> int:
    files = source_files()
    if not files:
        print("architecture: no production sources discovered", file=sys.stderr)
        return 1
    findings: Counter[tuple[str, str, str]] = Counter()
    for path in files:
        text = path.read_text(encoding="utf-8", errors="replace")
        for finding in dependency_findings(path, text):
            findings[(finding.rule_id, finding.path, finding.normalized_match)] += 1
        for finding in source_findings(path, text):
            findings[(finding.rule_id, finding.path, finding.normalized_match)] += 1
        for finding in root_findings(path):
            findings[(finding.rule_id, finding.path, finding.normalized_match)] += 1

    debt = load_debt()
    errors: list[str] = []
    for key, count in sorted(findings.items()):
        if key not in debt:
            errors.append(f"new violation: {key[0]} {key[1]} {key[2]} ({count})")
        elif count > debt[key]:
            errors.append(
                f"violation increased: {key[0]} {key[1]} {key[2]} "
                f"was {debt[key]}, now {count}"
            )
    for key, expected in sorted(debt.items()):
        if key not in findings:
            errors.append(f"stale debt entry must be removed: {key[0]} {key[1]} {key[2]}")

    if errors:
        print("architecture check failed", file=sys.stderr)
        for error in errors:
            print(error, file=sys.stderr)
        print(f"discovered {len(files)} production .c/.h files", file=sys.stderr)
        return 1

    print(f"architecture: ok ({len(files)} production .c/.h files; {len(debt)} known debt entries)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
