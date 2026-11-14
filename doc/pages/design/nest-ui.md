# Owl Nest UI {#page-design-nest-ui}

[TOC]

Design of the editor user interface: inspired by Unreal Engine's editor with a style of its own. The conventions are
written down in the [style guide](nest-style-guide.md); the user pages are under *The editor* in the help.

## Shell and look

- Owl amber is the only accent (blue kept for Play mode); two themes, Owl Dark and Owl Light, as `gui::Theme` tokens
  mapped onto ImGui; Inter with text roles; DPI scaling of the style and the fonts
- Default dock layout built on the first launch (`DockLayout`), presets Default and Hierarchy Left, Reset Layout, a
  status bar; panels on the right by default with an optional left column; document-bound panels shown only with
  their document
- Icons: a Lucide font for action glyphs; hand-drawn duotone SVGs for asset kinds and components (components carry one
  piece in the theme accent), sources in `source/owlnest/assets_sources/icons/`, rasterised at the size they are drawn
- Widget kit in `gui/widgets/`: property grid, vector field, section header, icon button, rich tooltip, search field,
  empty state, badge, chip and toasts
- Ribbon: File screen, Home / View / document tabs, quick access, collapse, one large button per group
- Viewport overlays: tools, playback, display toggles, frame-time chip and axis gizmo float over the view; a
  play-colour frame while playing
- Panels: label-left inspector, Hierarchy search / add / icons, Content Browser breadcrumb / search / views /
  thumbnails, Log chips, empty states, themed node canvas
- Assistance: command palette on a double `Shift` and in the ribbon search box (actions, entities, assets), inline
  notices for an incomplete component, toasts on save, `F1` contextual help

## Interaction basics

- Tooltips on every button, field and icon
- Context menus on the Hierarchy, the Content Browser, the viewport selection, the document tabs and the node graphs
- One drag & drop convention (`dragPreview` and drop highlight): assets onto viewport / inspector fields / hierarchy,
  entities onto entity fields, files from the OS file manager into the Content Browser
- DPI awareness and a text scale applied at once (`UiLayer::setTextScale`, 75 to 150 %)
- Asset thumbnails (`AssetThumbnails`) for images, tilesets, scenes, prefabs, meshes and sounds, rendered in the
  background and cached; atlas swatches in the asset fields of the inspector, thumbnails filling their whole tile

## Editor camera

- DCC-style navigation shared by every scene viewport: Alt+LMB rotate-in-place, Alt+RMB pan, Alt+MMB dolly; Ctrl+LMB
  orbit, Ctrl+MMB/RMB pan, wheel zoom
- `CameraEditor` builds its orientation from an Euler quaternion, so a full 360° turn works and the view never shears
- Orbit, pan and zoom speeds, vertical inversion and a dead zone under `Settings > Editor > Camera`
  (`CameraEditor::Navigation`)
- View-cube: the corner gizmo shows the six axis ends; a click on one looks from that side, a click on the centre
  resets the view
- Presets in the ribbon `View > Camera` group, the viewport overlay menu and the command palette: **Reset View**
  (`Home`), the six axis views (`CameraEditor::viewAlong`; Front is the XY plane, Top XZ, Right YZ, orthographic in the
  2D view), **Frame Selection** (`F`), **Frame Scene** (`Shift+F`)
- Go to camera viewpoint: the editor camera moves onto the selected `component::Camera` entity
  (`CameraEditor::lookFrom`; an orthographic camera gives the 2D view at its size); disabled for a camera of a
  `RendererRaycast` layer, whose scenes the editor renders flat

## Native file dialogues

nativefiledialog-extended is built with its `portal=True` option (`conan/recipes/nativefiledialog-extended`):
dialogues go through `xdg-desktop-portal` over D-Bus, without GTK (whose start-up freezes the UI), with the desktop's
own look. D-Bus is a dependency of the Linux packages; `FileDialog::openFile` / `saveFile` / `pickFolder` hide it.

## In-editor documentation {#in-editor-documentation}

The help panel is the user help of the editor.

- `doc/help.yml` lists the pages by group in a fixed order; `cmake/HelpAssets.cmake` writes them in that order with
  their group into `index.yml`, the panel draws the groups as they come and starts on the first page (`nest_help.md`);
  Doxygen's tree follows the same list (`fix_md_links.py`)
- Design pages stay out of the help (`design/plans.md` for Doxygen); links outside the bundle and badges become text
  (`tools/doc/help_bundle.py`)
- The build bundle wins over any `help/` of an asset folder; the configure deletes a stale `engine_assets/help`
  (`HelpPanel.BuildBundleWinsOverAStaleAssetHelp`)
- Lists with hanging indent and aligned numbers, the ordered-list start number from the parser, GFM task lists as
  checkboxes, inline code as a flat tinted span; the help content does not follow the light theme

Mermaid diagrams:

- `HelpAssets.cmake` runs `tools/doc/mermaid_svg.py bundle` (Poetry Python, standard library only) on the bundled
  pages: each mermaid fence becomes `![Flowchart](images/mermaid/<sha>.svg)`, the SVG files land in the build tree's
  `help_bundle/images/mermaid/`
- The renderer draws `flowchart` / `graph`, `stateDiagram-v2` (composite states as a group), `sequenceDiagram`
  (notes, loop / alt / opt / par blocks) and `classDiagram` as plain shapes and `<text>` (no `foreignObject`), layered
  layout, own dark background; syntax outside the subset fails the configure
- `MarkdownPreview` registers Inter with lunasvg (text needs a font face) and shows a rasterised SVG at its own size
- Tests: `HelpIndex.MermaidFencesAreReplacedByRenderedSvg` (no fence left, every referenced SVG exists),
  `tools/tests/test_mermaid_svg.py` (every documented diagram renders to well-formed SVG)
