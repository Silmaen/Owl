"""
Tests that check nothing, for the `CodeStyle` gate: every `TEST` / `TEST_F` / `TEST_P` body holds at least one
gtest assertion (`EXPECT_*`, `ASSERT_*`, `GTEST_SKIP`, `FAIL`, `ADD_FAILURE`, `SUCCEED`) or calls a helper named
`expect…` / `assert…` / `check…` that does. A test that only "does not crash" says so with `EXPECT_NO_THROW`.
"""

from __future__ import annotations

import re
from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path

_TEST_RE = re.compile(r"^(?:TEST|TEST_F|TEST_P)\(\s*(\w+)\s*,\s*(\w+)\s*\)\s*\{", re.MULTILINE)
_CHECK_RE = re.compile(
    r"\b(?:EXPECT_\w+|ASSERT_\w+|GTEST_SKIP|FAIL|ADD_FAILURE|SUCCEED|(?:expect|assert|check)[A-Z]\w*)\b"
)


@dataclass(frozen=True)
class EmptyTest:
    """A test whose body holds no assertion."""

    path: Path
    line: int
    name: str


def _body(text: str, start: int) -> str:
    depth = 1
    index = start
    while depth and index < len(text):
        depth += {"{": 1, "}": -1}.get(text[index], 0)
        index += 1
    return text[start:index]


def empty_tests(files: Iterable[Path]) -> list[EmptyTest]:
    """
    Find the tests that assert nothing.

    :param files: The test sources.
    :return: One entry per test without any assertion, in file and line order.
    """
    found: list[EmptyTest] = []
    for path in files:
        try:
            text = path.read_text(errors="replace")
        except OSError:
            continue
        for match in _TEST_RE.finditer(text):
            if not _CHECK_RE.search(_body(text, match.end())):
                line = text.count("\n", 0, match.start()) + 1
                found.append(EmptyTest(path, line, f"{match.group(1)}.{match.group(2)}"))
    return found
