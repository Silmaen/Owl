-- Lever of the hub: pulling it (E) opens the gate of the chest alcove.
-- Demonstrates: the `EntityLink` component read from a script (`entity.get_link`): the lever does not know the gate by
--               name, it asks the link of its `LeverLink` entity, which the inspector edits like any other component
--               (a hidden entity holds it: a visible link also copies the position of its target). Also
--               `scene.destroy_entity`, `transform.set_rotation` (the lever tilts), `isometric.set_sprite_frame`
--               (the chest opens), gamestate (`gate_open` survives a visit of a level), a toast.
--
-- The state is `gate_open` in gamestate; the scenario test reads it.

local TOAST_SECONDS = 2.5
local CHEST_OPEN = 151
local PULLED_ANGLE = -0.9

local link_id, chest_id, toast_id = 0, 0, 0
local toast_left = 0
local pulled = false

local function toast(text)
    ui.set_text(toast_id, text)
    ui.set_visible(toast_id, true)
    toast_left = TOAST_SECONDS
end

local function open_gate(target)
    pulled = true
    scene.destroy_entity(target)
    transform.set_rotation(entity_id, 0, 0, PULLED_ANGLE)
    if chest_id ~= 0 then
        isometric.set_sprite_frame(chest_id, CHEST_OPEN)
    end
    gamestate.set("gate_open", 1)
end

function on_create()
    link_id = scene.find_entity("LeverLink")
    chest_id = scene.find_entity("GateChest")
    toast_id = ui.find("HubToast")
    if gamestate.get("gate_open", 0) > 0 then
        local target = entity.get_link(link_id)
        if target ~= 0 then
            open_gate(target)
        end
    else
        gamestate.set("gate_open", 0)
    end
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_lever_interact.
function on_lever_interact(player_id)
    if pulled then
        toast("The lever is already pulled.")
        return
    end
    local target = entity.get_link(link_id)
    if target == 0 then
        toast("The lever moves, nothing answers.")
        return
    end
    open_gate(target)
    toast("The gate swings open!")
end

function on_update(dt)
    if toast_left > 0 then
        toast_left = toast_left - dt
        if toast_left <= 0 then
            ui.set_visible(toast_id, false)
        end
    end
end
