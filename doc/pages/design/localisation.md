# Localisation {#page-design-localisation}

[TOC]

A game ships in several languages, authored entirely in Owl Nest. Shipped in v0.6.0, summarised in the
[Roadmap](../roadmap.md); user guide: [Languages](../languages.md).

## Plan

| Item            | Plan                                                 | Status                                                       |
|-----------------|------------------------------------------------------|--------------------------------------------------------------|
| String tables   | Edited in a grid in Owl Nest                         | Done: `localisation.yml`, `LocalisationDocument`             |
| Lua             | `tr()` returns the string of the current language    | Done: global `tr`, `locale` table                            |
| HUD             | Texts referenced by key                              | Done: `UiText::textKey`                                      |
| Fonts           | One font per language, chosen for its glyph coverage | Done: `Language::font`, packed with the language             |
| Language choice | In the game settings                                 | Done: built-in `language` setting, `locale.set_language`     |
| Fallback        | A fallback language for missing keys                 | Done: fallback language, then the key                        |
| Export          | Packs only the chosen languages                      | Done: `Language::exported`, `withExportedLanguages()`        |

## Decisions

| Decision                      | Reason                                                                                     |
|-------------------------------|--------------------------------------------------------------------------------------------|
| One project file              | Languages, fallback and strings change together; string tables are groups of keys inside it |
| Lookup through `EngineContext` | One index by key, rebuilt when the localisation changes: no linear search per text per frame |
| `Text` kept beside `textKey`  | The scene stays readable and the HUD editor shows something without a localisation          |

The same string tables later translate the Owl Nest interface itself, after 1.0.
