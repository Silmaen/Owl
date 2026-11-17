-- Mod screen of the engine: the mods of the game in load order, enabled or disabled, moved, applied.
-- A game replaces it with its own scene (`ModScreen` in the project settings) built on the same `mods` calls.

local list_id = 0
local info_id = 0
local toggle_id = 0
local apply_id = 0

local function selected_mod()
    local index = ui.list_get_selected(list_id)
    if index < 1 then
        return nil, 0
    end
    return mods.get(index), index
end

local function describe(mod)
    if mod == nil then
        return "Select a mod."
    end
    local state = mod.enabled and "enabled" or "disabled"
    if mod.enabled and not mod.mounted and mod.issue ~= "" then
        state = "not loaded: " .. mod.issue
    end
    local text = mod.name .. " " .. mod.version .. " (" .. state .. ")"
    if mod.author ~= "" then
        text = text .. "\nby " .. mod.author
    end
    if mod.description ~= "" then
        text = text .. "\n" .. mod.description
    end
    return text
end

local function refresh()
    ui.list_set_count(list_id, mods.count())
    ui.list_invalidate(list_id)
    local mod = selected_mod()
    ui.set_text(info_id, mods.is_managed() and describe(mod) or "The mods of this game are chosen in Owl Nest.")
    ui.set_text(toggle_id, (mod ~= nil and mod.enabled) and "Disable" or "Enable")
    ui.set_button_enabled(apply_id, mods.is_dirty())
end

function on_create()
    list_id = ui.find("ModList")
    info_id = ui.find("ModInfo")
    toggle_id = ui.find("ModToggle")
    apply_id = ui.find("ModApply")
    if mods.count() > 0 then
        ui.list_set_selected(list_id, 1)
    end
    refresh()
end

function on_mod_bind()
    local mod = mods.get(_row_index)
    if mod == nil then
        return
    end
    local mark = mod.enabled and "[x] " or "[ ] "
    ui.list_set_row(_list_entity, _row_index, mark .. mod.name, mod.version)
end

function on_mod_selected()
    refresh()
end

function on_toggle_clicked()
    local mod = selected_mod()
    if mod ~= nil then
        mods.set_enabled(mod.id, not mod.enabled)
        refresh()
    end
end

local function move_selected(offset)
    local mod, index = selected_mod()
    if mod ~= nil and mods.move(mod.id, offset) then
        ui.list_set_selected(list_id, index + offset)
        refresh()
    end
end

function on_up_clicked()
    move_selected(-1)
end

function on_down_clicked()
    move_selected(1)
end

function on_apply_clicked()
    mods.apply(mods.return_level())
end

function on_back_clicked()
    local level = mods.return_level()
    if level ~= "" then
        scene.load_scene(level)
    end
end
