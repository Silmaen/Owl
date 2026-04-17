# Documentation Conventions

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

- Config template: `DoxyfileTemplate` (relative paths from build dir)
- INPUT includes: `source/owl/public`, `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, `doc/`
- Custom header with mermaid.js: `doc/header.html`
- Theme: doxygen-awesome with dark mode toggle
- Build: `cmake --build <build_dir> --target documentation`

## Roadmap (`doc/pages/roadmap.md`)

### Format

- Versions ordered newest first (upcoming at top, released at bottom)
- Released versions: `## v0.1.0 -- 2026-04-16` (with actual date)
- Upcoming versions: `## v0.2.0 -- Expected 2026-08-01`
- Each version has a **Goal** paragraph and categorized feature lists
- Feature status badges:
  - `![Done][done]` — completed and merged
  - `![In Progress][progress]` — currently being implemented
  - `![Planned][planned]` — planned but not started
- Badge references defined at bottom of file (shield.io URLs)
- Sub-items use indented bullet lists (4 spaces) with concrete details
- Update status from Planned → Done as features are completed

### When to Update

- When starting a new feature: mark as In Progress
- When completing a feature: mark as Done with brief implementation details
- When bumping a version: add the release date, create new Unreleased section
- When planning new features: add under the appropriate future version

## Changelog (`CHANGELOG.md`)

### Format

Follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/):

```markdown
## [Unreleased] (0.1.1-dev)

### Added
- New feature description

### Changed
- Modified behavior description

### Fixed
- Bug fix description

## [0.1.0] - 2026-04-16

### Added
- ...
```

### Categories (in order)

1. **Added** — new features
2. **Changed** — changes in existing functionality
3. **Deprecated** — soon-to-be removed features
4. **Removed** — removed features
5. **Fixed** — bug fixes
6. **Security** — vulnerability fixes

### Rules

- One bullet per change, concise but specific
- Reference component/file names for clarity (e.g., "`PackWriter` progress callback")
- Group related changes under a single bullet with sub-items if needed
- When releasing: rename `[Unreleased]` to `[X.Y.Z] - YYYY-MM-DD`, create new `[Unreleased]`

## GitHub Root Files

Keep these files at the repository root, synchronized with content:

| File                 | Purpose                              | Update Frequency  |
|----------------------|--------------------------------------|-------------------|
| `README.md`          | Project overview, badges, quick start | Each release      |
| `CHANGELOG.md`       | Version history                      | Every PR / feature |
| `CONTRIBUTING.md`    | Contributor guide                    | As conventions change |
| `CODE_OF_CONDUCT.md` | Community standards                  | Rarely            |
| `SECURITY.md`        | Vulnerability reporting policy       | As versions change |
| `LICENSE`            | MIT License                          | Never             |
