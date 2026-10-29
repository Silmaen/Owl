# Localisation {#page-languages}

[TOC]

A game ships its texts in several languages, authored in Owl Nest. Plan and decisions:
[Localisation design](design/localisation.md).

## The localisation file

`localisation.yml`, at the root of the project, holds the languages of the game, the fallback language and the
strings, each in a string table (a group of keys: `menu`, `dialogue`...):

```yaml
Localisation: ''
FormatVersion: 1
fallback: en
languages:
  - {code: en, name: English, export: true}
  - {code: ja, name: 日本語, font: NotoSansJP, export: true}
strings:
  - key: menu.start
    table: menu
    values: {en: Start, ja: 開始}
```

- A text missing in a language shows its value in the fallback language, else its key.
- `font` names the font (`FontLibrary`) of the localised HUD texts in that language, chosen for its glyphs; without
  one, each text keeps its own font.
- The game export packs the languages marked `export` only, with their fonts.

## Editing in Owl Nest

**Home > Project > Localisation** (or a double-click on `localisation.yml`) opens the localisation document:

- **Languages**: name, font and **Export** of each one, the **Fallback** button, a code field to add one and a bin
  to remove one with its texts.
- **Strings**: one row per key, one column per language; a missing value is tinted and shows the fallback text as a
  hint. Filter by table, search keys and texts, add a string to the table shown. Click the handle of a row to
  select it, **Ctrl** and **Shift** to select many; **Ctrl+D** duplicates them, **Delete** removes them, **move to
  table** moves them into another table. The toolbar counts the missing texts of each language.
- Every edit is one undo step; **Save** updates the texts shown by the editor.

A `UI Text` takes a **Text Key** in the inspector, picked among the keys: the HUD then shows its translation, the
`Text` field staying the text written in the scene.

## Scripts and settings

```lua
ui.set_text(label, string.format(tr("settings.volume"), 80))   -- tr is locale.tr
locale.set_language("fr")                                      -- also kept in the `language` setting
for _, language in ipairs(locale.get_languages()) do log.info(language.name) end
```

The built-in setting `language` (`game_settings.yml` default, user override in `settings.yml`) chooses the language
at start; an unknown code falls back. Reference: [Lua API: Localisation](lua-api-locale.md).
