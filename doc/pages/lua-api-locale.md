# Lua API: Localisation {#page-lua-api-locale}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `locale`

Texts of the game in its languages (`localisation.yml`).

| Function                            | Returns            | Description                                                                                                   |
|-------------------------------------|--------------------|---------------------------------------------------------------------------------------------------------------|
| `locale.tr(key: string)`            | `text: string`     | Text of a key in the language of the game, else in the fallback language, else the key; also the global `tr`. |
| `locale.get_language()`             | `code: string`     | Code of the language of the texts (`en`), empty when the game has none.                                       |
| `locale.set_language(code: string)` |                    | Show the texts in a language (an unknown one falls back) and keep it in the `language` setting.               |
| `locale.get_languages()`            | `languages: table` | Languages of the game, as a list of `{code, name}`.                                                           |
