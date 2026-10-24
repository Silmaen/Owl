# Engine for C++ Developers {#page-guide-engine-developer}

[TOC]

The pages for people who use or extend the engine in C++: the module layout, the renderers, the subsystems the
scripts sit on. Build the engine first ([Building from Source](building.md)); the API reference is the generated
Doxygen documentation of `source/owl/public/`.

## Start here

| Page                                     | What you learn                                                   |
|------------------------------------------|------------------------------------------------------------------|
| [Architecture Overview](architecture.md) | Modules, backends, applications, project system, shader pipeline |
| [Renderer](renderer.md)                  | Renderer stack, batches, cameras, shaders, textures, text        |
| [Scene and Components](scene.md)         | The ECS on EnTT: components, hierarchy, serialization            |

## Renderers

| Page                                    | What you learn                      |
|-----------------------------------------|-------------------------------------|
| [Voxel Engine](voxel.md)                | Chunks, meshing, streaming          |
| [Raycaster Method](raycaster_method.md) | The raycast wall and floor renderer |

## Subsystems

| Page                                      | What you learn                                            |
|-------------------------------------------|-----------------------------------------------------------|
| [Physics](physics.md)                     | Box2D integration                                         |
| [Sound](sound.md)                         | The OpenAL backend and the sound components               |
| [Events and Input](event_input.md)        | Event dispatch, input backends                            |
| [Lua Scripting](scripting.md)             | How the Lua state, the sandbox and the bindings are wired |
| [Node Graph](node_graph.md)               | The node canvas widget and its documents                  |
| [Profiling](profiling.md)                 | Profiler scopes, Tracy, benchmarks                        |
| [Owl Nest internals](editor_internals.md) | Editor structure, commands, sessions, MCP, scenarios      |

Changing the engine itself: [contributors](guide_contributor.md). Making a game with it: [game makers](guide_game_maker.md).
