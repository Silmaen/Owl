---
paths:
  - "**/*.md"
  - "doc/**"
  - "Doxyfile*"
  - "DoxyfileTemplate"
---

# Documentation Conventions

See also `.claude/rules/ongoing-quality.md` for the cross-cutting "keep docs in sync with code"
policy that applies to every PR.

## Documentation Pages (`doc/pages/*.md`)

### Format

- First line: `# Title {#page-pagename}` (Doxygen anchor)
- Second line: `[TOC]` (generates table of contents)
- Use standard GitHub-flavored Markdown
- Tables must be **column-aligned in raw text**: every `|` separator must be at the same column
  position across all rows. Pad each cell with spaces so all columns have uniform width.
  The separator row (`|---|`) must match the exact column widths. This applies to ALL markdown
  files in the project (doc pages, README, CHANGELOG, CONTRIBUTING, sample_project/README).
- Use `[Link Text](other_page.md)` for cross-page links (INPUT_FILTER converts to `@ref` for Doxygen)
- Code blocks: use ` ```c++ ` for C++, ` ```lua ` for Lua, ` ```yaml ` for YAML, ` ```bash ` for shell

### Mermaid Diagrams

Use ` ```mermaid ` fenced blocks for architecture diagrams, flowcharts, sequence diagrams, and state diagrams.
These render natively on GitHub and are post-processed by mermaid.js in the Doxygen HTML output.

Prefer mermaid over ASCII art or external image files. Common diagram types:
- `flowchart TD/LR` — component relationships, data flow
- `sequenceDiagram` — interaction sequences (async flows, lifecycle)
- `stateDiagram-v2` — state machines (editor states, scene lifecycle)
- `classDiagram` — class relationships (sparingly)

### Existing Images

SVG diagrams in `doc/images/` are still valid. New diagrams should prefer mermaid inline.
If an SVG already exists, keep it alongside the mermaid version for Doxygen compatibility.

### Doxygen Configuration

- Config template: `DoxyfileTemplate`, configured into `<build>/Doxyfile` (paths from `@PROJECT_SOURCE_DIR@`)
- INPUT includes: `source/owl/public`, `README.md`, `CHANGELOG.md`, `ROADMAP.md`, `CONTRIBUTING.md`, `doc/`
- Public API only (`EXTRACT_ALL=NO`, `EXTRACT_PRIVATE=NO`, `OWL_API` predefined empty): with `WARN_AS_ERROR`, any
  undocumented public element, enum value or parameter fails the build
- Custom header with mermaid.js: `doc/header.html`
- Theme: doxygen-awesome with dark mode toggle
- Build: `cmake --build <build_dir> --target documentation` (the target exists when Doxygen is found;
  `OWL_ENABLE_DOCUMENTATION=ON` requires it)

## Roadmap and changelog: three levels

| Level   | Roadmap                                 | Changelog                                   | Updated               |
|---------|-----------------------------------------|---------------------------------------------|-----------------------|
| Glance  | `ROADMAP.md` (root): one row/version    | `CHANGELOG.md` (root): one row per release  | At release            |
| Concise | `doc/pages/roadmap.md` (`page-roadmap`) | `doc/pages/changelog.md` (`page-changelog`) | Every PR              |
| Detail  | `doc/pages/design/<topic>.md`           | — (detail goes to the work log / `TODO.md`) | With the feature work |

### Root files (`ROADMAP.md`, `CHANGELOG.md`)

- Ultra-short, readable at a glance: positioning in one sentence, then a table
  (roadmap: version / expected date / one-line theme / status; changelog: version / date / one-line summary)
- Each links to its `doc/pages/` counterpart; no detail is kept here

### Roadmap page (`doc/pages/roadmap.md`)

- Versions ordered newest first (upcoming at top, released at bottom)
- Released versions: `## v0.1.0 -- 2026-04-16` (with actual date); upcoming: `## v0.2.0 -- Expected 2026-08-01`
- Each upcoming version: a **Goal** of 2–3 lines, then **one line per item**, each with a badge and, when one
  exists, a link to its design page. No long sub-lists, no specifications
- Released sections are history: kept as shipped
- Feature status badges (references defined at the bottom of the file, shields.io URLs):
  - `![Done][done]` — completed and merged
  - `![In Progress][progress]` — currently being implemented
  - `![Planned][planned]` — planned but not started
  - `![To evaluate][evaluate]` — candidate, to be studied before committing to it
  - `![Ongoing][ongoing]` — cross-cutting effort maintained in every release (the *Ongoing* section)
- Exit criteria take precedence over dates

### Design pages (`doc/pages/design/*.md`)

- One page per work item (e.g. `foundations.md`, `owl-rhi.md`, `isometric.md`), anchor `{#page-design-<name>}`,
  `[TOC]`, relative `.md` links (`../roadmap.md`)
- They hold every specification removed from the roadmap; nothing is dropped when an item is condensed
- File names without dots (the help bundle derives page ids from the name up to its first dot) and distinct from
  the `doc/pages/*.md` names (the help bundle flattens them)

### When to update

- Starting a feature: badge to In Progress; completing it: badge to Done, a line in `doc/pages/changelog.md`
- Bumping a version: add the release date, create the new upcoming section, update both root tables
- Planning a feature: one line under the right version, detail in a design page

### Changelog page (`doc/pages/changelog.md`)

Follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), sections `## [Unreleased]` then
`## [X.Y.Z] - YYYY-MM-DD`. Categories, in order: **Added**, **Changed**, **Deprecated**, **Removed**,
**Fixed**, **Security**.

- **One line per change, one sentence**: what changes and what it brings. No nested sub-lists, no
  paragraph per feature; measurements and rejected attempts go to the work log, not the changelog
- Reference component/file names for clarity (e.g., "`PackWriter` progress callback")
- When releasing: rename `[Unreleased]` to `[X.Y.Z] - YYYY-MM-DD`, create a new `[Unreleased]`, and add the
  one-line summary row to the root `CHANGELOG.md`

## GitHub Root Files

Keep these files at the repository root, synchronized with content:

| File                 | Purpose                               | Update Frequency      |
|----------------------|---------------------------------------|-----------------------|
| `README.md`          | Project overview, badges, quick start | Each release          |
| `CHANGELOG.md`       | One-line-per-release summary          | Each release          |
| `ROADMAP.md`         | One-glance roadmap table              | Each release          |
| `CONTRIBUTING.md`    | Contributor guide                     | As conventions change |
| `CODE_OF_CONDUCT.md` | Community standards                   | Rarely                |
| `SECURITY.md`        | Vulnerability reporting policy        | As versions change    |
| `LICENSE`            | MIT License                           | Never                 |
