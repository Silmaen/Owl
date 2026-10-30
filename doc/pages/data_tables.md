# Data tables {#page-data-tables}

[TOC]

A data table holds the typed records a game reads by key: the goods of a market, the items of a shop, the waves of a
level, the stats of the enemies. It is a `.owltable` asset, edited in a grid in Owl Nest and read from Lua. Plan and
decisions: [Large worlds](design/large-worlds.md#large-worlds-data-tables).

## The table file

A YAML file (format version 1): its columns, then one map per row, the key of the row and one value per column.

```yaml
DataTable: ""
FormatVersion: 1
columns:
  - {name: speed, type: Float}
  - {name: lane, type: Enum, choices: [inner, outer]}
  - {name: cargo, type: Reference, table: "gid:690ca68e9f195fec:tables/cargo.owltable"}
  - {name: mesh, type: Asset}
rows:
  - key: CartSlow
    speed: 0.6
    lane: inner
    cargo: grain
    mesh: "gid:04dd2abdfcec9e7a:meshes/tavern/barrel.glb"
```

| Type        | Cell                                             | Default       |
|-------------|--------------------------------------------------|---------------|
| `Integer`   | A whole number                                   | `0`           |
| `Float`     | A real number                                    | `0`           |
| `Bool`      | `true` or `false`                                | `false`       |
| `Text`      | A string                                         | empty         |
| `Enum`      | One of the `choices` of the column               | first choice  |
| `Asset`     | The path of an asset, stored as a GUID reference | empty         |
| `Reference` | The key of a row of the table named by `table`   | empty         |

- Every row has a key, unique in the table; `key` cannot name a column. A row or a column that breaks this rule is
  skipped on load, with a warning.
- Every cell has the type of its column: a value that does not fit becomes the default of the type, and changing the
  type of a column converts its cells (a `Float` becomes the nearest `Integer`, a number becomes its text...).
- A `Reference` column points into another table (or its own one when `table` is empty); a cell holds the key of a
  row there. Renaming a row does not rewrite the tables that reference it: the grid shows an unknown key in red.
- The `Asset` cells and the `table` of a reference are written with the GUID of the asset (`gid:`): moving or renaming
  the file in the Content Browser keeps them valid, like any other reference.

## Editing in Owl Nest {#data-tables-editor}

Create one with **New Data Table** in the Content Browser (a text column and a row to start from), open it with a
double-click. The tab is a grid: the keys on the left, one column per field.

- **Rows**: the toolbar adds a row after the selection, duplicates (`Ctrl+D`), moves (`Alt+Up` / `Alt+Down`) and
  removes (`Delete`) the selected rows. Click the handle of a row to select it, `Ctrl` and `Shift` select many,
  `Ctrl+A` selects the rows the search shows. Type a key and press Enter to rename the row.
- **Columns**: type a name, pick a type and press **Add a column**. The header of each column renames it (Enter),
  changes its type, its choices (Enum) or its table (Reference, picked from the tables of the project or dropped from
  the Content Browser), moves it left or right and removes it.
- **Cells**: numbers are dragged or typed, flags toggled, texts typed, an Enum and a Reference picked from a list,
  an asset typed or dropped from the Content Browser. A typed value becomes one edit when the field is left.
- Every gesture is a `table.*` command ([Console](editor_console.md)), one undo step of the tab. **Save** (`Ctrl+S`)
  writes the file; the game and the editor scripts read it again.

## Reading from Lua

```lua
local goods = "tables/goods.owltable"
local price = datatable.get(goods, "wheat", "price")        -- a number
local row = datatable.get_row(goods, "wheat")                -- row.price, row.stock...
for _, key in ipairs(datatable.get_keys(goods)) do ... end   -- rows in order
local market_table, market = datatable.resolve(goods, "wheat", "market")
local name = datatable.get(market_table, market, "name")     -- follow the reference
local cheap = datatable.find(goods, "price", 1.5)            -- key of the first row with that price
```

The tables are read once per engine context (from the pack in an exported game, from the asset folders in the
editor) and shared by the scenes. Reference: [Splines and data tables](lua-api-data.md).

## Export

A table named in a script (a quoted `.owltable` path) or in a string property of a script is packed with the game,
with the tables its `Reference` columns point into and the assets of its `Asset` cells.
