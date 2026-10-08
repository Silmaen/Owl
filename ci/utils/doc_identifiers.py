"""
Stale identifiers in the user documentation, for the `CodeStyle` gate: a symbol, a file or a CMake option cited
between backticks in `doc/pages` must exist in the repository, so a rename or a removal cannot leave the pages
describing code that is gone.

Only the inline code spans that look like an identifier are checked, fenced blocks are not:

* a path (`source/owl/public/scene/Scene.h`, `Renderer3D.cpp`, `engine_assets/`) must be a tracked file or folder,
  or the tail of one;
* an `OWL_*` name (CMake option, macro) and a C++ name (`Scene`, `Scene::onUpdate()`, `renderer::Camera`) must
  appear in the code; the namespaces of a qualified name must be declared;
* a Lua call (`scene.find_entity`) must name a word of the code (the bindings register their names as strings).

Words of the code are read from the sources, the tests, the assets, the build files and the CI. The changelog is
history, the roadmap and the design pages name code that does not exist yet: none of them is checked. `doc/audit`
lies outside `doc/pages`. `EXCEPTIONS` lists the few names that live outside the repository, each with its reason.
"""

from __future__ import annotations

import re
import subprocess
from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path

SKIPPED_PAGES: frozenset[str] = frozenset({"changelog.md", "roadmap.md"})
"""Pages left out: the changelog cites the names of their time, the roadmap those of the code to come."""

SKIPPED_FOLDERS: frozenset[str] = frozenset({"design"})
"""Folders of `doc/pages` left out: the design pages name the code they plan."""

EXTERNAL_NAMESPACES: frozenset[str] = frozenset({"std", "entt"})
"""Namespaces of the libraries the pages cite, declared outside the repository."""

_PLUGIN = "class of the teamcity-github plugin, another repository"
EXCEPTIONS: dict[str, str] = {
    "lib/": "folder of the installed package",
    "include/": "folder of the installed package",
    "origin/main": "branch of the remote",
    ".teamcity/target/generated-configs/": "written by Maven when the DSL is validated",
    "ImLightRig": "widget of ImGuizmo, a dependency",
    "TracyLua": "Tracy API, a dependency",
    "LockableBase": "Tracy API, a dependency",
    "DraftAwareBuildFilter": _PLUGIN,
    "DraftBuildQueueCleaner": _PLUGIN,
    "PullRequestEventListener": _PLUGIN,
    "BuildStatusCheckRunPublisher": _PLUGIN,
    "PrPromotionTagger": _PLUGIN,
    "SimplePageExtension": _PLUGIN,
    "checkRun.infraNeutral": "setting of the teamcity-github plugin, another repository",
}
"""Cited names that live outside the repository, with the reason why."""

_CODE_ROOTS: tuple[str, ...] = (
    "source/",
    "test/",
    "bench/",
    "engine_assets/",
    "sample_project/",
    "cmake/",
    "ci/",
    "conan/",
    "docker/",
    ".teamcity/",
    "CMakeLists.txt",
    "CMakePresets",
    "conanfile.py",
    "ci_action.py",
    "DoxyfileTemplate",
    "pyproject.toml",
    "gcovr.cfg",
)
"""Tracked paths whose words count as code."""

_TEXT_SUFFIXES: frozenset[str] = frozenset(
    {
        ".h", ".hpp", ".cpp", ".inl", ".py", ".cmake", ".txt", ".json", ".yml", ".yaml", ".lua", ".slang",
        ".kts", ".kt", ".toml", ".cfg", ".sh", ".in", ".owl", ".glsl", "",
    }
)  # fmt: skip
"""Suffixes of the files read for words (an empty suffix covers `DoxyfileTemplate`, `Dockerfile`)."""

_FILE_SUFFIXES: str = (
    r"h|hpp|cpp|inl|py|md|cmake|txt|yml|yaml|json|slang|lua|toml|kts|kt|xml|sh|svg|png|ttf|cfg|html|css|in|owl|lock"
)
"""Extensions that make a code span a file name."""

