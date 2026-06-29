#!/usr/bin/env python3
"""Exercise forwarder review and diagnostics without changing production files."""

from __future__ import annotations

import contextlib
import io
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import check_context_simplification as checker


class ContextSimplificationTests(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.exceptions: dict[tuple[str, str, str], str] = {}
        for name, value in (("ROOT", self.root), ("SRC", self.root / "src"),
                            ("FORWARDER_EXCEPTIONS", self.exceptions)):
            patched = patch.object(checker, name, value)
            patched.start()
            self.addCleanup(patched.stop)

    def source(self, path: str, contents: str) -> None:
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(contents, encoding="utf-8")

    def run_check(self) -> tuple[int, str, str]:
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = checker.main()
        return status, out.getvalue(), err.getvalue()

    def test_new_business_forwarder_fails_and_removal_recovers(self) -> None:
        self.source("src/app/flow.c", "\nint renamed(int value) { return domain(value); }\n")
        status, _, err = self.run_check()
        self.assertEqual(status, 1)
        self.assertIn("new unreviewed semantic forwarders: 1", err)
        self.assertIn("src/app/flow.c:2 renamed -> domain", err)
        self.source("src/app/flow.c", "")
        self.assertEqual(self.run_check()[0], 0)

    def test_exact_reviewed_boundary_passes(self) -> None:
        self.source("src/platform/boundary.c", "int bridge(int x) { return native(x); }\n")
        self.exceptions[("src/platform/boundary.c", "bridge", "native")] = "Backend API contract."
        status, out, err = self.run_check()
        self.assertEqual((status, err), (0, ""))
        self.assertIn("1 reviewed boundary forwarders; 0 unreviewed", out)

    def test_exception_does_not_cover_another_path_name_or_callee(self) -> None:
        self.exceptions[("src/platform/boundary.c", "bridge", "native")] = "Backend API contract."
        self.source("src/platform/boundary.c", "int bridge(int x) { return changed(x); }\n"
                    "int renamed(int x) { return native(x); }\n")
        self.source("src/app/copied.c", "int bridge(int x) { return native(x); }\n")
        status, _, err = self.run_check()
        self.assertEqual(status, 1)
        self.assertIn("new unreviewed semantic forwarders: 3", err)
        self.assertIn("src/platform/boundary.c:1 bridge -> changed", err)
        self.assertIn("src/platform/boundary.c:2 renamed -> native", err)
        self.assertIn("src/app/copied.c:1 bridge -> native", err)

    def test_removing_boundary_cannot_fund_new_business_wrapper(self) -> None:
        self.exceptions[("src/platform/boundary.c", "bridge", "native")] = "Backend API contract."
        self.source("src/app/flow.c", "int wrapper(int x) { return domain(x); }\n")
        status, _, err = self.run_check()
        self.assertEqual(status, 1)
        self.assertIn("src/app/flow.c:1 wrapper -> domain", err)
        self.assertIn("stale boundary exception must be removed", err)

    def test_stale_exception_and_missing_reason_fail(self) -> None:
        self.exceptions[("src/platform/boundary.c", "bridge", "native")] = "Backend API contract."
        status, _, err = self.run_check()
        self.assertEqual(status, 1)
        self.assertIn("stale boundary exception must be removed", err)
        self.source("src/platform/boundary.c", "int bridge(int x) { return native(x); }\n")
        self.exceptions[("src/platform/boundary.c", "bridge", "native")] = "  "
        status, _, err = self.run_check()
        self.assertEqual(status, 1)
        self.assertIn("boundary exception requires a reason", err)

    def test_test_adapter_directory_and_both_build_branches_are_checked(self) -> None:
        self.source("src/platform/test_adapter/input.h",
                    "#ifdef TEST_BUILD\n"
                    "static inline int poll(int x) { return simulated(x); }\n"
                    "#else\n"
                    "static inline int poll(int x) { return native(x); }\n"
                    "#endif\n")
        self.exceptions[("src/platform/test_adapter/input.h", "poll", "simulated")] = "Test input backend."
        status, _, err = self.run_check()
        self.assertEqual(status, 1)
        self.assertIn("src/platform/test_adapter/input.h:4 poll -> native", err)
        self.exceptions[("src/platform/test_adapter/input.h", "poll", "native")] = "Normal input backend."
        self.assertEqual(self.run_check()[0], 0)

    def test_transformations_validation_and_side_effects_are_not_forwarders(self) -> None:
        self.source("src/app/flow.c",
                    "int transform(int x) { return domain(x + 1); }\n"
                    "int validate(int x) { if (x < 0) return 0; return domain(x); }\n"
                    "void effect(int x) { audit(x); domain(x); }\n"
                    "/* int comment(int x) { return domain(x); } */\n")
        self.assertEqual(self.run_check()[0], 0)

    def test_member_path_alias_remains_forbidden(self) -> None:
        self.source("src/app/state.h", "#define alias session->state\n")
        status, _, err = self.run_check()
        self.assertEqual(status, 1)
        self.assertIn("member-path aliases remain: 1", err)
        self.assertIn("src/app/state.h:1 alias -> session->state", err)


if __name__ == "__main__":
    unittest.main()
