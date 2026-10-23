-- The rules of a Tower defence map, on the `Game` entity of each map scene.
--
-- - The map gives its waves, its gold and its lives as properties (`waves`: one string per wave, groups
--   "kind count gap" separated by ";"); the pads are the overlay cells of `pad_tile` on the road layer of `Ground`,
--   the road is the path of the `Ground` graph from the `Gate` entity to the `Keep` entity (`path.find`). A new map is
--   a duplicated scene: paint the road and the pads, move the gate and the keep, edit the waves.
-- - The values of the towers and of the enemies are properties of this script, the same for every map unless a
--   map overrides one in the inspector.
-- - Waves are timed: a countdown before each one; calling it early (Space, pad Y, the HUD button) pays
--   `early_bonus` gold per second left. Every enemy reaching the keep costs a life.
-- - Enemies, towers, projectiles, health bars and puffs are prefabs spawned at runtime (`scene.spawn_prefab`).
-- - A click on a pad opens the build ring (three `BuildChoice` sprites over the pad, the range ring of the hovered
--   one, the info panel of the HUD); a click on a tower opens its ring with the sell badge; the right button, Backspace
--   or pad B closes it. The keyboard and the gamepad move a cursor from pad to pad (Q / E, Tab, the D-pad), Enter or
--   pad A opens the ring and builds, 1 to 3 pick a tower, X sells.
-- - F or pad RB doubles the speed (`time.set_scale`), Escape, P or Start pauses (`time.set_paused`): the pause window
--   is the only place that shows the controls during a map.
-- - The camera pans with WASD, the arrows, the left stick or the mouse on an edge of the view, and stays over the map.
-- - The state is mirrored in `gamestate` (`tower_gold`, `tower_lives`, `tower_wave`, ...) for the scenarios; the map
--   done is the `tower_done_NN` setting, the end stats go to the victory scene as `tower_result_*`.

properties = {
    { name = "map_number", type = "int", default = 1, min = 1 },
    { name = "scene_path", type = "string", default = "scenes/map_01.owl" },
    { name = "next_scene", type = "string", default = "scenes/victory.owl" },
    { name = "waves", type = "strings", default = { "runner 6 1.0" } },
    { name = "start_gold", type = "int", default = 220, min = 0 },
    { name = "lives", type = "int", default = 20, min = 1 },
    { name = "first_wave_delay", type = "float", default = 25.0, min = 0 },
    { name = "wave_delay", type = "float", default = 18.0, min = 0 },
    { name = "early_bonus", type = "float", default = 2.0, min = 0 },
    { name = "hp_growth", type = "float", default = 0.06, min = 0 },
    { name = "pad_tile", type = "int", default = 5 },
    { name = "road_layer", type = "int", default = 1, min = 0 },
    { name = "sell_share", type = "float", default = 0.5, min = 0, max = 1 },
    { name = "archer_cost", type = "int", default = 70 },
    { name = "archer_range", type = "float", default = 3.2 },
    { name = "archer_damage", type = "float", default = 9.0 },
    { name = "archer_reload", type = "float", default = 0.6 },
    { name = "cannon_cost", type = "int", default = 110 },
    { name = "cannon_range", type = "float", default = 2.8 },
    { name = "cannon_damage", type = "float", default = 24.0 },
    { name = "cannon_reload", type = "float", default = 1.7 },
    { name = "cannon_splash", type = "float", default = 1.1 },
    { name = "frost_cost", type = "int", default = 90 },
    { name = "frost_range", type = "float", default = 2.6 },
    { name = "frost_damage", type = "float", default = 3.0 },
    { name = "frost_reload", type = "float", default = 1.0 },
    { name = "frost_slow", type = "float", default = 0.5, min = 0.05, max = 1 },
    { name = "frost_time", type = "float", default = 1.8, min = 0 },
    { name = "runner_hp", type = "float", default = 28.0 },
    { name = "runner_speed", type = "float", default = 2.2 },
    { name = "runner_gold", type = "int", default = 7 },
    { name = "soldier_hp", type = "float", default = 70.0 },
    { name = "soldier_speed", type = "float", default = 1.3 },
    { name = "soldier_gold", type = "int", default = 12 },
    { name = "armoured_hp", type = "float", default = 140.0 },
    { name = "armoured_speed", type = "float", default = 0.85 },
    { name = "armoured_gold", type = "int", default = 22 },
    { name = "armoured_arrow", type = "float", default = 0.4, min = 0, max = 1 },
    { name = "arrow_speed", type = "float", default = 11.0, min = 0.1 },
    { name = "ball_flight", type = "float", default = 0.7, min = 0.1 },
    { name = "pan_speed", type = "float", default = 10.0, min = 0 },
    { name = "edge_pixels", type = "float", default = 14.0, min = 0 },
    { name = "camera_margin", type = "float", default = 6.0, min = 0 },
    { name = "end_delay", type = "float", default = 2.5, min = 0 },
    { name = "wipe_color", type = "color", default = { 0.1, 0.14, 0.24, 1 } },
}

