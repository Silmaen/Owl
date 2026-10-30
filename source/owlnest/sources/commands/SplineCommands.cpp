/**
 * @file SplineCommands.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SplineCommands.h"

#include "../EntitySnapshot.h"
#include "CommandRegistry.h"
#include "ComponentCommands.h"
#include "DocumentCommands.h"
#include "PrefabCommands.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/Spline.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace owl::nest::commands {

namespace {

using Prepared = expected<PreparedCommand, CommandFailure>;

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto splineEntity(const scene::Scene& iScene, const CommandArgs& iArgs) -> expected<scene::Entity, CommandFailure> {
	const auto& values = iArgs.getValues();
	const auto found = values.find("entity");
	if (found == values.end())
		return fail(CommandError::MissingArgument, "argument `entity` is missing");
	const auto entity = CommandRegistry::resolveEntity(iScene, found->second);
	if (!entity)
		return fail(CommandError::UnknownEntity, "no entity matches `entity`");
	if (!entity->hasComponent<scene::component::Spline>())
		return fail(CommandError::NotApplicable, std::format("`{}` has no Spline", entity->getName()));
	return *entity;
}

// Edit the points of a spline as one undo entry; the edit returns false (nothing changed) to refuse.
template<typename Edit>
auto editPoints(const shared<scene::Scene>& ioScene, scene::Entity iEntity, const std::string& iDescription,
				Edit&& iEdit) -> Prepared {
	auto before = EntitySnapshot::capture(iEntity);
	const auto beforeYaml = before.yamlData;
	auto& curve = iEntity.getComponent<scene::component::Spline>().curve;
	auto points = curve.getPoints();
	ResultData data;
	if (!std::forward<Edit>(iEdit)(curve, points, data))
		return fail(CommandError::NotApplicable, "nothing changes");
	auto command = mkUniq<ModifyEntityCommand>(iEntity.getUUID(), std::move(before), iDescription);
	command->setPrefabOverrides(PrefabOverrideChange::record(iEntity, *ioScene, beforeYaml));
	command->captureAfter(iEntity);
	return PreparedCommand{.command = std::move(command), .applied = true, .entity = iEntity.getUUID(), .data = data};
}

auto pointIndices(const CommandArgs& iArgs, const size_t iCount) -> expected<std::vector<size_t>, CommandFailure> {
	auto indices = parseIndices(iArgs.getText("points"));
	if (!indices)
		return fail(CommandError::InvalidArgument, "`points` must be indices separated by commas");
	for (const auto index: *indices) {
		if (index >= iCount)
			return fail(CommandError::InvalidArgument, std::format("no point {}", index));
	}
	std::ranges::sort(*indices);
	const auto [first, last] = std::ranges::unique(*indices);
	indices->erase(first, last);
	if (indices->empty())
		return fail(CommandError::InvalidArgument, "`points` names no point");
	return *indices;
}

auto smoothen(math::SplineCurve& ioCurve, const size_t iIndex) -> void {
	if (ioCurve.getInterpolation() != math::SplineInterpolation::Bezier)
		return;
	auto points = ioCurve.getPoints();
	points[iIndex].tangentOut = ioCurve.getSmoothTangent(iIndex);
	points[iIndex].tangentIn = points[iIndex].tangentOut * -1.f;
	ioCurve.setPoints(std::move(points));
}

auto addPoint(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = splineEntity(*ioScene, iArgs);
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto position = iArgs.getVec3("position").value_or(math::vec3{0.f, 0.f, 0.f});
	const auto index = iArgs.get<int64_t>("index").value_or(-1);
	return editPoints(ioScene, *entity, std::format("Add a point to '{}'", entity->getName()),
					  [position, index](math::SplineCurve& ioCurve, std::vector<math::SplinePoint>& ioPoints,
										ResultData& oData) -> bool {
						  const size_t at = index < 0 ? ioPoints.size()
													  : std::min(static_cast<size_t>(index), ioPoints.size());
						  ioPoints.insert(ioPoints.begin() + static_cast<std::ptrdiff_t>(at),
										  math::SplinePoint{.position = position});
						  ioCurve.setPoints(std::move(ioPoints));
						  smoothen(ioCurve, at);
						  oData.emplace("point", static_cast<int64_t>(at));
						  return true;
					  });
}

auto insertPoint(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = splineEntity(*ioScene, iArgs);
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	if (entity->getComponent<scene::component::Spline>().curve.getSegmentCount() == 0)
		return fail(CommandError::NotApplicable, "the spline needs two points to insert one between them");
	const auto distance = iArgs.get<double>("distance").value_or(0.);
	return editPoints(ioScene, *entity, std::format("Insert a point in '{}'", entity->getName()),
					  [distance](math::SplineCurve& ioCurve, std::vector<math::SplinePoint>& /*ioPoints*/,
								 ResultData& oData) -> bool {
						  const size_t index =
								  ioCurve.insertAtParameter(ioCurve.getParameterAtDistance(static_cast<float>(distance)));
						  oData.emplace("point", static_cast<int64_t>(index));
						  return true;
					  });
}

