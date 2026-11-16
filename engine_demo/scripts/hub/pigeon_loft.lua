-- The pigeon loft of the hub: E opens a network session (the first machine hosts, a second one joins it), then each
-- E makes the pigeon flap its wings on every machine of the session; the pigeon above the loft is replicated from the
-- host.
properties = {
	{ name = "port", type = "int", default = 47770 },
	{ name = "address", type = "string", default = "127.0.0.1" },
}

local loftPort = 47770
local loftAddress = "127.0.0.1"
local flaps = 0

local function isOnline()
	return net.get_role() ~= "offline"
end

local function say(text)
	ui.show_subtitle(text, 4)
end

function on_create()
	loftPort = port
	loftAddress = address
	gamestate.set("net_online", 0)
	gamestate.set("net_players", 1)
	gamestate.set("pigeon_flaps", 0)
	net.set_player_name(tr("loft.player"))
end

function on_loft_interact()
	if isOnline() then
		net.rpc_all(entity_id, "flap", net.get_local_player())
		return
	end
	if net.host(loftPort) then
		say(string.format(tr("loft.hosting"), net.get_port()))
	elseif net.join(loftAddress, loftPort) then
		say(string.format(tr("loft.joining"), loftAddress))
	else
		say(tr("loft.offline"))
		net.rpc_all(entity_id, "flap", net.get_local_player())
	end
	gamestate.set("net_online", isOnline() and 1 or 0)
end

function flap(player)
	flaps = flaps + 1
	gamestate.set("pigeon_flaps", flaps)
	local stats = net.get_stats()
	say(string.format(tr("loft.flap"), player, stats.players, stats.ping))
end

function on_lobby_changed()
	gamestate.set("net_players", #net.get_players())
end

function on_player_join(player)
	say(string.format(tr("loft.joined"), player))
end

function on_player_leave(player)
	say(string.format(tr("loft.left"), player))
end

function on_disconnected(reason)
	gamestate.set("net_online", 0)
	gamestate.set("net_players", 1)
	say(string.format(tr("loft.disconnected"), reason))
end
