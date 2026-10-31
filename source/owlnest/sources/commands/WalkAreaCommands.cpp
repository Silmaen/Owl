/**
 * @file WalkAreaCommands.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "WalkAreaCommands.h"

#include "../EntitySnapshot.h"
#include "CommandRegistry.h"
#include "ComponentCommands.h"
#include "PrefabCommands.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/WalkArea.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace owl::nest::commands {

using Vertex = gui::component::WalkAreaSelection::Vertex;

auto formatWalkVertices(const std::span<const Vertex> iVertices) -> std::string {
	std::string text;
	for (const auto& vertex: iVertices)
		text += std::format("{}{}:{}:{}", text.empty() ? "" : ",", vertex.polygon, vertex.ring, vertex.index);
	return text;
}

auto parseWalkVertices(const std::string_view iText) -> std::optional<std::vector<Vertex>> {
	std::vector<Vertex> vertices;
	for (const auto part: iText | std::views::split(',')) {
		std::string_view item{part.begin(), part.end()};
		while (!item.empty() && item.front() == ' ') item.remove_prefix(1);
		while (!item.empty() && item.back() == ' ') item.remove_suffix(1);
		if (item.empty())
			continue;
		std::array<size_t, 3> values{};
		size_t field = 0;
		for (const auto number: item | std::views::split(':')) {
			const std::string_view digits{number.begin(), number.end()};
			if (field >= values.size())
				return std::nullopt;
			if (const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), values[field]);
				error != std::errc{} || end != digits.data() + digits.size() || digits.empty())
				return std::nullopt;
			++field;
		}
		if (field != values.size())
			return std::nullopt;
		vertices.push_back({.polygon = values[0], .ring = values[1], .index = values[2]});
	}
	return vertices;
}

namespace {

using Prepared = expected<PreparedCommand, CommandFailure>;
using WalkArea = scene::component::WalkArea;

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto areaEntity(const scene::Scene& iScene, const CommandArgs& iArgs) -> expected<scene::Entity, CommandFailure> {
	const auto& values = iArgs.getValues();
	const auto found = values.find("entity");
	if (found == values.end())
		return fail(CommandError::MissingArgument, "argument `entity` is missing");
	const auto entity = CommandRegistry::resolveEntity(iScene, found->second);
	if (!entity)
		return fail(CommandError::UnknownEntity, "no entity matches `entity`");
	if (!entity->hasComponent<WalkArea>())
		return fail(CommandError::NotApplicable, std::format("`{}` has no Walk Area", entity->getName()));
	return *entity;
}

// Edit the floors of an area as one undo entry; the edit returns false (nothing changed) to refuse.
template<typename Edit>
auto editArea(const shared<scene::Scene>& ioScene, scene::Entity iEntity, const std::string& iDescription, Edit&& iEdit)
		-> Prepared {
	auto before = EntitySnapshot::capture(iEntity);
	const auto beforeYaml = before.yamlData;
	auto& area = iEntity.getComponent<WalkArea>();
	const auto polygons = area.polygons;
	ResultData data;
	if (!std::forward<Edit>(iEdit)(area, data)) {
		area.polygons = polygons;
		return fail(CommandError::NotApplicable, "nothing changes");
	}
	auto command = mkUniq<ModifyEntityCommand>(iEntity.getUUID(), std::move(before), iDescription);
	command->setPrefabOverrides(PrefabOverrideChange::record(iEntity, *ioScene, beforeYaml));
	command->captureAfter(iEntity);
	return PreparedCommand{.command = std::move(command), .applied = true, .entity = iEntity.getUUID(), .data = data};
}

auto square(const math::vec2& iCenter, const float iHalf) -> std::vector<math::vec2> {
	return {iCenter + math::vec2{-iHalf, -iHalf}, iCenter + math::vec2{iHalf, -iHalf},
			iCenter + math::vec2{iHalf, iHalf}, iCenter + math::vec2{-iHalf, iHalf}};
}

auto planar(const math::vec3& iPoint) -> math::vec2 { return {iPoint.x(), iPoint.y()}; }

auto readIndex(const CommandArgs& iArgs, const char* iName) -> std::optional<size_t> {
	const auto value = iArgs.get<int64_t>(iName).value_or(-1);
	if (value < 0)
		return std::nullopt;
	return static_cast<size_t>(value);
}

auto readVertices(const CommandArgs& iArgs, const WalkArea& iArea) -> expected<std::vector<Vertex>, CommandFailure> {
	auto vertices = parseWalkVertices(iArgs.getText("vertices"));
	if (!vertices)
		return fail(CommandError::InvalidArgument, "`vertices` must be `polygon:ring:index` items separated by commas");
	for (const auto& vertex: *vertices) {
		const auto* ring = iArea.getRing(vertex.polygon, vertex.ring);
		if (ring == nullptr || vertex.index >= ring->size())
			return fail(CommandError::InvalidArgument,
						std::format("no vertex {}:{}:{}", vertex.polygon, vertex.ring, vertex.index));
	}
	std::ranges::sort(*vertices);
	const auto [first, last] = std::ranges::unique(*vertices);
	vertices->erase(first, last);
	if (vertices->empty())
		return fail(CommandError::InvalidArgument, "`vertices` names no vertex");
	return *vertices;
}

auto addPolygon(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = areaEntity(*ioScene, iArgs);
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto center = planar(iArgs.getVec3("position").value_or(math::vec3{0.f, 0.f, 0.f}));
	const auto size = static_cast<float>(iArgs.get<double>("size").value_or(4.0));
	if (size <= 0.f)
		return fail(CommandError::InvalidArgument, "`size` must be positive");
	return editArea(ioScene, *entity, std::format("Add a floor to '{}'", entity->getName()),
					[center, size](WalkArea& ioArea, ResultData& oData) -> bool {
						ioArea.polygons.push_back({.outline = square(center, size * 0.5f), .holes = {}});
						oData.emplace("polygon", static_cast<int64_t>(ioArea.polygons.size() - 1));
						return true;
					});
}

auto addHole(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = areaEntity(*ioScene, iArgs);
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto polygon = readIndex(iArgs, "polygon");
	if (!polygon || *polygon >= entity->getComponent<WalkArea>().polygons.size())
		return fail(CommandError::InvalidArgument, "no such floor");
	const auto center = planar(iArgs.getVec3("position").value_or(math::vec3{0.f, 0.f, 0.f}));
	const auto size = static_cast<float>(iArgs.get<double>("size").value_or(1.0));
	if (size <= 0.f)
		return fail(CommandError::InvalidArgument, "`size` must be positive");
	return editArea(ioScene, *entity, std::format("Add a hole to '{}'", entity->getName()),
					[&polygon, center, size](WalkArea& ioArea, ResultData& oData) -> bool {
						auto& holes = ioArea.polygons[*polygon].holes;
						holes.push_back(square(center, size * 0.5f));
						oData.emplace("ring", static_cast<int64_t>(holes.size()));
						return true;
					});
}

auto deleteRing(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = areaEntity(*ioScene, iArgs);
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto polygon = readIndex(iArgs, "polygon");
	const auto ring = readIndex(iArgs, "ring").value_or(0);
	if (!polygon || entity->getComponent<WalkArea>().getRing(*polygon, ring) == nullptr)
		return fail(CommandError::InvalidArgument, "no such ring");
	return editArea(ioScene, *entity,
					std::format("Delete a {} of '{}'", ring == 0 ? "floor" : "hole", entity->getName()),
					[&polygon, ring](WalkArea& ioArea, ResultData& /*oData*/) -> bool {
						if (ring == 0)
							ioArea.polygons.erase(ioArea.polygons.begin() + static_cast<std::ptrdiff_t>(*polygon));
						else {
							auto& holes = ioArea.polygons[*polygon].holes;
							holes.erase(holes.begin() + static_cast<std::ptrdiff_t>(ring - 1));
						}
						return true;
					});
}