_PATH_RE = re.compile(r"^\.?[\w.-]+(?:/[\w.-]+)*/?$")
_FILE_RE = re.compile(rf"^[\w./-]*\.(?:{_FILE_SUFFIXES})$")
_OWL_RE = re.compile(r"^OWL_[A-Z0-9_]+$")
_CXX_RE = re.compile(r"^(?:[A-Za-z_]\w*::)+~?[A-Za-z_]\w*(?:\(\))?$|^[A-Z][a-z0-9]+(?:[A-Z][A-Za-z0-9]*)+(?:\(\))?$")
_CALL_RE = re.compile(r"^[a-z_]\w*(?:\.[a-z_]\w*)+(?:\(\))?$|^[a-z]\w*_\w+\(\)$|^[a-z]+[A-Z]\w*\(\)$")
_WORD_RE = re.compile(r"[A-Za-z_]\w*")
_NAME_RE = re.compile(r"[\w.-]+(?:/[\w.-]+)+/?|[\w-]+(?:\.[\w-]+)+")
_PREFIXED_RE = re.compile(r"\$\{PROJECT_PREFIX\}_(\w+)")
_BARE_SUFFIX_RE = re.compile(r"^\.\w+$")
_VERSION_RE = re.compile(r"/\d+\.\d+")
_SPAN_RE = re.compile(r"(?<!`)`([^`\n]+)`(?!`)")
_FENCE_RE = re.compile(r"^\s*(```|~~~)")
_NAMESPACE_RE = re.compile(r"^\s*(?:inline\s+)?namespace\s+([A-Za-z_][\w:]*)\s*\{", re.MULTILINE)


@dataclass(frozen=True)
class StaleIdentifier:
    """A code span of a documentation page that names nothing of the repository."""

    path: Path
    line: int
    column: int
    span: str
    kind: str


@dataclass(frozen=True)
class CodeIndex:
    """What the documentation may cite: tracked paths, words, file names and declared namespaces of the code."""

    paths: frozenset[str]
    words: frozenset[str]
    names: frozenset[str]
    namespaces: frozenset[str]

    def has_path(self, cited: str) -> bool:
        """
        Tell whether a cited path is a tracked file or folder, or the tail of one.

        :param cited: The path as the page writes it.
        :return: True when it exists.
        """
        name = cited.removeprefix("./").rstrip("/")
        if not name or name in self.paths or name in self.names or name + "/" in self.names:
            return True
        tail = "/" + name
        return any(path.endswith(tail) for path in self.paths) or any(
            known.rstrip("/").endswith(tail) for known in self.names
        )

    def has_namespace(self, parts: list[str]) -> bool:
        """
        Tell whether a namespace path is declared, possibly nested in `owl`.

        :param parts: Namespace components, outermost first.
        :return: True when `a::b` is declared or ends a declared namespace.
        """
        name = "::".join(parts)
        return any(ns == name or ns.endswith("::" + name) for ns in self.namespaces)


def tracked_files(repo: Path) -> list[str]:
    """
    Every file git tracks, repo-relative.

    :param repo: The repository root.
    :return: The POSIX paths.
    """
    result = subprocess.run(["git", "ls-files"], cwd=repo, capture_output=True, text=True, check=True)
    return [line for line in result.stdout.splitlines() if line]


