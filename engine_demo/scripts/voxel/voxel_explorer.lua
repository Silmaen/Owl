-- Voxel terrain level: explore the procedural terrain.
-- Demonstrates: a script on the entity of a `VoxelPlayer` (transform.get_position follows the player), a HUD of its
--               own (huds/voxel.owlhud), a victory objective stored in gamestate (`level_voxel_done`), scene.transition_to.
--
-- Objective: walk target_distance blocks away from the landing point (horizontal distance, the terrain streams in as you go).
-- Escape goes back to the hub without completing the level.

properties = {
    { name = "target_distance", type = "int", default = 100, min = 1 },
    { name = "landing_point", type = "vec2", default = { 8, 8 } },  -- landing point (x, z): the player spawns above it
}


local objective_id = 0
local leaving = false
local shown = -1

function on_create()
    objective_id = ui.find("ObjectiveText")
    ui.transition_fade_in(0.4)
end

function on_update(dt)
    if leaving or ui.is_transition_active() then
        return
    end
    if input.is_action_just_pressed("pause") then
        leaving = true
        scene.transition_to("scenes/world_map.owl", "fade_out", 0.3)
        return
    end
    local x, _, z = transform.get_position(entity_id)
    local distance = math.floor(math.sqrt((x - landing_point.x) ^ 2 + (z - landing_point.y) ^ 2))
    gamestate.set("voxel_distance", distance)
    if distance ~= shown then
        shown = distance
        ui.set_text(objective_id, "Objective: explore the terrain, " .. math.min(distance, target_distance) .. " / " ..
            target_distance .. " blocks from the landing point")
    end
    if distance >= target_distance then
        leaving = true
        gamestate.set("level_voxel_done", 1)
        sound.play("sounds/ui/victory.wav")
        scene.transition_to("scenes/world_map.owl", "fade_out", 0.4)
    end
end