auto insertVertex(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = areaEntity(*ioScene, iArgs);
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto polygon = readIndex(iArgs, "polygon");
	const auto ring = readIndex(iArgs, "ring").value_or(0);
	const auto index = readIndex(iArgs, "index");
	const auto* points = polygon ? entity->getComponent<WalkArea>().getRing(*polygon, ring) : nullptr;
	if (points == nullptr || !index || *index > points->size())
		return fail(CommandError::InvalidArgument, "no such ring or index");
	const auto position = planar(iArgs.getVec3("position").value_or(math::vec3{0.f, 0.f, 0.f}));
	return editArea(ioScene, *entity, std::format("Insert a vertex in '{}'", entity->getName()),
					[&polygon, ring, &index, position](WalkArea& ioArea, ResultData& oData) -> bool {
						auto& target = *ioArea.getRing(*polygon, ring);
						target.insert(target.begin() + static_cast<std::ptrdiff_t>(*index), position);
						oData.emplace("vertex", static_cast<int64_t>(*index));
						return true;
					});
}

auto moveVertices(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = areaEntity(*ioScene, iArgs);
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto vertices = readVertices(iArgs, entity->getComponent<WalkArea>());
	if (!vertices)
		return unexpected<CommandFailure>{vertices.error()};
	const auto offset = planar(iArgs.getVec3("offset").value_or(math::vec3{0.f, 0.f, 0.f}));
	if (offset.normSq() <= 0.f)
		return fail(CommandError::NotApplicable, "`offset` is null: nothing moves");
	return editArea(ioScene, *entity,
					std::format("Move {} of '{}'", vertices->size() > 1 ? "vertices" : "a vertex", entity->getName()),
					[&vertices, offset](WalkArea& ioArea, ResultData& /*oData*/) -> bool {
						for (const auto& vertex: *vertices) {
							auto& point = (*ioArea.getRing(vertex.polygon, vertex.ring))[vertex.index];
							point = point + offset;
						}
						return true;
					});
}

