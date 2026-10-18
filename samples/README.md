# Sample games

Small but complete games made with Owl, one project folder each, beside the [Engine Demo](../engine_demo/README.md).
Open one from the **Sample games** list of the Owl Nest File screen, or with **Open project**. The plan and the rules
of the samples are in [Sample games](../doc/pages/design/sample-games.md).

- One self-contained project per folder (`owl_project.yml`, its own `name` and a title scene as `firstScene`): what a
  sample reuses from the Engine Demo is copied, never pointed at.
- Game rules in Lua only; CC0 or home-made art and sounds, the generators in `tools/samples/<game>/`.
- At least one scenario test per game, `test/unit/scenarios/<game>_*.owltest`; every sample is exported and played
  headless by `GameExportEndToEnd.EveryShippedProjectPlaysFromItsExportFolder`.