def build_index(repo: Path, files: Iterable[str]) -> CodeIndex:
    """
    Index the paths, the words, the file names and the namespaces of the repository.

    A file name the code writes (`runner.yml`, `assets/help/`) counts, so the pages may cite what the engine
    creates at run time; an option declared as `${PROJECT_PREFIX}_NAME` counts as `OWL_NAME`.

    :param repo: The repository root.
    :param files: The tracked files, repo-relative.
    :return: The index.
    """
    paths: set[str] = set()
    words: set[str] = set()
    names: set[str] = set()
    namespaces: set[str] = set()
    for rel in files:
        paths.add(rel)
        parent = Path(rel).parent
        while parent != Path("."):
            paths.add(parent.as_posix())
            parent = parent.parent
        if not rel.startswith(_CODE_ROOTS) or Path(rel).suffix not in _TEXT_SUFFIXES:
            continue
        try:
            text = (repo / rel).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        words.update(_WORD_RE.findall(text))
        words.update("OWL_" + name for name in _PREFIXED_RE.findall(text))
        names.update(_NAME_RE.findall(text))
        if rel.endswith((".h", ".hpp", ".cpp", ".inl")):
            namespaces.update(_NAMESPACE_RE.findall(text))
    return CodeIndex(frozenset(paths), frozenset(words), frozenset(names), frozenset(namespaces))


def classify(span: str) -> str | None:
    """
    Tell what kind of identifier a code span is.

    :param span: The text between the backticks.
    :return: `path`, `option`, `symbol` or `call`, or None when it is no identifier (a value, a command, a key).
    """
    if any(c in span for c in " <>*{}$~=,'\"") and not span.startswith("~"):
        return None
    if span.startswith(("/", "-", "http")) or _BARE_SUFFIX_RE.match(span) or _VERSION_RE.search(span):
        return None
    if _FILE_RE.match(span) or ("/" in span and _PATH_RE.match(span)):
        return "path"
    if _OWL_RE.match(span):
        return "option"
    if _CXX_RE.match(span):
        return "symbol"
    if _CALL_RE.match(span):
        return "call"
    return None


def check_span(span: str, kind: str, index: CodeIndex) -> bool:
    """
    Tell whether a classified code span names something of the repository.

    :param span: The text between the backticks.
    :param kind: Its kind, from `classify`.
    :param index: The repository index.
    :return: True when it exists.
    """
    if kind == "path":
        return index.has_path(span)
    name = span.removesuffix("()")
    if kind == "option":
        return name in index.words
    if kind == "call":
        return all(part in index.words for part in name.split("."))
    parts = name.lstrip("~").split("::")
    if parts[0] in EXTERNAL_NAMESPACES:
        return True
    if not all(part.lstrip("~") in index.words for part in parts):
        return False
    scope: list[str] = []
    for part in parts[:-1]:
        if not part.islower():
            break
        scope.append(part)
    return not scope or index.has_namespace(scope)


def code_spans(text: str) -> Iterable[tuple[int, int, str]]:
    """
    The inline code spans of a Markdown page, outside fenced blocks.

    :param text: The page.
    :return: (line, column, span) triples, 1-based.
    """
    fenced = False
    for line_no, line in enumerate(text.splitlines(), start=1):
        if _FENCE_RE.match(line):
            fenced = not fenced
            continue
        if fenced:
            continue
        for match in _SPAN_RE.finditer(line):
            yield line_no, match.start(1) + 1, match.group(1).strip()


def doc_pages(repo: Path) -> list[Path]:
    """
    The documentation pages to check.

    :param repo: The repository root.
    :return: The Markdown files of `doc/pages`, without the changelog and the design pages.
    """
    pages = repo / "doc" / "pages"
    return sorted(
        page
        for page in pages.rglob("*.md")
        if page.name not in SKIPPED_PAGES and not SKIPPED_FOLDERS.intersection(page.relative_to(pages).parts[:-1])
    )


def stale_identifiers(pages: Iterable[Path], index: CodeIndex) -> list[StaleIdentifier]:
    """
    Find the cited identifiers that name nothing of the repository.

    :param pages: The Markdown pages to read.
    :param index: The repository index.
    :return: The stale citations, page by page.
    """
    found: list[StaleIdentifier] = []
    for page in pages:
        for line, column, span in code_spans(page.read_text(encoding="utf-8")):
            if span in EXCEPTIONS:
                continue
            kind = classify(span)
            if kind is not None and not check_span(span, kind, index):
                found.append(StaleIdentifier(page, line, column, span, kind))
    return found