auto setVertex(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = areaEntity(*ioScene, iArgs);
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto polygon = readIndex(iArgs, "polygon");
	const auto ring = readIndex(iArgs, "ring").value_or(0);
	const auto index = readIndex(iArgs, "index");
	const auto* points = polygon ? entity->getComponent<WalkArea>().getRing(*polygon, ring) : nullptr;
	if (points == nullptr || !index || *index >= points->size())
		return fail(CommandError::InvalidArgument, "no such vertex");
	const auto position = iArgs.getVec3("position");
	if (!position)
		return fail(CommandError::MissingArgument, "argument `position` is missing");
	return editArea(ioScene, *entity, std::format("Edit a vertex of '{}'", entity->getName()),
					[&polygon, ring, &index, &position](WalkArea& ioArea, ResultData& /*oData*/) -> bool {
						auto& point = (*ioArea.getRing(*polygon, ring))[*index];
						const math::vec2 target = planar(*position);
						if ((point - target).normSq() <= 0.f)
							return false;
						point = target;
						return true;
					});
}

auto deleteVertices(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = areaEntity(*ioScene, iArgs);
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto vertices = readVertices(iArgs, entity->getComponent<WalkArea>());
	if (!vertices)
		return unexpected<CommandFailure>{vertices.error()};
	return editArea(ioScene, *entity,
					std::format("Delete {} of '{}'", vertices->size() > 1 ? "vertices" : "a vertex", entity->getName()),
					[&vertices](WalkArea& ioArea, ResultData& /*oData*/) -> bool {
						// From the last: the indices of the vertices left to delete stay valid.
						for (const auto& vertex: *vertices | std::views::reverse) {
							auto& ring = *ioArea.getRing(vertex.polygon, vertex.ring);
							ring.erase(ring.begin() + static_cast<std::ptrdiff_t>(vertex.index));
						}
						// A ring left with fewer than three vertices goes: a hole, or the floor for its outline.
						for (auto& polygon: ioArea.polygons)
							std::erase_if(polygon.holes, [](const std::vector<math::vec2>& iHole) -> bool {
								return iHole.size() < 3;
							});
						std::erase_if(ioArea.polygons, [](const scene::nav::WalkPolygon& iPolygon) -> bool {
							return iPolygon.outline.size() < 3;
						});
						return true;
					});
}

