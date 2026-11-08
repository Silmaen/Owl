/**
 * @file NavCommands.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "NavCommands.h"

#include "../EntitySnapshot.h"
#include "CommandRegistry.h"
#include "ComponentCommands.h"
#include "PrefabCommands.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/NavMeshVolume.h>
#include <scene/nav/NavBake.h>

#include <cstdint>
#include <format>
#include <string>
#include <utility>

namespace owl::nest::commands {

namespace {

using Prepared = expected<PreparedCommand, CommandFailure>;

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto describe(const scene::nav::NavBuildError iError) -> const char* {
	switch (iError) {
		case scene::nav::NavBuildError::NoGeometry:
			return "no static geometry inside the box";
		case scene::nav::NavBuildError::InvalidSettings:
			return "a size or a height of the settings is not positive";
		case scene::nav::NavBuildError::TooLarge:
			return "too many voxels, raise the cell size";
		case scene::nav::NavBuildError::NoWalkableArea:
			return "no floor flat, wide and high enough for an agent";
	}
	return "unknown error";
}

auto bake(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto& values = iArgs.getValues();
	const auto found = values.find("entity");
	if (found == values.end())
		return fail(CommandError::MissingArgument, "argument `entity` is missing");
	auto entity = CommandRegistry::resolveEntity(*ioScene, found->second);
	if (!entity)
		return fail(CommandError::UnknownEntity, "no entity matches `entity`");
	if (!entity->hasComponent<scene::component::NavMeshVolume>())
		return fail(CommandError::NotApplicable, std::format("`{}` has no Nav Mesh Volume", entity->getName()));
	auto before = EntitySnapshot::capture(*entity);
	const auto beforeYaml = before.yamlData;
	if (const auto baked = scene::nav::bakeNavMesh(*ioScene, *entity); !baked)
		return fail(CommandError::NotApplicable, std::format("nothing baked: {}", describe(baked.error())));
	auto command = mkUniq<ModifyEntityCommand>(entity->getUUID(), std::move(before),
											   std::format("Bake the navigation mesh of '{}'", entity->getName()));
	command->setPrefabOverrides(PrefabOverrideChange::record(*entity, *ioScene, beforeYaml));
	command->captureAfter(*entity);
	ResultData data;
	data.emplace("triangles",
				 static_cast<int64_t>(entity->getComponent<scene::component::NavMeshVolume>().indices.size() / 3));
	return PreparedCommand{.command = std::move(command), .applied = true, .entity = entity->getUUID(), .data = data};
}

}// namespace

void addNavCommands(CommandRegistry& ioRegistry) {
	ioRegistry.add({.name = "nav.bake",
					.description = "Bake the navigation mesh of the Nav Mesh Volume of an entity from the static "
								   "geometry inside its box.",
					.args = {{.name = "entity",
							  .type = ArgType::Entity,
							  .required = true,
							  .description = "Entity holding the Nav Mesh Volume."}},
					.factory = bake});
}

}// namespace owl::nest::commands
