-- The world map of Tower defence, between the levels: a parchment plan where the road from the capital is traced to
-- the last site open. A site is locked (grey ruin), open (red banner) or done (blue banner and its stars). A won
-- level opens the next one: the next stretch of the road is drawn dot by dot (`tower_map_reveal` in the game state,
-- set by defence.lua), then its site rises.
--
-- - `levels`: one string per site, "name|waves|unlock"; `level_scenes`: the scene of each site (empty for a level still
--   to come).
-- - A click on an open site, Q / E, Tab or the D-pad choose it, Enter, pad A or March! starts it; Bonuses lists what
--   the levels won unlocked; Escape, pad B or Back returns to the title.
-- - Progress: the `tower_done_NN` and `tower_stars_NN` settings; mirrored in `tower_map_*` game state keys.

properties = {
    { name = "levels", type = "strings", default = {} },
    { name = "level_scenes", type = "strings", default = {} },
    { name = "dot_time", type = "float", default = 0.08, min = 0.01 },
    { name = "wipe_color", type = "color", default = { 0.1, 0.14, 0.24, 1 } },
}

local sites = {}            -- { scene, name, waves, unlock, id, ring, stars = {ids}, dots = {ids}, done, open, stars_won }
local selected = 0
local leaving = false
local reveal, reveal_clock, reveal_index = 0, 0.0, 0
local ids = {}
local clock = 0.0

local function set(id, text)
    if id ~= 0 then ui.set_text(id, text) end
end

