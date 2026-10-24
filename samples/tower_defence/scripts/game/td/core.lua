-- The namespace of the Tower defence rules: one table per module of `scripts/game/td/`, filled by each module when
-- `defence.lua` requires it; the modules call each other through it at run time.

return {
    data = {},
    state = {},
    effects = {},
    briefing = {},
    ui = {},
    economy = {},
    camera = {},
    world = {},
    waves = {},
    visuals = {},
    enemies = {},
    towers = {},
    spells = {},
    wheel = {},
    input = {},
    tutorial = {},
}