-- What each tower kind is called, how its badge of `textures/icons.png` is numbered and how it reads on the HUD.
local KINDS = { "archer", "cannon", "frost" }
local NAMES = { archer = "Archer tower", cannon = "Cannon tower", frost = "Frost tower" }
local BLURBS = {
    archer = "Fast arrows at one enemy. Armour stops most of them.",
    cannon = "Slow shells bursting on a group, armour or not.",
    frost = "Chills an enemy: slower for a while, a little damage.",
}
local ICON = { archer = 0, cannon = 1, frost = 2, sell = 3 }
local ENEMY_ROW = { runner = 0, soldier = 1, armoured = 2 }
local PROJECTILE = { archer = "prefabs/projectile_arrow.owlprefab", cannon = "prefabs/projectile_ball.owlprefab",
                     frost = "prefabs/projectile_shard.owlprefab" }
local SHOT_SOUND = { archer = "sounds/game/arrow.wav", cannon = "sounds/game/cannon.wav",
                     frost = "sounds/game/frost.wav" }
local SQRT_HALF = math.sqrt(0.5)
local ENEMY_TOP = 3.0       -- height of a health bar over the foot of an enemy, in height units
local SHOT_HEIGHT = { archer = 3.2, cannon = 2.0, frost = 4.2 }

local map_id, camera_id, cursor_id, ring_id = 0, 0, 0, 0
local choice_ids = {}
local hud = {}
local shown = {}

local stats = {}            -- tower stats by kind, enemy stats by kind, read from the properties
local pads = {}             -- list of { x, y, tower }
local pad_at = {}           -- "x,y" -> pad
local pad_order = {}        -- pads from the left of the screen to the right
local route = nil           -- centres of the road cells, with the distance walked at each one
local route_request = 0

local enemies, towers, projectiles, puffs = {}, {}, {}, {}
local gold, lives_left = 0, 0
local wave, wave_count = 0, 0
local countdown, spawning = 0.0, nil    -- seconds before the next wave; the queue of the wave being sent
local spawn_clock = 0.0
local spawned = 0
local paused, fast = false, false
local ending, end_clock = nil, 0.0
local clock = 0.0
local message_clock = 0.0
local kills, leaks, built, earned, bonus_total = 0, 0, 0, 0, 0
local travel, paused_travel, travel_at_pause = 0.0, 0.0, nil
local last_shot_sound = {}

local ring = nil            -- { pad, choices = { {kind, id}, ... } } while the ring is open
local hover = 0             -- index of the highlighted choice
local cursor_pad = nil
local pad_mode = false
local last_mouse_x, last_mouse_y = nil, nil
local mouse_moved = false

-- Helpers -------------------------------------------------------------------------------------------------------------

local function set_text(key, text)
    if hud[key] ~= nil and hud[key] ~= 0 and shown[key] ~= text then
        ui.set_text(hud[key], text)
        shown[key] = text
    end
end

local function show(id, visible)
    if id ~= nil and id ~= 0 then ui.set_visible(id, visible) end
end

local function message(text, seconds)
    set_text("message", text)
    show(hud.message, true)
    message_clock = seconds or 2.0
end

local function play(path)
    sound.play(path)
end

local function key(x, y)
    return x .. "," .. y
end

local function cost_of(kind)
    return stats[kind].cost
end

local function refund_of(tower)
    return math.floor(stats[tower.kind].cost * sell_share + 0.5)
end

