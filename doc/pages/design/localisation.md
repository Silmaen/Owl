# Localisation {#page-design-localisation}

[TOC]

A game ships in several languages, authored entirely in Owl Nest. User guide: [Languages](../languages.md).

## Parts

| Item            | What                                                                                   |
|-----------------|----------------------------------------------------------------------------------------|
| String tables   | Edited in a grid in Owl Nest: `localisation.yml`, `LocalisationDocument`               |
| Lua             | `tr()` returns the string of the current language; `locale` table                      |
| HUD             | Texts referenced by key: `UiText::textKey`                                             |
| Fonts           | One font per language, chosen for its glyph coverage: `Language::font`, packed with it |
| Language choice | The built-in `language` game setting, `locale.set_language`                            |
| Fallback        | A fallback language for missing keys, then the key                                     |
| Export          | Packs only the chosen languages: `Language::exported`, `withExportedLanguages()`       |

## Decisions

| Decision                       | Reason                                                                                       |
|--------------------------------|----------------------------------------------------------------------------------------------|
| One project file               | Languages, fallback and strings change together; string tables are groups of keys inside it  |
| Lookup through `EngineContext` | One index by key, rebuilt when the localisation changes: no linear search per text per frame |
| `Text` kept beside `textKey`   | The scene stays readable and the HUD editor shows something without a localisation           |

Translating the Owl Nest interface on the same string tables is to be evaluated after 1.0.
