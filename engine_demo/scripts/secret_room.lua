-- Secret room of the farm cliff: its cells are covered by a plug of dirt tiles that look like the wall (non-collidable
-- copies in the decor layer), so the player walks into the hill. The first time the trigger (`Type: Target`) is entered,
-- the plug is replaced by the darkened cave tiles with `isometric.set_cell` (it works on any tilemap), the hidden coins and
-- heart (`SecretItem1..items`) appear, a banner tells the secret and `platformer_secret` keeps it open when the level is entered again from a save.
-- The rectangle is in tilemap cells (column, row from the top) of the entity tagged `map`, layer `layer`.

properties = {
    { name = "map",    type = "string", default = "ExtTilemap" },
    { name = "layer",  type = "float",  default = 1 },
    { name = "x0",     type = "float",  default = 0 },
    { name = "y0",     type = "float",  default = 0 },
    { name = "x1",     type = "float",  default = 0 },
    { name = "y1",     type = "float",  default = 0 },
    { name = "items",  type = "float",  default = 0 },
    { name = "tile_a", type = "float",  default = 237 },   -- darkened dirt of the cave: middle, left edge, right edge
    { name = "tile_b", type = "float",  default = 238 },
    { name = "tile_c", type = "float",  default = 239 },
}

local opened = false

local function reveal()
    opened = true
    local tilemap_id = scene.find_entity(map)
    if tilemap_id == 0 then return end
    for cy = y0, y1 do
        for cx = x0, x1 do
            local tile = tile_a
            if cx == x0 then tile = tile_b elseif cx == x1 then tile = tile_c end
            isometric.set_cell(tilemap_id, layer, cx, cy, tile, -1)
        end
    end
    -- The coins and the heart of the room (tagged SecretItem1..n) wait hidden until the wall opens.
    for i = 1, items do
        local item = scene.find_entity("SecretItem" .. i)
        if item ~= 0 then ui.set_visible(item, true) end
    end
end

function on_create()
    if gamestate.get("platformer_secret", 0) > 0 then reveal() end
end

function on_trigger_enter(player_id)
    if opened then return end
    reveal()
    gamestate.set("platformer_secret", 1)
    gamestate.set("platformer_banner", "Secret room found!")
    sound.play("sounds/victory.wav")
    log.info("Secret room opened")
end