local function mirror()
    gamestate.set("tower_gold", gold)
    gamestate.set("tower_lives", math.max(0, lives_left))
    gamestate.set("tower_wave", wave)
    gamestate.set("tower_waves", wave_count)
    gamestate.set("tower_countdown", math.ceil(countdown))
    gamestate.set("tower_towers", #towers)
    gamestate.set("tower_kills", kills)
    gamestate.set("tower_leaks", leaks)
    gamestate.set("tower_spawned", spawned)
    gamestate.set("tower_alive", #enemies)
    gamestate.set("tower_ring", ring == nil and 0 or (ring.pad.tower ~= nil and 2 or 1))
    gamestate.set("tower_hover", hover)
    gamestate.set("tower_paused", paused)
    gamestate.set("tower_speed", time.get_scale())
    gamestate.set("tower_travel", math.floor(travel * 100) / 100)
    gamestate.set("tower_paused_travel", paused_travel)
    gamestate.set("tower_bonus", bonus_total)
    gamestate.set("tower_cursor_x", cursor_pad and cursor_pad.x or -1)
    gamestate.set("tower_cursor_y", cursor_pad and cursor_pad.y or -1)
end

-- Set up --------------------------------------------------------------------------------------------------------------

local function read_stats()
    stats.archer = { cost = archer_cost, range = archer_range, damage = archer_damage, reload = archer_reload }
    stats.cannon = { cost = cannon_cost, range = cannon_range, damage = cannon_damage, reload = cannon_reload,
                     splash = cannon_splash }
    stats.frost = { cost = frost_cost, range = frost_range, damage = frost_damage, reload = frost_reload }
    stats.runner = { hp = runner_hp, speed = runner_speed, gold = runner_gold, arrow = 1.0 }
    stats.soldier = { hp = soldier_hp, speed = soldier_speed, gold = soldier_gold, arrow = 1.0 }
    stats.armoured = { hp = armoured_hp, speed = armoured_speed, gold = armoured_gold, arrow = armoured_arrow }
end

local function find_pads()
    local width, height = isometric.get_size(map_id)
    if width == nil then return end
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            local _, overlay = isometric.get_cell(map_id, road_layer, x, y)
            if overlay == pad_tile then
                local pad = { x = x, y = y, tower = nil }
                pads[#pads + 1] = pad
                pad_at[key(x, y)] = pad
            end
        end
    end
    for _, pad in ipairs(pads) do pad_order[#pad_order + 1] = pad end
    table.sort(pad_order, function(a, b)
        local sa, sb = a.x - a.y, b.x - b.y
        if sa ~= sb then return sa < sb end
        return a.x + a.y < b.x + b.y
    end)
end

-- The cell an entity stands on: a sprite stands on the front corner of its footprint (`cell + 1`).
local function cell_under(id, footprint)
    local x, y = transform.get_position(id)
    return math.floor(x - footprint), math.floor(y - footprint)
end

-- The road cell next to a cell (the gate stands on the first road cell, the keep beside the last one).
local function road_near(x, y, size)
    for radius = 0, 2 do
        for dy = -radius, size - 1 + radius do
            for dx = -radius, size - 1 + radius do
                if path.is_walkable(map_id, x + dx, y + dy) then return x + dx, y + dy end
            end
        end
    end
    return x, y
end

local function ask_route()
    local gate_id, keep_id = scene.find_entity("Gate"), scene.find_entity("Keep")
    if gate_id == 0 or keep_id == 0 then
        log.warn("Tower defence: the map needs a Gate and a Keep entity.")
        return
    end
    local gx, gy = cell_under(gate_id, 1)
    gx, gy = road_near(gx, gy, 1)
    local kx, ky = cell_under(keep_id, 2)
    kx, ky = road_near(kx, ky, 2)
    route_request = path.find(map_id, gx, gy, kx, ky)
end

local function poll_route()
    if route ~= nil or route_request == 0 then return end
    local status = path.poll(map_id, route_request)
    if status == "found" then
        local cells = path.get(map_id, route_request)
        route = { points = {}, total = 0.0 }
        for i = 1, #cells, 2 do
            local point = { x = cells[i] + 0.5, y = cells[i + 1] + 0.5, at = 0.0 }
            local last = route.points[#route.points]
            if last ~= nil then
                route.total = route.total + math.abs(point.x - last.x) + math.abs(point.y - last.y)
                point.at = route.total
            end
            route.points[#route.points + 1] = point
        end
        path.release(map_id, route_request)
        gamestate.set("tower_route", #route.points)
    elseif status == "not_found" or status == "unknown" then
        log.warn("Tower defence: no road from the gate to the keep.")
        path.release(map_id, route_request)
        route_request = 0
    end
end

-- The point of the road at a distance from the gate, the heading there (0 +x, 1 +y, 2 -x, 3 -y) and its direction.
local function along(distance)
    local points = route.points
    local index = 1
    while index < #points and points[index + 1].at < distance do index = index + 1 end
    local a, b = points[index], points[math.min(index + 1, #points)]
    local dx, dy = b.x - a.x, b.y - a.y
    local length = math.abs(dx) + math.abs(dy)
    local t = length > 0 and math.min(1.0, (distance - a.at) / length) or 0.0
    local heading = 0
    if dx > 0 then heading = 0 elseif dy > 0 then heading = 1 elseif dx < 0 then heading = 2 else heading = 3 end
    if length > 0 then dx, dy = dx / length, dy / length end
    return a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, heading, dx, dy
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_down", 0.4, wipe_color.r, wipe_color.g, wipe_color.b)
    end
    time.set_scale(1.0)
    time.set_paused(false)
    read_stats()
    map_id = scene.find_entity("Ground")
    camera_id = scene.find_entity("MainCamera")
    cursor_id = scene.find_entity("Cursor")
    ring_id = scene.find_entity("RangeRing")
    choice_ids = scene.find_entities("BuildChoice")
    for _, name in ipairs({ "gold", "lives", "wave", "message", "info", "info_icon", "info_title", "info_text",
                            "call", "speed", "pause", "resume" }) do
        hud[name] = 0
    end
    hud.gold = ui.find("GoldText")
    hud.lives = ui.find("LivesText")
    hud.wave = ui.find("WaveText")
    hud.message = ui.find("MessageText")
    hud.info = ui.find("InfoPanel")
    hud.info_icon = ui.find("InfoIcon")
    hud.info_title = ui.find("InfoTitle")
    hud.info_text = ui.find("InfoText")
    hud.call = ui.find("CallButton")
    hud.speed = ui.find("SpeedButton")
    hud.pause = ui.find("PauseWindow")
    hud.resume = ui.find("ResumeButton")
    show(hud.pause, false)
    show(hud.info, false)
    show(hud.message, false)
    show(cursor_id, false)
    show(ring_id, false)
    for _, id in ipairs(choice_ids) do show(id, false) end
    find_pads()
    ask_route()
    gold, lives_left = start_gold, lives
    wave_count = #waves
    countdown = first_wave_delay
    gamestate.set("tower_won", false)
    gamestate.set("tower_lost", false)
    gamestate.set("tower_map", map_number)
    gamestate.set("tower_scene", scene_path)
    message("Defend the keep!\n" .. wave_count .. " waves", 2.5)
    mirror()
end

-- Waves ---------------------------------------------------------------------------------------------------------------

local function parse_wave(text)
    local queue = {}
    for group in string.gmatch(text or "", "[^;]+") do
        local kind, count, gap = string.match(group, "^%s*(%a+)%s+(%d+)%s+([%d%.]+)%s*$")
        if kind ~= nil and stats[kind] ~= nil and stats[kind].hp ~= nil then
            for i = 1, tonumber(count) do
                queue[#queue + 1] = { kind = kind, gap = (i == 1 and #queue > 0) and tonumber(gap) * 2 or tonumber(gap) }
            end
        else
            log.warn("Tower defence: wave group '" .. group .. "' is not 'kind count gap'.")
        end
    end
    return queue
end

local function start_wave()
    wave = wave + 1
    spawning = parse_wave(waves[wave])
    spawn_clock = 0.0
    countdown = 0.0
    play("sounds/game/horn.wav")
    message("Wave " .. wave .. " / " .. wave_count, 1.8)
end

local function call_early()
    if ending ~= nil or spawning ~= nil or wave >= wave_count or route == nil then return end
    local bonus = math.floor(countdown * early_bonus)
    if bonus > 0 then
        gold = gold + bonus
        earned = earned + bonus
        bonus_total = bonus_total + bonus
        play("sounds/game/coins.wav")
    end
    start_wave()
    if bonus > 0 then message("Wave " .. wave .. " / " .. wave_count .. "\n+" .. bonus .. " gold", 1.8) end
end

local function spawn_enemy(kind)
    local base = stats[kind]
    local x, y, heading = along(0.0)
    local id = scene.spawn_prefab("prefabs/enemy_" .. kind .. ".owlprefab", x + 0.5, y + 0.5, 0.0)
    if id == 0 then return end
    isometric.set_sprite_frame(id, ENEMY_ROW[kind] * 4 + heading)
    local bar = scene.spawn_prefab("prefabs/health_bar.owlprefab", x + 0.5, y + 0.5, ENEMY_TOP)
    show(bar, false)
    local hp = base.hp * (1.0 + hp_growth * (wave - 1))
    spawned = spawned + 1
    local side = ((spawned * 37) % 7) / 6.0 - 0.5
    enemies[#enemies + 1] = {
        id = id, bar = bar, kind = kind, hp = hp, max = hp, speed = base.speed, gold = base.gold,
        arrow = base.arrow, distance = 0.0, side = side * 0.3, slow = 0.0, heading = heading, x = x, y = y,
        phase = spawned * 0.7,
    }
end

local function update_waves(dt)
    if route == nil then return end
    if spawning ~= nil then
        spawn_clock = spawn_clock - dt
        while spawning ~= nil and spawn_clock <= 0.0 do
            local next_one = table.remove(spawning, 1)
            if next_one == nil then
                spawning = nil
                if wave < wave_count then countdown = wave_delay end
            else
                spawn_enemy(next_one.kind)
                spawn_clock = spawn_clock + (spawning[1] and spawning[1].gap or 0.0)
                if #spawning == 0 then
                    spawning = nil
                    if wave < wave_count then countdown = wave_delay end
                end
            end
        end
    elseif wave < wave_count then
        countdown = countdown - dt
        if countdown <= 0.0 then start_wave() end
    end
end

-- Enemies -------------------------------------------------------------------------------------------------------------

local function spawn_puff(x, y, z, r, g, b, size, duration)
    local id = scene.spawn_prefab("prefabs/puff.owlprefab", x, y, z)
    if id == 0 then return end
    entity.set_tint(id, r, g, b, 0.85)
    puffs[#puffs + 1] = { id = id, t = 0.0, duration = duration, size = size, r = r, g = g, b = b }
end

local function remove_enemy(index)
    local foe = enemies[index]
    scene.destroy_entity(foe.id)
    if foe.bar ~= 0 then scene.destroy_entity(foe.bar) end
    foe.dead = true
    table.remove(enemies, index)
end

local function hurt(foe, amount)
    if foe.dead then return end
    foe.hp = foe.hp - amount
    if foe.hp <= 0 then
        gold = gold + foe.gold
        earned = earned + foe.gold
        kills = kills + 1
        play("sounds/game/pop.wav")
        spawn_puff(foe.x + 0.5, foe.y + 0.5, 0.4, 1.0, 0.95, 0.7, 0.6, 0.35)
        for index, other in ipairs(enemies) do
            if other == foe then
                remove_enemy(index)
                break
            end
        end
        return
    end
    if foe.bar ~= 0 then
        local share = foe.hp / foe.max
        show(foe.bar, true)
        transform.set_scale(foe.bar, 0.5 * share, 0.094, 1.0)
        entity.set_tint(foe.bar, 1.0 - share * 0.8, 0.25 + share * 0.65, 0.2, 1.0)
    end
end

local function update_enemies(dt)
    local index = 1
    while index <= #enemies do
        local foe = enemies[index]
        local speed = foe.speed
        if foe.slow > 0.0 then
            foe.slow = foe.slow - dt
            speed = speed * frost_slow
        end
        local step = speed * dt
        foe.distance = foe.distance + step
        travel = travel + step
        if foe.distance >= route.total then
            lives_left = lives_left - 1
            leaks = leaks + 1
            play("sounds/game/leak.wav")
            message("The keep is hit! " .. math.max(0, lives_left) .. " lives left", 1.5)
            remove_enemy(index)
        else
            local x, y, heading, dx, dy = along(foe.distance)
            -- Walk beside the middle of the road, on the right of the heading ((-dy, dx) on screen).
            x, y = x - dy * foe.side, y + dx * foe.side
            foe.x, foe.y = x, y
            local bob = 0.12 * math.abs(math.sin(clock * 9.0 * speed / 1.3 + foe.phase))
            transform.set_position(foe.id, x + 0.5, y + 0.5, bob)
            if foe.bar ~= 0 then transform.set_position(foe.bar, x + 0.5, y + 0.5, ENEMY_TOP + bob) end
            if heading ~= foe.heading then
                foe.heading = heading
                isometric.set_sprite_frame(foe.id, ENEMY_ROW[foe.kind] * 4 + heading)
            end
            local tint = foe.slow > 0.0 and 0.65 or 1.0
            if foe.tinted ~= tint then
                foe.tinted = tint
                entity.set_tint(foe.id, tint, tint + (1.0 - tint) * 0.5, 1.0, 1.0)
            end
            index = index + 1
        end
    end
end

-- Towers and projectiles ----------------------------------------------------------------------------------------------

local function close_ring()
    ring = nil
    hover = 0
    show(ring_id, false)
    for _, id in ipairs(choice_ids) do show(id, false) end
    show(hud.info, false)
end

local function target_of(tower)
    local best, best_distance = nil, -1.0
    local range = stats[tower.kind].range
    for _, foe in ipairs(enemies) do
        local dx, dy = foe.x - tower.cx, foe.y - tower.cy
        if dx * dx + dy * dy <= range * range and foe.distance > best_distance then
            best, best_distance = foe, foe.distance
        end
    end
    return best
end

local function fire(tower, foe)
    local z = SHOT_HEIGHT[tower.kind]
    local id = scene.spawn_prefab(PROJECTILE[tower.kind], tower.cx + 0.5, tower.cy + 0.5, z)
    if id == 0 then return end
    local shot = { id = id, kind = tower.kind, x = tower.cx, y = tower.cy, z = z, target = foe, t = 0.0,
                   sx = tower.cx, sy = tower.cy, sz = z, tx = foe.x, ty = foe.y }
    if tower.kind == "cannon" then
        -- Aim where the enemy will be when the shell lands.
        local lead = foe.distance + foe.speed * (foe.slow > 0.0 and frost_slow or 1.0) * ball_flight
        shot.tx, shot.ty = along(math.min(lead, route.total))
    end
    projectiles[#projectiles + 1] = shot
    if clock - (last_shot_sound[tower.kind] or -1.0) > 0.12 then
        last_shot_sound[tower.kind] = clock
        play(SHOT_SOUND[tower.kind])
    end
end

local function update_towers(dt)
    for _, tower in ipairs(towers) do
        tower.cooldown = tower.cooldown - dt
        if tower.cooldown <= 0.0 then
            local foe = target_of(tower)
            if foe ~= nil then
                fire(tower, foe)
                tower.cooldown = stats[tower.kind].reload
            else
                tower.cooldown = 0.0
            end
        end
    end
end

local function land(shot)
    if shot.kind == "cannon" then
        play("sounds/game/boom.wav")
        spawn_puff(shot.tx + 0.5, shot.ty + 0.5, 0.2, 1.0, 0.7, 0.35, 1.6, 0.4)
        local radius = stats.cannon.splash
        local hit = {}
        for _, foe in ipairs(enemies) do
            local dx, dy = foe.x - shot.tx, foe.y - shot.ty
            if dx * dx + dy * dy <= radius * radius then hit[#hit + 1] = foe end
        end
        for _, foe in ipairs(hit) do hurt(foe, stats.cannon.damage) end
        return
    end
    local foe = shot.target
    if foe == nil or foe.dead then return end
    if shot.kind == "archer" then
        play("sounds/game/hit.wav")
        hurt(foe, stats.archer.damage * foe.arrow)
    else
        foe.slow = frost_time
        spawn_puff(foe.x + 0.5, foe.y + 0.5, 0.6, 0.6, 0.85, 1.0, 0.7, 0.3)
        hurt(foe, stats.frost.damage)
    end
end

local function update_projectiles(dt)
    local index = 1
    while index <= #projectiles do
        local shot = projectiles[index]
        local arrived = false
        if shot.kind == "cannon" then
            shot.t = shot.t + dt / ball_flight
            local t = math.min(1.0, shot.t)
            shot.x = shot.sx + (shot.tx - shot.sx) * t
            shot.y = shot.sy + (shot.ty - shot.sy) * t
            shot.z = shot.sz * (1.0 - t) + 6.0 * t * (1.0 - t)
            arrived = t >= 1.0
        else
            if shot.target ~= nil and not shot.target.dead then
                shot.tx, shot.ty = shot.target.x, shot.target.y
            end
            local dx, dy, dz = shot.tx - shot.x, shot.ty - shot.y, 1.2 - shot.z
            local distance = math.sqrt(dx * dx + dy * dy + dz * dz * 0.05)
            local step = arrow_speed * dt
            if distance <= step or distance < 0.05 then
                arrived = true
            else
                shot.x, shot.y, shot.z = shot.x + dx / distance * step, shot.y + dy / distance * step,
                        shot.z + dz * step / distance
            end
        end
        if arrived then
            land(shot)
            scene.destroy_entity(shot.id)
            table.remove(projectiles, index)
        else
            transform.set_position(shot.id, shot.x + 0.5, shot.y + 0.5, shot.z)
            index = index + 1
        end
    end
end

local function update_puffs(dt)
    local index = 1
    while index <= #puffs do
        local puff = puffs[index]
        puff.t = puff.t + dt
        local t = math.min(1.0, puff.t / puff.duration)
        local size = puff.size * (0.5 + 0.7 * t)
        transform.set_scale(puff.id, size, size * 0.7, 1.0)
        entity.set_tint(puff.id, puff.r, puff.g, puff.b, 0.85 * (1.0 - t))
        if t >= 1.0 then
            scene.destroy_entity(puff.id)
            table.remove(puffs, index)
        else
            index = index + 1
        end
    end
end

-- The build ring ------------------------------------------------------------------------------------------------------

-- The foot of a badge of the ring: `slot` -1, 0 or 1 from left to right, raised over the pad; the feet stand two cells
-- nearer to the viewer than the pad (height added back) so that the badges are drawn over the tower and its neighbours.
local function badge_foot(pad, slot)
    local near = 1.5
    local sx, sy = slot * 52.0, (slot == 0) and -118.0 or -86.0
    local foot_sy = sy + 24.0
    return pad.x + 0.5 + near + sx / 64.0, pad.y + 0.5 + near - sx / 64.0, 2.0 * near - foot_sy / 16.0
end

local function show_range(pad, kind)
    if ring_id == 0 or kind == nil then
        show(ring_id, false)
        return
    end
    local r = stats[kind].range
    transform.set_position(ring_id, pad.x + 0.5 + r * SQRT_HALF, pad.y + 0.5 + r * SQRT_HALF, 0.0)
    transform.set_scale(ring_id, r * 2.0 * SQRT_HALF * 1.0, r * SQRT_HALF, 1.0)
    show(ring_id, true)
end

local function refresh_info()
    if ring == nil then
        show(hud.info, false)
        return
    end
    local choice = ring.choices[hover] or ring.choices[1]
    local pad = ring.pad
    show(hud.info, true)
    if pad.tower ~= nil then
        ui.set_image_frame(hud.info_icon, ICON[pad.tower.kind])
        set_text("info_title", NAMES[pad.tower.kind])
        set_text("info_text", "Sell for " .. refund_of(pad.tower) .. " gold (X, pad X or the coin).")
        show_range(pad, pad.tower.kind)
        return
    end
    if hover == 0 then
        ui.set_image_frame(hud.info_icon, ICON.archer)
        set_text("info_title", "Build pad")
        set_text("info_text", string.format("Archer %d, cannon %d, frost %d gold: point at a badge.",
                cost_of("archer"), cost_of("cannon"), cost_of("frost")))
        show_range(pad, nil)
        return
    end
    local kind = choice.kind
    ui.set_image_frame(hud.info_icon, ICON[kind])
    local price = cost_of(kind)
    set_text("info_title", NAMES[kind] .. " - " .. price .. " gold")
    set_text("info_text", gold >= price and BLURBS[kind] or ("Not enough gold: " .. price - gold .. " more."))
    show_range(pad, hover > 0 and kind or nil)
end

local function refresh_badges()
    if ring == nil then return end
    for index, choice in ipairs(ring.choices) do
        local lit = index == hover
        local afford = choice.kind == "sell" or gold >= cost_of(choice.kind)
        local base = afford and 1.0 or 0.45
        local glow = lit and 1.0 or 0.85
        entity.set_tint(choice.id, base * glow, base * glow, base * glow * (afford and 1.0 or 0.9), 1.0)
        local size = lit and 0.85 or 0.72
        transform.set_scale(choice.id, size, size, 1.0)
    end
end

local function open_ring(pad)
    close_ring()
    ring = { pad = pad, choices = {} }
    local slots = pad.tower ~= nil and { { "sell", 0 } } or { { "archer", -1 }, { "cannon", 0 }, { "frost", 1 } }
    for index, slot in ipairs(slots) do
        local id = choice_ids[index]
        if id ~= nil then
            local x, y, z = badge_foot(pad, slot[2])
            transform.set_position(id, x, y, z)
            isometric.set_sprite_frame(id, ICON[slot[1]])
            show(id, true)
            ring.choices[#ring.choices + 1] = { kind = slot[1], id = id }
        end
    end
    hover = pad_mode and 1 or 0
    play("sounds/ui/click.wav")
    refresh_badges()
    refresh_info()
end

local function build(pad, kind)
    if pad.tower ~= nil then return end
    local price = cost_of(kind)
    if gold < price then
        message("Not enough gold", 1.0)
        return
    end
    gold = gold - price
    local id = scene.spawn_prefab("prefabs/tower_" .. kind .. ".owlprefab", pad.x + 1, pad.y + 1, 0.0)
    if id == 0 then
        gold = gold + price
        return
    end
    local tower = { id = id, kind = kind, pad = pad, cx = pad.x + 0.5, cy = pad.y + 0.5, cooldown = 0.2 }
    pad.tower = tower
    towers[#towers + 1] = tower
    built = built + 1
    play("sounds/game/build.wav")
    close_ring()
end

local function sell(pad)
    local tower = pad.tower
    if tower == nil then return end
    gold = gold + refund_of(tower)
    scene.destroy_entity(tower.id)
    for index, other in ipairs(towers) do
        if other == tower then
            table.remove(towers, index)
            break
        end
    end
    pad.tower = nil
    play("sounds/game/sell.wav")
    close_ring()
end

local function choose(index)
    if ring == nil then return end
    local choice = ring.choices[index]
    if choice == nil then return end
    if choice.kind == "sell" then
        sell(ring.pad)
    else
        build(ring.pad, choice.kind)
    end
end

-- Input ---------------------------------------------------------------------------------------------------------------

local function pad_of_tower(id)
    for _, tower in ipairs(towers) do
        if tower.id == id then return tower.pad end
    end
    return nil
end

local function click()
    if ring ~= nil then
        local badge = scene.pick_entity(nil, nil, "BuildChoice")
        for index, choice in ipairs(ring.choices) do
            if choice.id == badge then
                choose(index)
                return
            end
        end
    end
    local pad = pad_of_tower(scene.pick_entity(nil, nil, "Tower"))
    if pad == nil then
        local x, y = isometric.pick_cell(map_id)
        if x ~= nil then pad = pad_at[key(x, y)] end
    end
    if pad ~= nil and (ring == nil or ring.pad ~= pad) then
        cursor_pad = pad
        open_ring(pad)
    else
        close_ring()
    end
end

local function hover_badge()
    if ring == nil or pad_mode then return end
    local badge = scene.pick_entity(nil, nil, "BuildChoice")
    local found = 0
    for index, choice in ipairs(ring.choices) do
        if choice.id == badge then found = index end
    end
    if found ~= hover then
        hover = found
        refresh_badges()
        refresh_info()
    end
end

local function step_cursor(direction)
    if #pad_order == 0 then return end
    local index = 0
    for i, pad in ipairs(pad_order) do
        if pad == cursor_pad then index = i end
    end
    if index == 0 then
        index = direction > 0 and 1 or #pad_order
    else
        index = (index - 1 + direction) % #pad_order + 1
    end
    cursor_pad = pad_order[index]
end

local function update_cursor()
    local mx, my = scene.get_viewport_mouse()
    if last_mouse_x ~= nil and (mx ~= last_mouse_x or my ~= last_mouse_y) then
        mouse_moved = true
        pad_mode = false
    end
    last_mouse_x, last_mouse_y = mx, my
    local shown_pad = nil
    if pad_mode then
        shown_pad = cursor_pad
    elseif mouse_moved and not ui.is_mouse_over_ui() then
        local x, y = isometric.pick_cell(map_id)
        if x ~= nil then shown_pad = pad_at[key(x, y)] end
    end
    if shown_pad ~= nil and cursor_id ~= 0 then
        transform.set_position(cursor_id, shown_pad.x + 1, shown_pad.y + 1, 0.0)
    end
    show(cursor_id, shown_pad ~= nil)
end

local function handle_input()
    if input.is_action_just_pressed("fast_forward") then on_speed_clicked() end
    if input.is_action_just_pressed("call_wave") then call_early() end
    local world_free = not ui.is_world_blocked()
    if world_free and input.is_action_just_pressed("primary") and not ui.is_mouse_over_ui() then
        pad_mode = false
        click()
    end
    if input.is_action_just_pressed("secondary") or input.is_action_just_pressed("cancel") then close_ring() end
    local step = 0
    if input.is_action_just_pressed("next_pad") then step = 1 end
    if input.is_action_just_pressed("prev_pad") then step = -1 end
    if step ~= 0 then
        pad_mode = true
        if ring ~= nil and #ring.choices > 1 then
            hover = (math.max(hover, 1) - 1 + step) % #ring.choices + 1
            refresh_badges()
            refresh_info()
        else
            close_ring()
            step_cursor(step)
        end
    end
    if input.is_action_just_pressed("confirm") and cursor_pad ~= nil then
        pad_mode = true
        if ring ~= nil and hover > 0 then
            choose(hover)
        else
            open_ring(cursor_pad)
        end
    end
    for index, kind in ipairs(KINDS) do
        if input.is_action_just_pressed("build_" .. index) and ring ~= nil and ring.pad.tower == nil then
            build(ring.pad, kind)
        end
    end
    if input.is_action_just_pressed("sell") and ring ~= nil and ring.pad.tower ~= nil then sell(ring.pad) end
end

-- Camera --------------------------------------------------------------------------------------------------------------

local function update_camera(real_dt)
    if camera_id == 0 then return end
    local right, down = input.get_action_vector("pan")
    local width, height = scene.get_viewport_size()
    if mouse_moved and width > 0 and edge_pixels > 0 and not ui.is_mouse_over_ui() then
        local mx, my = scene.get_viewport_mouse()
        if mx >= 0 and my >= 0 and mx < width and my < height then
            if mx < edge_pixels then right = -1 elseif mx > width - edge_pixels then right = 1 end
            if my < edge_pixels then down = -1 elseif my > height - edge_pixels then down = 1 end
        end
    end
    local x, y, z = transform.get_position(camera_id)
    if pad_mode and cursor_pad ~= nil then
        -- The keyboard and gamepad cursor drags the view along when it leaves the middle of the screen.
        local cx, cy = cursor_pad.x + 0.5, cursor_pad.y + 0.5
        local sx, sy = (cx - cy) - (x - y), (cx + cy) - (x + y)
        if math.abs(sx) > 9 or math.abs(sy) > 9 then
            x, y = x + (cx - x) * math.min(1.0, real_dt * 4.0), y + (cy - y) * math.min(1.0, real_dt * 4.0)
        end
    end
    if right ~= 0 or down ~= 0 then
        -- Screen right is world (+x, -y), screen down is world (+x, +y).
        local step = pan_speed * real_dt * 0.5
        x, y = x + (right + down) * step, y + (down - right) * step
    end
    local size = isometric.get_size(map_id) or 24
    local low, high = camera_margin, size - camera_margin
    x, y = math.max(low, math.min(high, x)), math.max(low, math.min(high, y))
    transform.set_position(camera_id, x, y, z)
end

-- HUD -----------------------------------------------------------------------------------------------------------------

local function update_hud()
    set_text("gold", "Gold " .. gold)
    set_text("lives", "Lives " .. math.max(0, lives_left))
    if spawning ~= nil then
        set_text("wave", "Wave " .. wave .. " / " .. wave_count .. " - incoming")
    elseif wave < wave_count then
        set_text("wave", "Wave " .. wave .. " / " .. wave_count .. " - next in " .. math.ceil(countdown) .. " s")
    else
        set_text("wave", "Wave " .. wave .. " / " .. wave_count .. " - last one")
    end
    local can_call = spawning == nil and wave < wave_count and route ~= nil and ending == nil
    if hud.call ~= 0 then
        ui.set_button_enabled(hud.call, can_call)
        set_text("call", can_call and ("Call wave +" .. math.floor(countdown * early_bonus)) or "Call wave")
    end
    set_text("speed", fast and "Speed x2" or "Speed x1")
    if ring ~= nil and ring.pad.tower == nil then refresh_info() end
end

-- End of the map ------------------------------------------------------------------------------------------------------

local function finish(won)
    ending = won and "won" or "lost"
    end_clock = end_delay
    close_ring()
    time.set_scale(1.0)
    fast = false
    gamestate.set(won and "tower_won" or "tower_lost", true)
    gamestate.set("tower_result_won", won)
    gamestate.set("tower_result_kills", kills)
    gamestate.set("tower_result_lives", math.max(0, lives_left))
    gamestate.set("tower_result_max_lives", lives)
    gamestate.set("tower_result_gold", earned)
    gamestate.set("tower_result_time", math.floor(clock))
    gamestate.set("tower_result_towers", built)
    gamestate.set("tower_result_wave", wave)
    gamestate.set("tower_result_waves", wave_count)
    if won then
        local done = string.format("tower_done_%02d", map_number)
        local best = string.format("tower_best_%02d", map_number)
        settings.set(done, true)
        gamestate.set("tower_done", true)
        if lives_left > settings.get(best, 0) then settings.set(best, lives_left) end
        settings.save()
        message("The keep holds!", end_delay)
    else
        play("sounds/game/defeat.wav")
        message("The keep has fallen", end_delay)
    end
end

local function check_end()
    if ending ~= nil then return end
    if lives_left <= 0 then
        finish(false)
    elseif wave >= wave_count and spawning == nil and #enemies == 0 and route ~= nil then
        finish(true)
    end
end

-- Pause ---------------------------------------------------------------------------------------------------------------

local function set_paused(value)
    paused = value
    time.set_paused(value)
    travel_at_pause = nil
    show(hud.pause, value)
    if value then
        close_ring()
        ui.raise_window(hud.pause)
        if hud.resume ~= 0 then ui.set_focus(hud.resume) end
    end
    mirror()
end

function on_pause_resume()
    set_paused(false)
end

function on_pause_restart()
    set_paused(false)
    ending = "restart"
    time.set_scale(1.0)
    scene.transition_to(scene_path, "wipe_down", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
end

function on_pause_quit()
    set_paused(false)
    ending = "quit"
    time.set_scale(1.0)
    scene.transition_to("scenes/title.owl", "wipe_down", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
end

function on_menu_clicked()
    if ending == nil then set_paused(not paused) end
end

function on_call_clicked()
    call_early()
end

function on_speed_clicked()
    if ending ~= nil or paused then return end
    fast = not fast
    time.set_scale(fast and 2.0 or 1.0)
end

-- Frame ---------------------------------------------------------------------------------------------------------------

function on_update(dt)
    local real_dt = time.real_delta()
    if ending == "restart" or ending == "quit" then return end
    if ending == nil and input.is_action_just_pressed("pause") then set_paused(not paused) end
    if message_clock > 0.0 then
        message_clock = message_clock - real_dt
        if message_clock <= 0.0 then show(hud.message, false) end
    end
    poll_route()
    if ending ~= nil then
        end_clock = end_clock - real_dt
        if end_clock <= 0.0 and ending ~= "leaving" then
            local target = ending == "won" and next_scene or "scenes/game_over.owl"
            ending = "leaving"
            scene.transition_to(target, "wipe_down", 0.5, wipe_color.r, wipe_color.g, wipe_color.b)
        end
    end
    -- The world runs on the scene clock: while paused `dt` is 0 and nothing moves (`tower_paused_travel` checks it).
    clock = clock + dt
    if ending == nil and not paused then
        handle_input()
        update_waves(dt)
    end
    if route ~= nil then update_enemies(dt) end
    update_towers(dt)
    update_projectiles(dt)
    update_puffs(dt)
    if ending == nil then check_end() end
    if paused then
        -- Measured from the end of the frame the pause was asked in: the clock stops from the next one.
        travel_at_pause = travel_at_pause or travel
        paused_travel = travel - travel_at_pause
    else
        update_cursor()
        hover_badge()
        update_camera(real_dt)
        update_hud()
    end
    mirror()
end