auto entitySpec() -> ArgSpec {
	return {.name = "entity",
			.type = ArgType::Entity,
			.required = true,
			.description = "Entity holding the Walk Area."};
}

auto polygonSpec() -> ArgSpec {
	return {.name = "polygon",
			.type = ArgType::Integer,
			.required = true,
			.description = "Index of the floor, from 0."};
}

auto ringSpec() -> ArgSpec {
	return {.name = "ring",
			.type = ArgType::Integer,
			.description = "0 for the outline of the floor (the default), 1 + n for its hole n."};
}

auto verticesSpec() -> ArgSpec {
	return {.name = "vertices",
			.type = ArgType::Text,
			.required = true,
			.description = "Vertices as `polygon:ring:index` items separated by commas (`0:0:2,0:1:0`)."};
}

}// namespace

void addWalkAreaCommands(CommandRegistry& ioRegistry) {
	ioRegistry.add({.name = "walk.add_polygon",
					.description = "Add a square floor to the Walk Area of an entity, centred on a position of the "
								   "entity's space.",
					.args = {entitySpec(),
							 {.name = "position",
							  .type = ArgType::Vec3,
							  .required = true,
							  .description = "Centre, in the space of the entity (z is ignored)."},
							 {.name = "size", .type = ArgType::Number, .description = "Side of the square (4)."}},
					.factory = addPolygon});
	ioRegistry.add({.name = "walk.add_hole",
					.description = "Cut a square hole in a floor of the Walk Area of an entity.",
					.args = {entitySpec(),
							 polygonSpec(),
							 {.name = "position",
							  .type = ArgType::Vec3,
							  .required = true,
							  .description = "Centre, in the space of the entity (z is ignored)."},
							 {.name = "size", .type = ArgType::Number, .description = "Side of the square (1)."}},
					.factory = addHole});
	ioRegistry.add({.name = "walk.delete_ring",
					.description = "Delete a floor (ring 0) or one of its holes from the Walk Area of an entity.",
					.args = {entitySpec(), polygonSpec(), ringSpec()},
					.factory = deleteRing});
	ioRegistry.add({.name = "walk.insert_vertex",
					.description = "Insert a vertex in a ring of the Walk Area of an entity.",
					.args = {entitySpec(),
							 polygonSpec(),
							 ringSpec(),
							 {.name = "index",
							  .type = ArgType::Integer,
							  .required = true,
							  .description = "Index of the new vertex in the ring, from 0."},
							 {.name = "position",
							  .type = ArgType::Vec3,
							  .required = true,
							  .description = "Position, in the space of the entity (z is ignored)."}},
					.factory = insertVertex});
	ioRegistry.add(
			{.name = "walk.move_vertices",
			 .description = "Move vertices of the Walk Area of an entity by an offset of the entity's space.",
			 .args = {entitySpec(),
					  verticesSpec(),
					  {.name = "offset",
					   .type = ArgType::Vec3,
					   .required = true,
					   .description = "Offset added to each vertex, in the space of the entity (z is ignored)."}},
			 .factory = moveVertices});
	ioRegistry.add({.name = "walk.set_vertex",
					.description = "Set the position of a vertex of the Walk Area of an entity.",
					.args = {entitySpec(),
							 polygonSpec(),
							 ringSpec(),
							 {.name = "index",
							  .type = ArgType::Integer,
							  .required = true,
							  .description = "Index of the vertex in the ring, from 0."},
							 {.name = "position",
							  .type = ArgType::Vec3,
							  .required = true,
							  .description = "Position, in the space of the entity (z is ignored)."}},
					.factory = setVertex});
	ioRegistry.add({.name = "walk.delete_vertices",
					.description = "Delete vertices of the Walk Area of an entity; a ring left with fewer than three "
								   "goes with them (the floor for its outline).",
					.args = {entitySpec(), verticesSpec()},
					.factory = deleteVertices});
}

}// namespace owl::nest::commands
