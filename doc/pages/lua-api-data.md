# Lua API: Splines and data tables {#page-lua-api-data}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `spline`

Splines sampled by distance, and the entities following them.

| Function                                                                               | Returns                               | Description                                                                                                                                        |
|----------------------------------------------------------------------------------------|---------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------|
| `spline.get_length(entity_id: entity)`                                                 | `length: number`                      | Length of the entity's `Spline`, in the units of the entity; nothing when it has none.                                                             |
| `spline.sample(entity_id: entity, distance: number, [lane: string])`                   | `x: number`, `y: number`, `z: number` | World position at a distance along the entity's `Spline` (clamped when open, wrapped when closed), in a lane when named; nothing when it has none. |
| `spline.get_tangent(entity_id: entity, distance: number)`                              | `x: number`, `y: number`, `z: number` | World unit direction of the entity's `Spline` at a distance along it; nothing when it has none.                                                    |
| `spline.get_nearest(entity_id: entity, x: number, y: number, z: number)`               | `distance: number`                    | Distance along the entity's `Spline` of its point nearest to a world position; nothing when it has none.                                           |
| `spline.get_point_count(entity_id: entity)`                                            | `count: integer`                      | Number of control points of the entity's `Spline`; nothing when it has none.                                                                       |
| `spline.get_point(entity_id: entity, index: integer)`                                  | `x: number`, `y: number`, `z: number` | World position of a control point of the entity's `Spline`, from 1; nothing when it has no such point.                                             |
| `spline.set_point(entity_id: entity, index: integer, x: number, y: number, z: number)` | `done: boolean`                       | Move a control point of the entity's `Spline` (from 1) to a world position; false when it has no such point.                                       |
| `spline.get_distance(entity_id: entity)`                                               | `distance: number`                    | Distance along its spline of the entity's `SplineFollower`; nothing when it has none.                                                              |
| `spline.set_distance(entity_id: entity, distance: number)`                             | `done: boolean`                       | Put the entity's `SplineFollower` at a distance along its spline (placed at the next frame); false when it has none.                               |
| `spline.get_speed(entity_id: entity)`                                                  | `speed: number`                       | Speed of the entity's `SplineFollower`, in units of the spline per second; nothing when it has none.                                               |
| `spline.set_speed(entity_id: entity, speed: number)`                                   | `done: boolean`                       | Set the speed of the entity's `SplineFollower` (negative goes backwards); false when it has none.                                                  |
| `spline.set_lane(entity_id: entity, lane: string)`                                     | `done: boolean`                       | Move the entity's `SplineFollower` to a lane of its spline (empty: the curve itself); false when it has none.                                      |

## `datatable`

Typed records of the `.owltable` data tables, by row key and column.

| Function                                                        | Returns                        | Description                                                                                                                                                     |
|-----------------------------------------------------------------|--------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `datatable.get(table: string, row: string, column: string)`     | `value: any`                   | Cell of a `.owltable` by row key and column name: an integer, a number, a boolean or a string (an asset path, the row key of a reference); nothing when absent. |
| `datatable.get_row(table: string, row: string)`                 | `row: table`                   | Every cell of a row of a `.owltable`, by column name; nothing when it has no such row.                                                                          |
| `datatable.get_keys(table: string)`                             | `keys: table`                  | Keys of the rows of a `.owltable`, in order; nothing when it cannot be read.                                                                                    |
| `datatable.get_columns(table: string)`                          | `columns: table`               | Names of the columns of a `.owltable`, in order; nothing when it cannot be read.                                                                                |
| `datatable.get_count(table: string)`                            | `count: integer`               | Number of rows of a `.owltable`; nothing when it cannot be read.                                                                                                |
| `datatable.has_row(table: string, row: string)`                 | `found: boolean`               | Whether a `.owltable` has a row of this key.                                                                                                                    |
| `datatable.resolve(table: string, row: string, column: string)` | `table: string`, `key: string` | Follow a reference cell: the table it points into and the key of the row; nothing when the column is not a reference.                                           |
| `datatable.find(table: string, column: string, value: any)`     | `key: string`                  | Key of the first row of a `.owltable` whose cell in a column equals a value; nothing when none does.                                                            |
