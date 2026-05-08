"""
Code-style and documentation-quality gate for the Owl Engine.

This action **only inspects** the tree — it never rewrites anything. A failing
check means the developer must fix the source themselves.

Sub-checks (all on by default):

* **clang-format** dry-run on every C++ source / header — fails if any file
  would be reformatted.
* **typos** — runs `codespell` on every source / header / docs file.
* **comment-quality** — Doxygen comment hygiene that doxygen itself does not
  flag: `///` only allowed on single-line member/enum-value comments,
  `@brief` / parameter / return descriptions must end with a sentence
  terminator (`.`, `?`, `!`).
* **private-member-docs** — every `m_*` / `mp_*` / `s_*` / `g_*` field needs
  a `///` line above or `///<` inline.
* **cpp-style** — project-specific conventions that clang-format misses:
  banned `std::shared_ptr` / `std::make_shared` (use `shared<T>` / `mkShared<T>()`);
  banned class-name suffixes `*Service` / `*Helper` / `*Util`; `UI*` identifier
  prefix (must be `Ui*`); `enum class` (must be `enum struct`); blank-line
  rule after `OWL_PROFILE_FUNCTION()` / around `OWL_DIAG_PUSH/POP`; logging
  message format (`Subsystem: capitalized message ending with .`).
* **structural** — file header (`@file` tag, `Copyright (c) YYYY`); `OWL_API`
  warning for free functions in `source/owl/{public,private}` headers.

Doxygen is **deliberately not** run here — the project already has a separate
`Documentation` action that builds doxygen with `WARN_AS_ERROR=YES`.

Each sub-check can be disabled with extra args (`-- --no-<name>=true`):
`--no-format`, `--no-typos`, `--no-comment-quality`, `--no-doc-audit`,
`--no-cpp-style`, `--no-structural`.
"""

from __future__ import annotations

import re
import shutil
import subprocess
from pathlib import Path
from typing import Iterable, List, Optional, Tuple

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig


# ────────────────────────────────────────────────────────────────────────────
# Source discovery
# ────────────────────────────────────────────────────────────────────────────

SOURCE_ROOTS: tuple[Path, ...] = (
    root / "source" / "owl" / "public",
    root / "source" / "owl" / "private",
    root / "source" / "owlnest" / "sources",
    root / "source" / "owlrunner" / "sources",
)
"""Directories scanned by every sub-check that walks the tree."""

CXX_EXTENSIONS: tuple[str, ...] = (".h", ".hpp", ".cpp", ".cc", ".cxx", ".inl")
"""File extensions inspected by `clang-format` and the cpp-style audit."""

HEADER_EXTENSIONS: tuple[str, ...] = (".h", ".hpp")
"""File extensions inspected by header-only audits."""

DOC_ROOTS: tuple[Path, ...] = (
    root / "doc",
    root,  # README, CHANGELOG, CONTRIBUTING at the root
)
"""Directories scanned by the typo check (in addition to source roots)."""


def _iter_sources(roots: Iterable[Path], extensions: tuple[str, ...]) -> List[Path]:
    """
    Walk the given roots and return every source file with one of `extensions`.
    Returned in deterministic (sorted) order so the report is stable.
    """
    out: List[Path] = []
    for r in roots:
        if not r.exists():
            continue
        for path in sorted(r.rglob("*")):
            if path.is_file() and path.suffix in extensions:
                out.append(path)
    return out


# ────────────────────────────────────────────────────────────────────────────
# Sub-check 1 — clang-format dry-run
# ────────────────────────────────────────────────────────────────────────────


