# Content Browser {#page-editor-assets}

[TOC]

The **Content Browser** (bottom strip) shows the files of the asset folders: the project, the application and the
engine. It opens them, places them in the scene and takes files dropped from the desktop.

## Browsing

- **Folder tree** (left, resizable): the asset roots and their folders, unfolded down to the folder shown.
- **Toolbar**: parent folder, a clickable path, a name filter, grid or list, the thumbnail size and the style of the
  mesh thumbnails (**Textured**, **Shaded**, **Flat**). Below, one chip per kind of file present (`All 14`,
  `Texture 11`...) filters the view.
- **Tiles**: images show themselves, tilesets their atlas, HUDs a render, scenes their primary camera, prefabs a
  framing of their entities, materials a sphere, meshes a render of their material, sounds their waveform; other
  files the icon of their kind. Each tile has a colour bar of its family, its kind and its extension. A preview is
  made again when its file changes; the status bar counts those still to make.

## Opening and placing

| Gesture                                         | Effect                                                                           |
|-------------------------------------------------|----------------------------------------------------------------------------------|
| Double-click a folder                           | Enter it                                                                         |
| Double-click a scene (`.owl`)                   | Open it in a scene tab                                                           |
| Double-click a prefab                           | Open it as a prefab document                                                     |
| Double-click a mesh                             | Place a Mesh Renderer entity at the origin                                       |
| Double-click a macro (`.owlmacro`)              | Replay it on the active scene ([macros](editor_console.md#macros))               |
| Double-click any other file                     | Open it in its [document](editor_documents.md)                                   |
| Drag a prefab, a mesh to the viewport           | Place it under the mouse ([dropping assets](editor_viewport.md#dropping-assets)) |
| Drag a material onto a mesh                     | Give every slot of the mesh the material                                         |
| Drag a texture, a sound, a script to Properties | Fill the matching field of the component                                         |
| Drop files from the desktop                     | Copy them into the folder shown                                                  |

A dragged tile follows the cursor and the places that accept it light up; a field that takes another kind of file
ignores it.

## Context menus

| Where          | Entries                                                                       |
|----------------|-------------------------------------------------------------------------------|
| Background     | **Create Folder**, **New Material**, **Import File...**, **Import Folder...** |
| File or folder | **Open**, **Rename**, **Delete**                                              |
| Texture        | **Import Settings** (selects it in the Import panel), **Cook**                |
| Folder         | **Cook Folder**                                                               |
| Mesh           | **Open** (the mesh viewer), **Place in Scene**, **Import Settings** (LODs)    |
| Macro          | **Edit as Text**                                                              |

**New Material** creates `New Material.owlmat` in the folder shown and opens it in the material editor.

## Asset identity and import settings {#editor-asset-import}

Every asset of the project has a `<file>.owlmeta` sidecar holding its GUID and, for a texture, its import settings
(see [Content pipeline](design/content-pipeline.md#content-pipeline-cook)). The browser hides the sidecars and
treats them as part of their file:

- **Rename**, drag to another folder and **Delete** run the `asset.rename`, `asset.move` and `asset.delete` commands:
  the sidecar follows the file, and every reference written by GUID keeps resolving.
- **Cook** and **Cook Folder** run `asset.cook`: the asset is cooked into the project cache now, as the export would.

The **Import** panel (docked with Properties) shows the selected asset:

| Field                     | Content                                                                              |
|---------------------------|--------------------------------------------------------------------------------------|
| File, importer, GUID      | Name, importer of the file, GUID with a copy button                                  |
| Pixel art                 | Nearest-texel filtering                                                              |
| Pack storage              | `None` (source file), `Uastc` (quality) or `Etc1s` (size): KTX2 Basis Universal      |
| Mipmaps                   | Mip chain built by the cook (compressed textures)                                    |
| Cache state, **Cook Now** | Whether the cooked result is in the cache for the current bytes and settings         |

Each setting has a reset button; a change is one undo step (`asset.set_import`). **Apply to Folder** gives the
settings to every texture of the folder shown.

## Hot reload {#editor-hot-reload}

Owl Nest watches the asset folders and reloads a file changed on disk at the start of the next frame, in Edit and in
Play:

| File                         | What reloads                                                                             |
|------------------------------|------------------------------------------------------------------------------------------|
| `.png`, `.jpg`               | Every texture read from it (its size may change)                                         |
| `.slang`                     | Every shader compiled from it, on Vulkan and OpenGL                                      |
| `.owltileset`, `.owltilemap` | The tilemaps, doors, push walls and voxel worlds using it, edited and played             |
| `.lua`                       | Every running instance of the script, see [Scripting](scripting.md#scripting-hot-reload) |
| `.owl`                       | The scene tab of that file, when it is not playing and has no unsaved edit               |

- A failed reload keeps the previous version (pixels, shader, running script) and logs the file and the error.
- A scene with unsaved edits is never replaced: a warning says the file changed. The editor's own saves reload
  nothing.
- Not reloaded: the colliders built at Play start (tilemap collision flags apply at the next Play), new shader
  bindings (restart), C++ game code.
- **Engine parameters > Debug > Hot Reload** turns the watch off.
