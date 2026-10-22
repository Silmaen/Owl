-- The warehouse corridor at the bottom of the Sokoban menus (script of the `Corridor` entity, its children are the
-- tiles). On the title the hero (`MenuHero`) walks in from the left and pushes a crate (`MenuCrate`) tile by tile onto
-- the middle goal, where it turns gold; then both go back off screen and start again. Other menus have no hero: the
-- corridor stays still. Nothing here reads the input.

properties = {
    { name = "frame_idle", type = "int", default = 8, min = 0 },
    { name = "frame_walk", type = "int", default = 9, min = 0 },
    { name = "walk_frames", type = "int", default = 4, min = 1 },
    { name = "frame_push", type = "int", default = 13, min = 0 },
    { name = "frame_box", type = "int", default = 5, min = 0 },
    { name = "frame_box_on_goal", type = "int", default = 6, min = 0 },
    { name = "step_time", type = "float", default = 0.42, min = 0.01 },
    { name = "slide_time", type = "float", default = 0.16, min = 0.01 },
    { name = "rest_time", type = "float", default = 2.5, min = 0 },
}

local TILE = 1.25
local FLOOR_Y = -6.9
local HERO_START, CRATE_START, GOAL = -18, -14, 0 -- in tiles

local hero, crate = 0, 0
local hero_tile, crate_tile = HERO_START, CRATE_START
local clock, walk_frame = 0.0, 0
local resting = 0.0

local function place(id, tile, from, t, z)
    local x = (from + (tile - from) * t) * TILE
    transform.set_position(id, x, FLOOR_Y, z)
end

local function reset()
    hero_tile, crate_tile = HERO_START, CRATE_START
    place(hero, hero_tile, hero_tile, 1, 0.06)
    place(crate, crate_tile, crate_tile, 1, 0.05)
    isometric.set_sprite_frame(crate, frame_box)
    isometric.set_sprite_frame(hero, frame_idle)
    clock = 0.0
end

function on_create()
    hero = scene.find_entity("MenuHero")
    crate = scene.find_entity("MenuCrate")
    if hero ~= 0 and crate ~= 0 then reset() end
end

function on_update(dt)
    if hero == 0 or crate == 0 then return end
    if resting > 0 then
        resting = resting - dt
        if resting <= 0 then reset() end
        return
    end
    clock = clock + dt
    local pushing = hero_tile + 1 == crate_tile
    local t = math.min(1.0, clock / slide_time)
    place(hero, hero_tile + 1, hero_tile, t, 0.06)
    if pushing then
        place(crate, crate_tile + 1, crate_tile, t, 0.05)
        isometric.set_sprite_frame(hero, frame_push)
    else
        isometric.set_sprite_frame(hero, frame_walk + walk_frame)
    end
    if clock >= step_time then
        clock = 0.0
        walk_frame = (walk_frame + 1) % walk_frames
        hero_tile = hero_tile + 1
        if pushing then crate_tile = crate_tile + 1 end
        if crate_tile == GOAL then
            isometric.set_sprite_frame(crate, frame_box_on_goal)
            isometric.set_sprite_frame(hero, frame_idle)
            place(hero, hero_tile, hero_tile, 1, 0.06)
            place(crate, crate_tile, crate_tile, 1, 0.05)
            resting = rest_time
        end
    end
end