def _check_clang_format() -> int:
    """
    Run `clang-format --dry-run --Werror` on every C++ source. Returns 0 when
    every file is already formatted, or a non-zero count of misformatted files.
    """
    log.info("code-style: clang-format dry-run...")
    if shutil.which("clang-format") is None:
        log.error("[clang-format] tool not found in PATH")
        return 1
    files = _iter_sources(SOURCE_ROOTS, CXX_EXTENSIONS)
    if not files:
        log.warning("[clang-format] no C++ sources discovered")
        return 0
    failures = 0
    batch_size = 32
    for i in range(0, len(files), batch_size):
        chunk = [str(p) for p in files[i: i + batch_size]]
        result = subprocess.run(
            ["clang-format", "--dry-run", "--Werror", *chunk],
            cwd=str(root),
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            for line in (result.stderr or "").splitlines():
                if line.strip():
                    log.error(f"clang-format: {line}")
            failures += 1
    return failures


# ────────────────────────────────────────────────────────────────────────────
# Sub-check 2 — codespell (typos)
# ────────────────────────────────────────────────────────────────────────────


def _resolve_codespell_command() -> Optional[List[str]]:
    """
    Find codespell across the three deployment scenarios:

    1. `codespell` is on `PATH` (developer ran us via `poetry run …`).
    2. The `codespell` script lives next to `sys.executable` (we're already
       inside the poetry venv but `PATH` was scrubbed by the parent shell).
    3. Codespell is importable as `codespell_lib`, in which case we invoke
       it via `python -m codespell_lib` — same effect, no script needed.

    Returns the `argv[0..]` list, or `None` if codespell really isn't
    reachable from the current interpreter.
    """
    import sys
    if (path := shutil.which("codespell")) is not None:
        return [path]
    sibling = Path(sys.executable).parent / "codespell"
    if sibling.exists():
        return [str(sibling)]
    sibling_exe = Path(sys.executable).parent / "codespell.exe"
    if sibling_exe.exists():
        return [str(sibling_exe)]
    try:
        import codespell_lib  # noqa: F401  (probe only)

        return [sys.executable, "-m", "codespell_lib"]
    except ImportError:
        return None


def _check_typos() -> int:
    """
    Run `codespell` over the source + docs trees. Use a project-local ignore
    file so legitimate identifiers don't trip the dictionary.
    """
    log.info("code-style: codespell typo scan...")
    base_cmd = _resolve_codespell_command()
    if base_cmd is None:
        log.warning(
            "codespell: tool not reachable from this interpreter — run via "
            "`poetry run python ci_action.py …` or `poetry sync` first"
        )
        return 0
    targets: List[str] = []
    for r in SOURCE_ROOTS:
        if r.exists():
            targets.append(str(r))
    docs_dir = root / "doc"
    if docs_dir.exists():
        targets.append(str(docs_dir))
    for f in ("README.md", "CHANGELOG.md", "CONTRIBUTING.md"):
        p = root / f
        if p.exists():
            targets.append(str(p))
    if not targets:
        return 0
    cmd = [*base_cmd, "--quiet-level=2"]
    ignore_words = root / "ci" / "codespell-ignore-words.txt"
    if ignore_words.exists():
        cmd += [f"--ignore-words={ignore_words}"]
    cmd += targets
    result = subprocess.run(cmd, cwd=str(root), capture_output=True, text=True)
    misses = 0
    for line in (result.stdout or "").splitlines():
        if line.strip():
            log.error(f"codespell: {line}")
            misses += 1
    return misses


# ────────────────────────────────────────────────────────────────────────────
# Sub-check 3 — comment quality (`///` discipline + sentence punctuation)
# ────────────────────────────────────────────────────────────────────────────


# Match a `/**` block — captures everything up to the closing `*/`.
_DOXY_BLOCK_RE = re.compile(r"/\*\*(?!/)(?P<body>.*?)\*/", re.DOTALL)
_TRIPLE_SLASH_LINE_RE = re.compile(r"^\s*///(?!<)(?!\s*$).*$")
# A description "sentence" lives on a `* word ...` line inside a `/** */`.
_DOC_PROSE_LINE_RE = re.compile(r"^\s*\*\s*(?P<text>[^@\s].*?)\s*$")

# Detect a function declaration line. Heuristic: contains a `(` followed by a
# matching `)`, ends with `;` or `{` (or `= ...;`), and is not an obvious
# declaration of a local variable / member field (no `=` before the `(`).
# Also covers trailing-return-type form `auto name(...) -> T`.
_FUNCTION_DECL_RE = re.compile(
    r"^[^/=]*?\b\w+\s*\([^;]*\)\s*"
    r"(?:const\s*|noexcept\s*|override\s*|final\s*|=\s*default\s*|=\s*delete\s*|=\s*0\s*)*"
    r"(?:->[^;{]+)?\s*[;{]"
)
# Pull the return type out of a function declaration so we can detect `void`.
# Anchored on `)` (followed by optional cv-/noexcept-qualifiers) so an arrow
# operator in a body expression (e.g. `m_x->doThing()`) cannot be misread as
# a trailing return type.
_RETURN_TRAILING_RE = re.compile(
    r"\)\s*(?:const\s*|noexcept\s*|override\s*|final\s*)*->\s*([^;{]+?)\s*[;{]"
)
# Specifiers consumed BEFORE looking for the leading return type. Matched as a
# greedy block so a token like `explicit` can never be mistaken for the type.
# `OWL_API` (and conventional `*_API` export macros) are also accepted in
# leading position; they can also sit between the return type and the
# function name and are stripped from `_API_MACRO_RE` after capture.
_LEADING_SPECIFIERS_RE = re.compile(
    r"^\s*(?:\[\[[^\]]+\]\]\s*|explicit\s+|virtual\s+|inline\s+|constexpr\s+|"
    r"static\s+|friend\s+|extern\s+|noexcept\s+|OWL_API\s+|[A-Z][A-Z0-9_]+_API\s+)+"
)
_RETURN_LEADING_RE = re.compile(
    r"^\s*(?P<ret>[\w:<>,&\*\s]+?)\s+\w+\s*\("
)
# Strip export-macro tokens that happen to be captured inside the return type
# (e.g. `void OWL_API foo(...)` → captured `ret = "void OWL_API"`).
_API_MACRO_RE = re.compile(r"\b(?:OWL_API|[A-Z][A-Z0-9_]+_API)\b")


def _check_comment_quality() -> int:
    """
    Five checks:

    1. **`///` discipline** — `///` is reserved for single-line comments. Two
       or more consecutive `///` lines on member declarations are an
       anti-pattern (use `/** ... */` instead).
    2. **Sentence punctuation** — the last prose line of a `/** ... */`
       description must end with `.`, `?`, or `!`. Single-fragment / short
       declarations (less than 16 chars) are tolerated.
    3. **Function comment style** — a function declaration must be documented
       with a `/** ... */` block, never with a `///` line above it.
    4. **Missing `@return`** — when a `/** ... */` block sits above a
       function with a non-`void` return type, the block must contain a
       `@return` tag.
    5. **`@brief` on its own line** — `@brief` must NOT carry text on the
       same line. The description belongs on the next line indented one
       extra space after the `*`.

    Helpers in an anonymous namespace (`namespace { ... }`) and `static`
    free functions inside a `.cpp` are skipped — they're translation-unit
    locals and don't need to be documented.
    """
    log.info("code-style: comment-quality audit...")
    issues = 0
    for path in _iter_sources(SOURCE_ROOTS, CXX_EXTENSIONS):
        text = path.read_text(errors="replace")
        lines = text.splitlines()

        # --- check 1: consecutive `///` lines outside `/** */` blocks
        block_ranges: list[tuple[int, int]] = []
        for m in _DOXY_BLOCK_RE.finditer(text):
            block_ranges.append((m.start(), m.end()))

        def _in_doxy_block(byte_offset: int) -> bool:
            return any(s <= byte_offset < e for s, e in block_ranges)

        offset = 0
        run_start = -1
        for i, line in enumerate(lines):
            line_offset = offset
            offset += len(line) + 1  # +1 for newline
            in_block = _in_doxy_block(line_offset)
            if not in_block and _TRIPLE_SLASH_LINE_RE.match(line):
                if run_start == -1:
                    run_start = i
            else:
                if run_start != -1 and i - run_start >= 2:
                    log.error(
                        f"slash-discipline: {path.relative_to(root)}:{run_start + 1}: "
                        f"`///` block spans {i - run_start} lines — use `/** */`"
                    )
                    issues += 1
                run_start = -1
        if run_start != -1 and len(lines) - run_start >= 2:
            log.error(
                f"slash-discipline: {path.relative_to(root)}:{run_start + 1}: "
                f"`///` block spans {len(lines) - run_start} lines — use `/** */`"
            )
            issues += 1

        # --- check 5: `@brief` must be on its own line
        for m in _DOXY_BLOCK_RE.finditer(text):
            body = m.group("body")
            block_offset = m.start()
            block_line_index = text.count("\n", 0, block_offset)
            for offset, raw in enumerate(body.splitlines()):
                stripped = raw.strip().lstrip("*").strip()
                if not stripped.startswith("@brief"):
                    continue
                rest = stripped[len("@brief"):].lstrip()
                if rest:
                    log.error(
                        f"brief-on-own-line: {path.relative_to(root)}:"
                        f"{block_line_index + offset + 1}: `@brief` carries text "
                        f"on the same line — content must go on the next line "
                        f"indented one extra space"
                    )
                    issues += 1

        # --- check 2: sentence-terminator on `/** */` blocks
        for m in _DOXY_BLOCK_RE.finditer(text):
            body = m.group("body")
            block_lines = body.splitlines()
            # Group prose into "paragraphs" — every sequence of non-tag
            # `* word ...` lines forms one logical sentence whose final char
            # must be a terminator.
            paragraph: list[str] = []
            block_offset = m.start()
            block_line_index = text.count("\n", 0, block_offset)

            def _flush_paragraph() -> int:
                nonlocal issues
                if not paragraph:
                    return 0
                joined = " ".join(paragraph).strip()
                # Allow short / fragmentary descriptions.
                if len(joined) < 16:
                    return 0
                if joined.endswith((".", "?", "!", ":", ";")):
                    return 0
                # Skip fenced lists like `- foo`.
                if joined.startswith(("- ", "* ", "1. ")):
                    return 0
                log.error(
                    f"doc-punct: {path.relative_to(root)}:{block_line_index + 1}: "
                    f"comment paragraph does not end with `.`: {joined[:80]}"
                )
                issues += 1
                return 1

            for j, raw in enumerate(block_lines):
                stripped = raw.strip().lstrip("*").strip()
                if not stripped:
                    _flush_paragraph()
                    paragraph = []
                    continue
                if stripped.startswith("@"):
                    # Tag line: also describe its tail prose.
                    # `@brief Foo bar` / `@param[in] iX something something`.
                    _flush_paragraph()
                    paragraph = []
                    # Collect the tag's description into a fresh paragraph.
                    tag_text = re.sub(r"^@\w+(\[[^\]]+\])?\s*\S*\s*", "", stripped, count=1)
                    # `@brief` on its own line (project rule) — text is on the next line.
                    if tag_text:
                        paragraph.append(tag_text)
                else:
                    paragraph.append(stripped)
            _flush_paragraph()

        # --- checks 3 & 4: function-declaration comment style + missing @return
        issues += _check_function_doc_style(path, lines, text)

    return issues


def _file_anon_namespace_ranges(lines: list[str]) -> list[tuple[int, int]]:
    """
    Return inclusive `(start_line, end_line)` ranges (0-based) covering every
    anonymous namespace body in the file. Tracking is brace-based and very
    rough — good enough to skip helpers inside `namespace { ... }`.
    """
    ranges: list[tuple[int, int]] = []
    depth = 0
    anon_open_at: int | None = None
    anon_open_depth = -1
    for i, line in enumerate(lines):
        stripped = line.strip()
        if anon_open_at is None and re.match(r"^\s*namespace\s*\{", stripped):
            anon_open_at = i
            anon_open_depth = depth
        opens = stripped.count("{")
        closes = stripped.count("}")
        depth += opens - closes
        if anon_open_at is not None and depth <= anon_open_depth:
            ranges.append((anon_open_at, i))
            anon_open_at = None
            anon_open_depth = -1
    return ranges


def _check_function_doc_style(path: Path, lines: list[str], text: str) -> int:
    """
    Walk the file and, for every function declaration:
      * if a `///` comment sits directly above, complain (use `/** */`).
      * if a `/** */` block sits above and the function returns non-void,
        require a `@return` tag inside the block.

    Free functions / methods that are TU-local (anonymous namespace, or
    `static` free function inside a `.cpp`) are skipped.
    """
    issues = 0
    is_cpp = path.suffix in {".cpp", ".cc", ".cxx", ".inl"}
    anon_ranges = _file_anon_namespace_ranges(lines) if is_cpp else []

    def _in_anon_ns(line_no: int) -> bool:
        return any(s <= line_no <= e for s, e in anon_ranges)

    rel = path.relative_to(root)

    for i, line in enumerate(lines):
        stripped = line.strip()
        # Cheap reject: the line must look like a function decl. We require
        # `(` and `)` and a terminator (`;` or `{`).
        if "(" not in stripped or ")" not in stripped:
            continue
        if not (stripped.endswith(";") or stripped.endswith("{") or
                stripped.endswith("}") or "= delete" in stripped or "= default" in stripped):
            continue
        # Filter out lines that are clearly not function declarations:
        # keywords, control flow, macro calls, calls used as expressions.
        first_token = stripped.split("(", 1)[0]
        if any(k in first_token for k in (
            "if ", "else", "while ", "for ", "switch ", "case ", "return ",
            "throw ", "do ", "catch ", "try ", "do{", "//", "/*",
        )):
            continue
        if first_token.strip().startswith(("#", "@", "//")):
            continue
        # Skip friend declarations — they document the befriended target,
        # not the current scope.
        if first_token.strip().startswith("friend"):
            continue
        # Skip in-cpp TU-local helpers.
        if is_cpp and "static " in stripped[:64]:
            continue
        if _in_anon_ns(i):
            continue
        # Skip operator overloads — Doxygen handles them differently.
        if "operator" in stripped:
            continue
        # Reject variable declarations with function-call initializers
        # (`Foo bar = baz();`, `Foo bar = std::numeric_limits<...>::max();`,
        # `std::function<void()> m_callback = ...;`). We require the FIRST
        # `(` to come BEFORE any `=` and to be preceded by an identifier
        # (the function name) — variable initializers always have `=`
        # before their `(`.
        first_paren = stripped.index("(")
        first_eq = stripped.find("=", 0, first_paren)
        if 0 <= first_eq < first_paren:
            continue
        # Reject `std::function<...>` style member fields where the `()`
        # belongs to the template type, not to a function being declared.
        # Pattern: the `(` is part of `<something()>` so the angle-bracket
        # nesting is non-zero at the `(` position.
        prefix = stripped[:first_paren]
        if prefix.count("<") > prefix.count(">"):
            continue
        # The token immediately before the first `(` must be an identifier
        # (the function name). Reject when there's a non-identifier char
        # right before — that's almost always an expression, not a decl.
        prev_char_idx = first_paren - 1
        while prev_char_idx >= 0 and stripped[prev_char_idx].isspace():
            prev_char_idx -= 1
        if prev_char_idx < 0 or not (stripped[prev_char_idx].isalnum() or stripped[prev_char_idx] == "_"):
            continue
        # Scan upward for the comment block above this declaration.
        j = i - 1
        while j >= 0 and lines[j].strip() == "":
            j -= 1
        if j < 0:
            continue
        prev = lines[j].rstrip()
        prev_stripped = prev.strip()
        # Case A: previous line is a `///` line (single triple-slash doc).
        if prev_stripped.startswith("///") and not prev_stripped.startswith("///<"):
            log.error(
                f"slash-on-fn: {rel}:{i + 1}: function declaration documented "
                f"with `///` (must use `/** */`): {stripped[:80]}"
            )
            issues += 1
            continue
        # Case B: previous line ends a `/** */` block. Walk up to its `/**`,
        # but only across lines that are part of the same comment (start with
        # `*` or contain `/**`). A non-comment line above means the `*/` was
        # the trailing end of an unrelated comment (e.g. C-style `/* */` at a
        # different scope) and we must NOT pretend it documents this function.
        if prev_stripped.endswith("*/"):
            block_end_line = j
            block_start_line = j
            while block_start_line >= 0 and "/**" not in lines[block_start_line]:
                line_strip = lines[block_start_line].lstrip()
                if not (line_strip.startswith("*") or line_strip.startswith("/*")):
                    block_start_line = -1
                    break
                block_start_line -= 1
            if block_start_line < 0:
                continue
            # Reject single-asterisk `/* */` comments (only `/**` opens a
            # Doxygen block).
            if "/**" not in lines[block_start_line]:
                continue
            block_text = "\n".join(lines[block_start_line: block_end_line + 1])
            # Determine the function's return type. Try trailing-return
            # syntax first, fall back to leading (after stripping specifiers
            # like `explicit` / `virtual` / attributes that would otherwise
            # be captured as the "type").
            ret = ""
            if mt := _RETURN_TRAILING_RE.search(stripped):
                ret = mt.group(1).strip()
            else:
                # Strip leading specifiers / attributes before extracting
                # the type. After this, a constructor (`Foo(args);`) leaves
                # only one identifier before `(` and the leading regex fails.
                no_specs = _LEADING_SPECIFIERS_RE.sub("", stripped, count=1)
                if ml := _RETURN_LEADING_RE.match(no_specs):
                    ret = ml.group("ret").strip()
            if not ret or ret == "void":
                continue
            # Strip residual attributes from the return type.
            ret_clean = re.sub(r"\[\[[^\]]+\]\]\s*", "", ret).strip()
            ret_clean = _API_MACRO_RE.sub("", ret_clean).strip()
            if ret_clean == "void":
                continue
            # Skip destructors (start with `~`) explicitly.
            if "~" in stripped.split("(", 1)[0]:
                continue
            # `@copydoc` / `@copybrief` inherit doc from the referenced symbol
            # — skip them.
            if "@copydoc" in block_text or "@copybrief" in block_text:
                continue
            if "@return" not in block_text and "@returns" not in block_text:
                log.error(
                    f"missing-return-doc: {rel}:{i + 1}: non-void function "
                    f"missing `@return`: {stripped[:80]}"
                )
                issues += 1
            elif re.search(r"@returns?\s+TODO\.?\s*$", block_text, re.MULTILINE):
                # Author dropped a placeholder and never came back to fill it.
                log.error(
                    f"return-todo: {rel}:{i + 1}: `@return TODO.` placeholder "
                    f"left unfilled: {stripped[:80]}"
                )
                issues += 1

    return issues


# ────────────────────────────────────────────────────────────────────────────
# Sub-check 4 — private-member doc audit
# ────────────────────────────────────────────────────────────────────────────


_SKIP_KEYWORDS = re.compile(
    r"^\s*(?:friend|using|namespace|template|enum|struct|class|public|private|"
    r"protected|return|if|else|while|for|switch|case|default|throw|operator|"
    r"static_assert|OWL_|@\w+|//|/\*|\*|#)\b"
)
_ACCESS_RE = re.compile(r"^\s*(public|private|protected)\s*:\s*$")
_FIELD_RE = re.compile(
    # Field heuristic: storage qualifiers (optional) + type token (must contain
    # at least one non-whitespace char so a stray indent doesn't pass) + space
    # + member identifier + opener.
    r"^\s*(?:mutable\s+|static\s+|constexpr\s+|inline\s+|const\s+|volatile\s+)*"
    r"[\w:<>,&\*][\w:<>,&\*\s]*\s+(m_\w+|mp_\w+|s_\w+|g_\w+)\s*[=;{]"
)


def _has_doc_above(lines: list[str], idx: int) -> bool:
    """True when the line(s) directly above `idx` form a Doxygen comment block."""
    j = idx - 1
    while j >= 0 and lines[j].strip() == "":
        j -= 1
    if j < 0:
        return False
    s = lines[j].strip()
    return s.startswith("///") or s.endswith("*/")


def _has_inline_doc(line: str) -> bool:
    """True when the line itself ends with a `///<` or `//!<` Doxygen comment."""
    return "///<" in line or "//!<" in line


def _check_private_member_docs() -> int:
    """
    Walk every header and report private member fields with no `///` doc above.
    """
    log.info("code-style: private-member doc audit...")
    files = _iter_sources(SOURCE_ROOTS, HEADER_EXTENSIONS)
    misses = 0
    for path in files:
        try:
            lines = path.read_text(errors="replace").splitlines()
        except OSError:
            continue
        access = "public"
        for i, line in enumerate(lines):
            if re.match(r"^\s*(?:template[^>]*>\s*)?(?:class|struct)\s+\w", line):
                access = "private" if "class " in line.split("//")[0] else "public"
            m = _ACCESS_RE.match(line)
            if m:
                access = m.group(1)
                continue
            if access == "public":
                continue
            stripped = line.strip()
            if not stripped or _SKIP_KEYWORDS.match(line):
                continue
            if not _FIELD_RE.match(line):
                continue
            if _has_doc_above(lines, i) or _has_inline_doc(line):
                continue
            log.error(
                f"doc-audit: {path.relative_to(root)}:{i + 1}: undocumented "
                f"private member: {stripped[:80]}"
            )
            misses += 1
    return misses


# ────────────────────────────────────────────────────────────────────────────
# Sub-check 5 — cpp-style audit (project conventions clang-format misses)
# ────────────────────────────────────────────────────────────────────────────


# Banned standard-library aliases that the project has its own wrappers for.
# Patterns require a `<` / `(` after to skip mentions inside comments / strings
# (e.g. "std::shared_ptr" in a doc comment is fine).
_BANNED_STD_PTR = (
    (re.compile(r"\bstd::shared_ptr\s*<"), "std::shared_ptr<", "shared<"),
    (re.compile(r"\bstd::make_shared\s*<"), "std::make_shared<", "mkShared<"),
    (re.compile(r"\bstd::unique_ptr\s*<"), "std::unique_ptr<", "uniq<"),
    (re.compile(r"\bstd::make_unique\s*<"), "std::make_unique<", "mkUniq<"),
    (re.compile(r"\bstd::weak_ptr\s*<"), "std::weak_ptr<", "weak<"),
)

# Banned class-name suffixes per `.claude/rules/cpp-style.md`.
_BANNED_SUFFIX_RE = re.compile(
    r"\b(?:class|struct)\s+(\w+(?:Service|Helper|Util))\b"
)

# `UI*` identifier prefix (project wants `Ui*`). We match `UI` followed by
# uppercase to avoid matching legitimate macros like `UIDS`.
_UI_PREFIX_RE = re.compile(r"\b(UI(?:Layer|Panel|Text|Widget|Button|Slider|Element|Manager|Helper)\w*)")

# `enum class` (project uses `enum struct`).
_ENUM_CLASS_RE = re.compile(r"\benum\s+class\b")

# Logging macros — capture the format string to lint its content.
_LOG_MACRO_RE = re.compile(
    r'\bOWL_(?:CORE_)?(?:TRACE|INFO|WARN|ERROR|CRITICAL)\s*\(\s*"((?:[^"\\]|\\.)*)"'
)

# `OWL_PROFILE_FUNCTION()` should be followed by a blank line.
_PROFILE_FN_RE = re.compile(r"^\s*OWL_PROFILE_FUNCTION\s*\(\s*\)\s*$")

# Diagnostic block markers.
_DIAG_PUSH_RE = re.compile(r"^\s*OWL_DIAG_PUSH\s*$")
_DIAG_POP_RE = re.compile(r"^\s*OWL_DIAG_POP\s*$")


def _check_cpp_style() -> int:
    """
    Project-specific style rules that clang-format does not enforce.

    Catches:
      - `std::shared_ptr` / `make_shared` / `unique_ptr` / `make_unique` /
        `weak_ptr` (must use `shared` / `mkShared` / `uniq` / `mkUniq` / `weak`)
      - class names with banned suffixes `*Service` / `*Helper` / `*Util`
      - `UI*` identifiers (must be `Ui*`)
      - `enum class` (must be `enum struct`)
      - `OWL_PROFILE_FUNCTION()` not followed by a blank line
      - missing blank line before `OWL_DIAG_PUSH` / after `OWL_DIAG_POP`
      - blank line directly after `OWL_DIAG_PUSH` or before `OWL_DIAG_POP`
        (the block must be contiguous)
      - logging messages without a trailing period (when length ≥ 16 chars)
    """
    log.info("code-style: cpp-style audit...")
    issues = 0
    for path in _iter_sources(SOURCE_ROOTS, CXX_EXTENSIONS):
        try:
            text = path.read_text(errors="replace")
        except OSError:
            continue
        lines = text.splitlines()
        rel = path.relative_to(root)

        # Strip line-comments and block-comments before pattern checks so we
        # don't flag `std::shared_ptr` inside a docstring.
        # (We keep the line numbers by replacing comment content with spaces.)
        code = _strip_comments(text)
        code_lines = code.splitlines()

        # --- banned std:: aliases (skip the header that defines the wrappers)
        rel_str = str(rel).replace("\\", "/")
        if not rel_str.endswith("public/core/Core.h"):
            for line_no, line in enumerate(code_lines, start=1):
                for pat, banned, replacement in _BANNED_STD_PTR:
                    if pat.search(line):
                        log.error(
                            f"smart-ptr: {rel}:{line_no}: use `{replacement}` "
                            f"instead of `{banned}`"
                        )
                        issues += 1

        # --- banned class-name suffixes
        for m in _BANNED_SUFFIX_RE.finditer(code):
            ln = code.count("\n", 0, m.start()) + 1
            log.error(
                f"class-suffix: {rel}:{ln}: forbidden class-name suffix "
                f"on `{m.group(1)}` — fold helpers into a `utils` namespace"
            )
            issues += 1

        # --- UI* prefix
        for m in _UI_PREFIX_RE.finditer(code):
            ln = code.count("\n", 0, m.start()) + 1
            ident = m.group(1)
            log.error(
                f"ui-prefix: {rel}:{ln}: use `Ui*` instead of `UI*` "
                f"(found `{ident}`)"
            )
            issues += 1

        # --- enum class
        for m in _ENUM_CLASS_RE.finditer(code):
            ln = code.count("\n", 0, m.start()) + 1
            log.error(
                f"enum-style: {rel}:{ln}: use `enum struct` instead of `enum class`"
            )
            issues += 1

        # --- profile / diag blanks
        for i, line in enumerate(code_lines):
            if _PROFILE_FN_RE.match(line):
                # Next line should be blank.
                if i + 1 < len(code_lines) and code_lines[i + 1].strip():
                    log.error(
                        f"profile-blank: {rel}:{i + 1}: "
                        f"`OWL_PROFILE_FUNCTION()` must be followed by a blank line"
                    )
                    issues += 1
            if _DIAG_PUSH_RE.match(line):
                # Previous line must be blank (or top of file).
                if i > 0 and code_lines[i - 1].strip():
                    log.error(
                        f"diag-block: {rel}:{i + 1}: "
                        f"`OWL_DIAG_PUSH` must be preceded by a blank line"
                    )
                    issues += 1
                # Next line must NOT be blank.
                if i + 1 < len(code_lines) and not code_lines[i + 1].strip():
                    log.error(
                        f"diag-block: {rel}:{i + 2}: "
                        f"no blank line allowed directly after `OWL_DIAG_PUSH`"
                    )
                    issues += 1
            if _DIAG_POP_RE.match(line):
                # Previous line must NOT be blank.
                if i > 0 and not code_lines[i - 1].strip():
                    log.error(
                        f"diag-block: {rel}:{i}: "
                        f"no blank line allowed directly before `OWL_DIAG_POP`"
                    )
                    issues += 1
                # Next line must be blank (or end of file / closing brace).
                if (
                    i + 1 < len(code_lines)
                    and code_lines[i + 1].strip()
                    and not code_lines[i + 1].lstrip().startswith(("}", ")", ";"))
                ):
                    log.error(
                        f"diag-block: {rel}:{i + 2}: "
                        f"`OWL_DIAG_POP` must be followed by a blank line"
                    )
                    issues += 1

        # --- logging message format
        for m in _LOG_MACRO_RE.finditer(code):
            msg = m.group(1)
            if len(msg) < 16:
                continue  # tolerate short flags / one-word events
            # Skip banner/separator messages (mostly repeated punctuation
            # characters like "----------" or "===").
            non_alnum = sum(1 for c in msg if not c.isalnum() and c != " ")
            if len(msg) and non_alnum / len(msg) >= 0.6:
                continue
            # Strip format placeholders so the period check works.
            stripped_msg = re.sub(r"\{[^{}]*\}", "X", msg).rstrip()
            if not stripped_msg.endswith((".", "?", "!", ":")):
                ln = code.count("\n", 0, m.start()) + 1
                log.error(
                    f"log-msg: {rel}:{ln}: log message should end with `.`: "
                    f'"{msg[:80]}"'
                )
                issues += 1

    return issues


# Helper: strip `//` and `/* */` comments while keeping line counts intact.
def _strip_comments(text: str) -> str:
    """
    Replace every comment span with the same number of space characters
    (preserving newlines) so byte offsets / line numbers are unchanged.
    """
    out = list(text)
    n = len(text)
    i = 0
    while i < n:
        # `//` line comment
        if text[i:i + 2] == "//":
            j = text.find("\n", i)
            if j == -1:
                j = n
            for k in range(i, j):
                out[k] = " "
            i = j
        # `/* */` block comment
        elif text[i:i + 2] == "/*":
            j = text.find("*/", i + 2)
            if j == -1:
                j = n
            else:
                j += 2
            for k in range(i, j):
                if text[k] != "\n":
                    out[k] = " "
            i = j
        elif text[i] == '"':
            # Skip string literals so `//` / `/*` inside them aren't replaced.
            j = i + 1
            while j < n and text[j] != '"':
                if text[j] == "\\" and j + 1 < n:
                    j += 2
                else:
                    j += 1
            i = j + 1
        else:
            i += 1
    return "".join(out)


# ────────────────────────────────────────────────────────────────────────────
# Sub-check 6 — structural audit (file headers + OWL_API hint)
# ────────────────────────────────────────────────────────────────────────────


_FILE_HEADER_RE = re.compile(r"^\s*\*?\s*@file\s+\S+", re.MULTILINE)
_COPYRIGHT_RE = re.compile(r"Copyright\s*\(c\)\s+\d{4}", re.IGNORECASE)
_FREE_FUNCTION_DECL_RE = re.compile(
    r"^(?:\[\[[^\]]+\]\]\s*)*(?:inline\s+|constexpr\s+|static\s+)*"
    r"(?:auto|void|bool|int|float|double|char|unsigned|signed|"
    r"std::\w+|owl::[\w:]+|shared|uniq|weak|[\w:]+<[^>]*>|[A-Z]\w*)"
    r"\s+\w+\s*\([^)]*\)\s*(?:->\s*[^;{]+)?\s*;\s*$"
)


def _check_structural() -> int:
    """
    File header + OWL_API audits. The header check is a hard error; the
    OWL_API check emits warnings only because the regex has false positives.
    """
    log.info("code-style: structural audit...")
    issues = 0

    for path in _iter_sources(SOURCE_ROOTS, CXX_EXTENSIONS):
        head = "\n".join(path.read_text(errors="replace").splitlines()[:12])
        if not _FILE_HEADER_RE.search(head):
            log.error(f"file-header: {path.relative_to(root)}: missing `@file` tag")
            issues += 1
        if not _COPYRIGHT_RE.search(head):
            log.error(
                f"file-header: {path.relative_to(root)}: missing `Copyright (c) YYYY` line"
            )
            issues += 1

    # OWL_API is only mandatory for symbols declared in PUBLIC headers — those
    # are the ones consumed by external translation units (test binaries,
    # Owl Nest, downstream apps). Private headers are linked against from
    # within the same shared library and don't need the export tag.
    for path in _iter_sources(
        (root / "source" / "owl" / "public",),
        HEADER_EXTENSIONS,
    ):
        try:
            text = path.read_text(errors="replace")
        except OSError:
            continue
        # Strip comments + string literals so braces inside them don't break
        # the scope tracker.
        code = _strip_comments(text)
        lines = code.splitlines()

        # `brace_depth` is the total brace nesting; `class_open_depths`
        # records the brace depth at which each currently-open class/struct
        # body lives, so we can tell "am I inside a class body?" from "am I
        # in any `{}` block?". `pending_class` tracks `class Foo` / `struct
        # Foo : Base` declarations that span multiple lines so we register
        # the scope when its opening brace finally arrives.
        brace_depth = 0
        class_open_depths: list[int] = []
        pending_class = False

        # `class\s+\w+` (and `struct\s+\w+`) matches both single-line
        # (`class Foo {`) and multi-line (`class Foo\n: public Bar {`) forms;
        # we then look at whether the same line carries `{` or `;`.
        class_intro_re = re.compile(r"\b(?:class|struct)\s+\w[\w<>:]*")

        for line in lines:
            stripped = line.strip()

            # Open a class scope on the line that finally contains `{`.
            opened_here = False
            if class_intro_re.search(stripped) and "{" in stripped and not stripped.endswith(";"):
                class_open_depths.append(brace_depth)
                opened_here = True
                pending_class = False
            elif pending_class and "{" in stripped:
                class_open_depths.append(brace_depth)
                opened_here = True
                pending_class = False
            elif (
                class_intro_re.search(stripped)
                and "{" not in stripped
                and ";" not in stripped
            ):
                # Class header line, body opens later.
                pending_class = True

            opens = stripped.count("{")
            closes = stripped.count("}")
            new_depth = brace_depth + opens - closes
            while class_open_depths and class_open_depths[-1] >= new_depth:
                class_open_depths.pop()
            brace_depth = max(0, new_depth)
            _ = opened_here  # kept for readability above

            if class_open_depths:
                continue  # inside a class body — methods inherit OWL_API
            # Skip lines that live inside a function body (heuristic: brace
            # depth deeper than the namespace nesting). Free function decls
            # at namespace scope sit at depth 0 or 1; anything ≥ 2 is most
            # likely an in-body construct that happens to look like a decl.
            if brace_depth >= 2:
                continue
            if not stripped or stripped.startswith(("//", "/*", "*", "#")):
                continue
            if "OWL_API" in stripped:
                continue
            if _FREE_FUNCTION_DECL_RE.match(stripped):
                if "operator" in stripped or stripped.startswith(("using ", "typedef ")):
                    continue
                # `main` is the C++ program entry point — defined by the
                # application binary, not the engine DLL — so it never carries
                # `OWL_API`.
                if re.match(r"^\s*auto\s+main\s*\(", stripped):
                    continue
                # Warning only — heuristic is broad and we don't want to block
                # the build on every suspect signature.
                log.warning(
                    f"owl-api: {path.relative_to(root)}: free function may need "
                    f"`OWL_API`: {stripped[:80]}"
                )

    return issues


# ────────────────────────────────────────────────────────────────────────────
# Action entry point
# ────────────────────────────────────────────────────────────────────────────


class CodeStyle(BaseAction):
    """
    Aggregate code-style and doc-quality gate (read-only — never rewrites
    sources). See module docstring for the full check list.

    Extra args (passed after `--` on the command line):

        --no-format=true            skip clang-format dry-run
        --no-typos=true             skip codespell typo check
        --no-comment-quality=true   skip `///` / sentence-punctuation audit
        --no-doc-audit=true         skip private-member doc audit
        --no-cpp-style=true         skip cpp-style convention audit
        --no-structural=true        skip file-header / OWL_API audit
    """

    def run(
        self,
        preset: PresetConfig,
        extra_args: Optional[list[str]] = None,
    ) -> int:
        """
        Run every enabled sub-check sequentially and aggregate exit codes.
        :param preset: The preset to use (only the doc-build step needs the build dir).
        :param extra_args: Optional extra arguments (`--no-<check>=true`).
        :return: 0 when every enabled check passes, non-zero otherwise.
        """
        opts = self.parse_extra_args(extra_args)
        skip = {
            "format": opts.get("no-format", "false") == "true",
            "typos": opts.get("no-typos", "false") == "true",
            "comments": opts.get("no-comment-quality", "false") == "true",
            "doc-audit": opts.get("no-doc-audit", "false") == "true",
            "cpp-style": opts.get("no-cpp-style", "false") == "true",
            "structural": opts.get("no-structural", "false") == "true",
        }

        log.info(f"Running CodeStyle gate for preset: {preset.cmake_preset}")

        results: List[Tuple[str, int]] = []
        if not skip["format"]:
            results.append(("clang-format", _check_clang_format()))
        if not skip["typos"]:
            results.append(("typos (codespell)", _check_typos()))
        if not skip["comments"]:
            results.append(("comment-quality", _check_comment_quality()))
        if not skip["doc-audit"]:
            results.append(("private-member-docs", _check_private_member_docs()))
        if not skip["cpp-style"]:
            results.append(("cpp-style", _check_cpp_style()))
        if not skip["structural"]:
            results.append(("structural", _check_structural()))

        log.info("─" * 60)
        log.info("CodeStyle summary:")
        total = 0
        for name, code in results:
            status = "OK" if code == 0 else f"FAILED ({code} issue(s))"
            log.info(f"  • {name:<22} {status}")
            total += code

        if total == 0:
            log.info("CodeStyle gate passed — no issues.")
            return 0
        log.error(f"CodeStyle gate failed — {total} issue(s) total.")
        return total