auto deletePoints(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = splineEntity(*ioScene, iArgs);
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto indices =
			pointIndices(iArgs, entity->getComponent<scene::component::Spline>().curve.getPoints().size());
	if (!indices)
		return unexpected<CommandFailure>{indices.error()};
	return editPoints(ioScene, *entity,
					  std::format("Delete {} from '{}'", indices->size() > 1 ? "points" : "a point", entity->getName()),
					  [&indices](math::SplineCurve& ioCurve, std::vector<math::SplinePoint>& ioPoints,
								 ResultData& /*oData*/) -> bool {
						  for (const auto index: *indices | std::views::reverse)
							  ioPoints.erase(ioPoints.begin() + static_cast<std::ptrdiff_t>(index));
						  ioCurve.setPoints(std::move(ioPoints));
						  return true;
					  });
}

auto movePoints(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = splineEntity(*ioScene, iArgs);
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto indices =
			pointIndices(iArgs, entity->getComponent<scene::component::Spline>().curve.getPoints().size());
	if (!indices)
		return unexpected<CommandFailure>{indices.error()};
	const auto offset = iArgs.getVec3("offset").value_or(math::vec3{0.f, 0.f, 0.f});
	if (offset.normSq() <= 0.f)
		return fail(CommandError::NotApplicable, "`offset` is null: nothing moves");
	return editPoints(ioScene, *entity,
					  std::format("Move {} of '{}'", indices->size() > 1 ? "points" : "a point", entity->getName()),
					  [&indices, offset](math::SplineCurve& ioCurve, std::vector<math::SplinePoint>& ioPoints,
										 ResultData& /*oData*/) -> bool {
						  for (const auto index: *indices) ioPoints[index].position = ioPoints[index].position + offset;
						  ioCurve.setPoints(std::move(ioPoints));
						  return true;
					  });
}

auto setPoint(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = splineEntity(*ioScene, iArgs);
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto index = iArgs.get<int64_t>("point").value_or(-1);
	if (index < 0 ||
		std::cmp_greater_equal(index, entity->getComponent<scene::component::Spline>().curve.getPoints().size()))
		return fail(CommandError::InvalidArgument, std::format("no point {}", index));
	return editPoints(ioScene, *entity, std::format("Edit a point of '{}'", entity->getName()),
					  [&iArgs, index](math::SplineCurve& ioCurve, std::vector<math::SplinePoint>& ioPoints,
									  ResultData& /*oData*/) -> bool {
						  auto& point = ioPoints[static_cast<size_t>(index)];
						  const auto before = point;
						  point.position = iArgs.getVec3("position").value_or(point.position);
						  point.tangentIn = iArgs.getVec3("tangent_in").value_or(point.tangentIn);
						  point.tangentOut = iArgs.getVec3("tangent_out").value_or(point.tangentOut);
						  if (point == before)
							  return false;
						  ioCurve.setPoints(std::move(ioPoints));
						  return true;
					  });
}

auto entitySpec() -> ArgSpec {
	return {.name = "entity", .type = ArgType::Entity, .required = true, .description = "Entity holding the Spline."};
}

auto pointsSpec(const char* iDescription) -> ArgSpec {
	return {.name = "points", .type = ArgType::Text, .required = true, .description = iDescription};
}

}// namespace

void addSplineCommands(CommandRegistry& ioRegistry) {
	ioRegistry.add({.name = "spline.add_point",
					.description = "Add a control point to the Spline of an entity, at a position of the entity's "
								   "space (a Bezier point gets smooth handles).",
					.args = {entitySpec(),
							 {.name = "position",
							  .type = ArgType::Vec3,
							  .required = true,
							  .description = "Position, in the space of the entity."},
							 {.name = "index",
							  .type = ArgType::Integer,
							  .description = "Index of the new point, from 0 (appended when absent)."}},
					.factory = addPoint});
	ioRegistry.add({.name = "spline.insert_point",
					.description = "Insert a control point on the Spline of an entity at a distance along it, "
								   "keeping its shape.",
					.args = {entitySpec(),
							 {.name = "distance",
							  .type = ArgType::Number,
							  .required = true,
							  .description = "Distance along the curve, from its first point."}},
					.factory = insertPoint});
	ioRegistry.add({.name = "spline.delete_points",
					.description = "Delete control points of the Spline of an entity.",
					.args = {entitySpec(), pointsSpec("Indices of the points, from 0 (`0,2`).")},
					.factory = deletePoints});
	ioRegistry.add({.name = "spline.move_points",
					.description = "Move control points of the Spline of an entity by an offset of the entity's space.",
					.args = {entitySpec(), pointsSpec("Indices of the points, from 0 (`0,2`)."),
							 {.name = "offset",
							  .type = ArgType::Vec3,
							  .required = true,
							  .description = "Offset added to each point, in the space of the entity."}},
					.factory = movePoints});
	ioRegistry.add({.name = "spline.set_point",
					.description = "Set the position or the Bezier handles of a control point of the Spline of an "
								   "entity.",
					.args = {entitySpec(),
							 {.name = "point",
							  .type = ArgType::Integer,
							  .required = true,
							  .description = "Index of the point, from 0."},
							 {.name = "position",
							  .type = ArgType::Vec3,
							  .description = "Position, in the space of the entity."},
							 {.name = "tangent_in",
							  .type = ArgType::Vec3,
							  .description = "Bezier handle before the point, as an offset from it."},
							 {.name = "tangent_out",
							  .type = ArgType::Vec3,
							  .description = "Bezier handle after the point, as an offset from it."}},
					.factory = setPoint});
}

}// namespace owl::nest::commands