local function by_x(list)
    local out = {}
    for _, id in ipairs(list) do
        local x, y = transform.get_position(id)
        out[#out + 1] = { id = id, x = x, y = y }
    end
    table.sort(out, function(a, b) return a.x < b.x end)
    return out
end

local function done_key(index)
    return string.format("tower_done_%02d", index)
end

local function stars_key(index)
    return string.format("tower_stars_%02d", index)
end

local function refresh_panel()
    local site = sites[selected]
    if site == nil then return end
    set(ids.name, site.name)
    if site.scene == "" then
        set(ids.info, "A land still to conquer: this level comes later.")
    elseif site.done then
        set(ids.info, string.format("%s waves - defended, %d of 3 stars", site.waves, site.stars_won))
    else
        set(ids.info, string.format("%s waves - %s", site.waves, site.open and "the Horde is coming" or "locked"))
    end
    set(ids.unlock, "Win to unlock: " .. site.unlock)
    ui.set_button_enabled(ids.march, site.open and site.scene ~= "")
    for index, other in ipairs(sites) do ui.set_visible(other.ring, index == selected) end
    gamestate.set("tower_map_selected", selected)
end

local function select(index)
    if sites[index] == nil or not sites[index].open then return end
    if selected ~= index then sound.play("sounds/ui/click.wav") end
    selected = index
    refresh_panel()
end

local function show_site(site)
    local frame = site.done and 2 or (site.open and 1 or 0)
    isometric.set_sprite_frame(site.id, frame)
    for star, id in ipairs(site.stars) do
        ui.set_visible(id, site.done)
        isometric.set_sprite_frame(id, star <= site.stars_won and 1 or 0)
    end
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_down", 0.4, wipe_color.r, wipe_color.g, wipe_color.b)
    end
    time.set_scale(1.0)
    time.set_paused(false)
    ids.name, ids.info = scene.find_entity("SiteName"), scene.find_entity("SiteInfo")
    ids.unlock, ids.march = scene.find_entity("SiteUnlock"), scene.find_entity("MarchButton")
    ids.bonus, ids.bonus_back = scene.find_entity("BonusWindow"), scene.find_entity("BonusBackButton")
    local site_ids, rings, stars = by_x(scene.find_entities("Site")), by_x(scene.find_entities("SiteRing")),
            by_x(scene.find_entities("SiteStar"))
    reveal = gamestate.get("tower_map_reveal", 0)
    gamestate.remove("tower_map_reveal")
    local open_count = 0
    for index, text in ipairs(levels) do
        local name, waves, unlock = string.match(text, "^([^|]*)|([^|]*)|(.*)$")
        local site = { scene = level_scenes[index] or "", name = name or "?", waves = waves or "?", unlock = unlock or "",
                       id = site_ids[index] and site_ids[index].id or 0, ring = rings[index] and rings[index].id or 0,
                       stars = {}, dots = {} }
        for star = 1, 3 do
            local entry = stars[(index - 1) * 3 + star]
            site.stars[star] = entry and entry.id or 0
        end
        for _, dot in ipairs(by_x(scene.find_entities("PathDot " .. index))) do site.dots[#site.dots + 1] = dot.id end
        site.done = settings.get(done_key(index), false) == true
        site.stars_won = settings.get(stars_key(index), 0)
        site.open = site.done or index == 1 or (sites[index - 1] ~= nil and sites[index - 1].done)
        if site.open then open_count = index end
        sites[index] = site
    end
    for index, site in ipairs(sites) do
        -- The stretch being revealed starts hidden; every other stretch shows up to the last open site.
        local shown = site.open and index ~= reveal
        for _, dot in ipairs(site.dots) do ui.set_visible(dot, shown) end
        if index == reveal then site.open, site.rising = false, true end
        show_site(site)
    end
    for index = #sites, 1, -1 do
        if sites[index].open and (not sites[index].done or selected == 0) then
            selected = index
            if not sites[index].done then break end
        end
    end
    if selected == 0 then selected = 1 end
    for index = 1, #sites do
        local line = scene.find_entity("BonusLine" .. index)
        local site = sites[index]
        set(line, (site.done and "+ " or "- ") .. site.unlock .. (site.done and "" or " (win " .. site.name .. ")"))
    end
    ui.set_visible(ids.bonus, false)
    gamestate.set("tower_map_open", open_count)
    gamestate.set("tower_map", true)
    refresh_panel()
end

local function leave(path, jingle)
    if leaving then return end
    leaving = true
    if jingle then sound.play("sounds/ui/start.wav") end
    scene.transition_to(path, "wipe_down", 0.35, wipe_color.r, wipe_color.g, wipe_color.b)
end

function on_march_clicked()
    local site = sites[selected]
    if site ~= nil and site.open and site.scene ~= "" then leave(site.scene, true) end
end

function on_back_clicked()
    leave("scenes/title.owl", false)
end

function on_bonus_clicked()
    ui.set_visible(ids.bonus, true)
    ui.raise_window(ids.bonus)
    if ids.bonus_back ~= 0 then ui.set_focus(ids.bonus_back) end
    gamestate.set("tower_map_bonus", true)
end

function on_bonus_closed()
    ui.set_visible(ids.bonus, false)
    if ids.march ~= 0 then ui.set_focus(ids.march) end
    gamestate.set("tower_map_bonus", false)
end

-- The next stretch of the road, dot by dot, then the site of the level it opens.
local function update_reveal(dt)
    local site = sites[reveal]
    if site == nil or not site.rising then return end
    reveal_clock = reveal_clock + dt
    while reveal_clock >= dot_time and reveal_index < #site.dots do
        reveal_clock = reveal_clock - dot_time
        reveal_index = reveal_index + 1
        ui.set_visible(site.dots[reveal_index], true)
        sound.play("sounds/ui/click.wav")
    end
    if reveal_index >= #site.dots and reveal_clock >= 0.3 then
        site.rising, site.open = false, true
        show_site(site)
        sound.play("sounds/game/build.wav")
        select(reveal)
        gamestate.set("tower_map_open", reveal)
    end
    gamestate.set("tower_map_dots_shown", reveal_index)
end

function on_update(dt)
    local real = time.real_delta()
    clock = clock + real
    update_reveal(real)
    -- The open, unfinished sites bob a little.
    for _, site in ipairs(sites) do
        if site.open and not site.done and site.id ~= 0 then
            local s = 2.4 * (1.0 + 0.04 * math.sin(clock * 3.0))
            transform.set_scale(site.id, s, s, 1.0)
        end
    end
    if leaving or gamestate.get("tower_map_bonus", false) then
        if input.is_action_just_pressed("ui_cancel") then on_bonus_closed() end
        return
    end
    if input.is_action_just_pressed("ui_cancel") or input.is_action_just_pressed("cancel") then
        on_back_clicked()
        return
    end
    local step = 0
    if input.is_action_just_pressed("next_pad") then step = 1 end
    if input.is_action_just_pressed("prev_pad") then step = -1 end
    if step ~= 0 then
        local index = selected
        repeat
            index = index + step
        until index < 1 or index > #sites or sites[index].open
        if index >= 1 and index <= #sites then select(index) end
    end
    if input.is_action_just_pressed("confirm") then on_march_clicked() end
    if input.is_action_just_pressed("primary") and not ui.is_mouse_over_ui() then
        local picked = scene.pick_entity(nil, nil, "Site")
        for index, site in ipairs(sites) do
            if site.id == picked then
                if index == selected then on_march_clicked() else select(index) end
            end
        end
    end
end
