# Localisation {#page-design-localisation}

[TOC]

A game ships in several languages, authored entirely in Owl Nest. Planned for v0.6.0, summarised in the [Roadmap](../roadmap.md).

## Plan

| Item            | Plan                                                 |
|-----------------|------------------------------------------------------|
| String tables   | Edited in a grid in Owl Nest                         |
| Lua             | `tr()` returns the string of the current language    |
| HUD             | Texts referenced by key                              |
| Fonts           | One font per language, chosen for its glyph coverage |
| Language choice | In the game settings                                 |
| Fallback        | A fallback language for missing keys                 |
| Export          | Packs only the chosen languages                      |

The same string tables later translate the Owl Nest interface itself, after 1.0.
