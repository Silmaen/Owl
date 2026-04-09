---
name: Use existing files instead of creating new ones
description: Always check for existing files before creating new documentation files
type: feedback
---

Always search for existing files before creating new ones, especially for documentation.

**Why:** User had an existing `doc/pages/roadmap.md` file. Creating a separate `roadmap_010.md` was wrong — the content should have gone into the existing file.

**How to apply:** Before creating any new doc/config file, glob for similar filenames. Prefer updating existing files over creating new ones.
