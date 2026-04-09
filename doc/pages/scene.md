# Scene and Component System {#scene}

[TOC]

This page documents the Entity-Component-System (ECS), scene lifecycle, component
reference, and serialization.

## Overview

Owl uses an ECS built on [EnTT](https://github.com/skypjack/entt). A `Scene` owns an
`entt::registry` containing all entities and their components. Entities are lightweight
handles (`Entity` wraps `entt::entity` + `Scene*`). Each entity automatically receives
five mandatory components: `ID`, `Tag`, `Transform`, `Visibility`, and `Hierarchy`.

Scenes are serialized to and from YAML files (`.owl` extension).

## Entity Lifecycle

### Creation

| Method                         | Description                                    |
|--------------------------------|------------------------------------------------|
| `Scene::createEntity(name)`    | Create entity with auto-generated UUID         |
| `Scene::createEntityWithUUID(uuid, name)` | Create with explicit UUID          |

### Destruction

| Method                                | Behavior                                          |
|---------------------------------------|---------------------------------------------------|
| `Scene::destroyEntity(entity)`        | Children reparented to grandparent; world position preserved |
| `Scene::destroyEntityWithChildren(entity)` | Cascade delete of entire subtree            |

### Component API

| Method                          | Description                               |
|---------------------------------|-------------------------------------------|
| `entity.addComponent<T>(args)`  | Attach a new component                    |
| `entity.addOrReplaceComponent<T>(args)` | Attach or overwrite                 |
| `entity.getComponent<T>()`      | Get reference to component                |
| `entity.hasComponent<T>()`      | Check if component exists                 |
| `entity.removeComponent<T>()`   | Detach a component                        |

### Queries

| Method                        | Returns                                        |
|-------------------------------|------------------------------------------------|
| `getAllEntities()`            | All entities as `vector<Entity>`               |
| `getRootEntities()`          | Entities with `parentId == 0`                  |
| `getChildren(entity)`        | Direct children as `vector<Entity>`            |
| `findEntityByUUID(uuid)`     | Entity matching UUID, or null                  |
| `getEntityCount()`           | Total entity count                             |

### Duplication

| Method                     | Behavior                                                  |
|----------------------------|-----------------------------------------------------------|
| `duplicateEntity(entity)`  | Create root copy with new UUID, no children               |
| `duplicateSubtree(entity)` | Recursive duplicate with new UUIDs and correct hierarchy  |

## Scene Lifecycle

![Scene Lifecycle](../images/scene_lifecycle.svg)

### Status

| Status    | Description                          |
|-----------|--------------------------------------|
| `Editing` | Editor mode, no simulation           |
| `Playing` | Runtime: physics, scripts, triggers  |
| `Victory` | Victory overlay displayed            |
| `Death`   | Death overlay displayed              |

### Lifecycle Methods

| Method                       | When Called          | What It Does                                                |
|------------------------------|----------------------|-------------------------------------------------------------|
| `onStartRuntime()`           | Play pressed         | Initialize physics, start sounds with `playOnStart`, reset animated sprites |
| `onUpdateRuntime(timestep)`  | Each frame (Play)    | Scripts, input, physics, entity links, sounds, animated sprites, triggers, then render |
| `onRenderRuntime()`          | Each frame (Pause)   | Render only, no simulation                                  |
| `onUpdateEditor(timestep, camera)` | Each frame (Edit) | Render with editor camera                                 |
| `onEndRuntime()`             | Stop pressed         | Stop sounds, destroy physics                                |
| `onViewportResize(size)`     | Viewport resized     | Resize all cameras                                          |

## Component Reference {#components}

![Component Overview](../images/component_overview.svg)

### Mandatory Components

These are automatically added to every entity and cannot be removed.

#### ID

| Field | Type   | Description        |
|-------|--------|--------------------|
| `id`  | `UUID` | Unique identifier  |

Serialized directly as the entity key in YAML (`Entity: <uuid>`).

#### Tag

| Field | Type     | YAML Key | Description     |
|-------|----------|----------|-----------------|
| `tag` | `string` | `Tag`    | Display name    |

#### Transform

| Field       | Type              | YAML Key    | Description                         |
|-------------|-------------------|-------------|-------------------------------------|
| `transform` | `math::Transform` | `Transform` | Local position, rotation, and scale |

Stores the **local** transform relative to the parent. World transform is computed
on demand via `Scene::getWorldTransform(entity)`.

#### Visibility

| Field           | Type   | YAML Key     | Serialized | Description                     |
|-----------------|--------|--------------|------------|---------------------------------|
| `gameVisible`   | `bool` | `Visibility` | Yes        | Rendered during gameplay        |
| `editorVisible` | `bool` | —            | No         | Rendered in editor (always true on load) |

#### Hierarchy

| Field        | Type                | YAML Key    | Description                        |
|--------------|---------------------|-------------|------------------------------------|
| `parentId`   | `UUID`              | `Hierarchy` | Parent UUID (0 = root)             |
| `childrenIds`| `vector<UUID>`      | —           | Rebuilt from `parentId` after load |

### Rendering Components

#### SpriteRenderer

| Field         | Type             | Default         | Description              |
|---------------|------------------|-----------------|--------------------------|
| `color`       | `math::vec4`     | `{1, 1, 1, 1}` | Tint color               |
| `texture`     | `Texture2D`      | `nullptr`       | Sprite texture           |
| `tilingFactor`| `float`          | `1.0`           | Texture repetition       |

YAML key: `SpriteRenderer`

#### AnimatedSpriteRenderer

| Field          | Type       | Default  | Description                     |
|----------------|------------|----------|---------------------------------|
| `color`        | `vec4`     | white    | Tint color                      |
| `texture`      | `Texture2D`| null    | Spritesheet                     |
| `columns`      | `uint32_t` | `1`      | Grid columns                    |
| `rows`         | `uint32_t` | `1`      | Grid rows                       |
| `firstFrame`   | `uint32_t` | `0`      | Start frame                     |
| `lastFrame`    | `uint32_t` | `0`      | End frame                       |
| `frameDuration`| `float`    | `0.1`    | Seconds per frame               |
| `loop`         | `bool`     | `true`   | Loop animation                  |

YAML key: `AnimatedSpriteRenderer`.
See [Renderer > Animated Sprites](@ref renderer) for UV computation details.

#### CircleRenderer

| Field       | Type         | Default         | Description          |
|-------------|--------------|-----------------|----------------------|
| `color`     | `math::vec4` | `{1, 1, 1, 1}` | Circle color         |
| `thickness` | `float`      | `1.0`           | Ring thickness (0–1) |
| `fade`      | `float`      | `0.005`         | Edge fade amount     |

YAML key: `CircleRenderer`

#### Text

| Field         | Type     | Default         | Description          |
|---------------|----------|-----------------|----------------------|
| `text`        | `string` | `""`            | Text content         |
| `font`        | `Font`   | default font    | MSDF font            |
| `color`       | `vec4`   | `{1, 1, 1, 1}` | Text color           |
| `kerning`     | `float`  | `0.0`           | Extra letter spacing |
| `lineSpacing` | `float`  | `0.0`           | Extra line spacing   |

YAML key: `TextRenderer`

#### BackgroundTexture

| Field      | Type      | Default            | Description                    |
|------------|-----------|--------------------|--------------------------------|
| `mode`     | `Mode`    | `Background`       | `Background` or `Skybox`       |
| `type`     | `Type`    | `SolidColor`       | `SolidColor`, `Gradient`, `Texture` |
| `color`    | `vec4`    | `{0.2, 0.3, 0.8, 1}` | Main / bottom color         |
| `topColor` | `vec4`    | `{0.8, 0.9, 1, 1}` | Top color (gradient)          |
| `texture`  | `Texture2D`| `nullptr`         | Background or equirectangular  |

YAML key: `BackgroundTexture`. Only the first entity with this component is rendered.

### Camera Component

| Field            | Type          | Default | Description                   |
|------------------|---------------|---------|-------------------------------|
| `primary`        | `bool`        | `true`  | Active camera for rendering   |
| `fixedAspectRatio`| `bool`       | `false` | Lock aspect ratio             |
| `camera`         | `SceneCamera` | —       | Orthographic or perspective   |

YAML key: `Camera`. See [Renderer > Camera System](@ref renderer).

### Gameplay Components

#### Player

| Field           | Type    | Default | Description                     |
|-----------------|---------|---------|---------------------------------|
| `primary`       | `bool`  | `true`  | Active player                   |
| `linearImpulse` | `float` | `0.1`   | Horizontal movement force       |
| `jumpImpulse`   | `float` | `0.2`   | Vertical jump force             |
| `canJump`       | `bool`  | `true`  | Whether jumping is allowed      |

YAML key: `Player`. Input is parsed from keyboard each frame via `parseInputs()`.

#### PhysicBody

| Field          | Type       | Default     | Description                  |
|----------------|------------|-------------|------------------------------|
| `type`         | `BodyType` | `Dynamic`   | `Static`, `Dynamic`, `Kinematic` |
| `fixedRotation`| `bool`     | `false`     | Prevent body rotation        |
| `colliderSize` | `vec3f`    | `{1,1,1}`  | Box collider dimensions      |
| `density`      | `float`    | `1.0`       | Material density             |
| `restitution`  | `float`    | `0.0`       | Bounciness (0–1)             |
| `friction`     | `float`    | `0.5`       | Surface friction             |

YAML key: `PhysicBody`. See [Physics](@ref physics) for the full physics guide.

#### Trigger

| Field       | Type          | Default    | Description                         |
|-------------|---------------|------------|-------------------------------------|
| `type`      | `TriggerType` | `Victory`  | `Victory`, `Death`, `Target`, `Teleport` |
| `levelName` | `string`      | `""`       | Target scene (Teleport type)        |
| `targetName`| `string`      | `""`       | Target entity name in destination   |

YAML key: `Trigger`. Collision between the player and a trigger entity activates the
trigger effect: change scene status, or queue a teleport request.

#### EntityLink

| Field              | Type     | Default | Description                      |
|--------------------|----------|---------|----------------------------------|
| `linkedEntityName` | `string` | `""`    | Name of the entity to follow     |

YAML key: `EntityLink`. The linked entity's world position is copied to this entity
each frame, converting to local space when parented.

#### NativeScript

Enables C++ scripting via `ScriptableEntity`. Use `bind<T>()` to associate a script
class that implements `onCreate()`, `onUpdate(timestep)`, and `onDestroy()`.

YAML key: `NativeScript` (not typically serialized in scenes).

### Audio Components

See [Sound System](@ref sound) for the full audio guide.

- **SoundSource** (YAML: `SoundSource`): audio playback with 3D spatial support
- **SoundListener** (YAML: `SoundListener`): marks the entity as the audio "ear"

## Transform Hierarchy

![ECS Pipeline](../images/ecs_pipeline.svg)

Entities support parent-child relationships via the `Hierarchy` component.

### World Transform

Each entity's `Transform` stores a **local** transform. The world transform is computed
by walking the parent chain:

```
worldTransform = parentWorldTransform * localTransform
```

Root entities (`parentId == 0`) have local = world (no overhead).

### Visibility Inheritance

If any ancestor is hidden, the entity is effectively hidden.
`Scene::isEffectivelyVisible()` walks the parent chain to check.

### Hierarchy Operations

| Operation                | Behavior                                                                        |
|--------------------------|---------------------------------------------------------------------------------|
| **Set parent**           | Circular reference check, local transform recomputed to preserve world position |
| **Unparent**             | Entity becomes root, world transform stored as new local                        |
| **Delete entity**        | Children reparented to grandparent; world position preserved                    |
| **Delete with children** | Cascade delete of entire subtree                                                |
| **Duplicate entity**     | Duplicate is a root entity with no children                                     |
| **Duplicate subtree**    | Recursive duplicate with new UUIDs and correct parent references                |

### Physics and Hierarchy

Physics bodies (Box2D) operate in **world space** independently of the scene hierarchy.
See [Physics > Hierarchy Interaction](@ref physics) for details.

## Scene Serialization

`SceneSerializer` handles YAML I/O:

| Method                              | Description                       |
|-------------------------------------|-----------------------------------|
| `serialize(path)`                   | Write scene to `.owl` file        |
| `deserialize(path)`                | Read scene from file              |
| `deserializeFromBuffer(data, name)` | Read from memory (pack file)      |

### YAML Format

```yaml
Scene: My Scene
Entities:
  - Entity: 12345678
    Tag:
      tag: Platform
    Transform:
      translation: [0, -2, 0]
      rotation: [0, 0, 0]
      scale: [10, 0.5, 1]
    SpriteRenderer:
      color: [0.5, 0.5, 0.5, 1]
      tilingFactor: 1
      texture: nam:platform_texture
    PhysicBody:
      type: Static
      fixedRotation: false
      colliderSize: [1, 1, 1]
      density: 1
      restitution: 0
      friction: 0.5
    Hierarchy:
      parentId: 0
```

Components are serialized in the order defined by `SerializableComponents`. The
`Hierarchy` component only stores `parentId`; children lists are rebuilt after
deserialization via `Scene::rebuildHierarchyChildren()`.

## Teleport System

Cross-scene teleportation uses a `TeleportRequest` on the Scene:

1. Player collides with a Teleport trigger
2. `SceneTrigger::onTriggered()` fills the `TeleportRequest` with `levelName`, `targetName`, and player velocity
3. The editor/runner detects the request and loads the target scene
4. The next frame, the target scene starts runtime and applies the initial velocity to the player at the target entity position

The `levelName` field accepts `test_level`, `test_level.owl`, or `scenes/test_level.owl` —
the engine auto-appends `.owl` and searches asset directories including a `scenes/` subdirectory.

## Render Pipeline

`Scene::render()` iterates component groups in this order:

1. **BackgroundTexture** — first entity only
2. **SpriteRenderer** — `drawQuad()` with texture/color/tiling
3. **AnimatedSpriteRenderer** — `drawQuad()` with computed UV coordinates
4. **CircleRenderer** — `drawCircle()` with thickness/fade
5. **Text** — `drawString()` with font/color/kerning

Each entity is checked for visibility via `isEffectivelyVisible()` and rendered
using its world transform from `getWorldTransform()`.
