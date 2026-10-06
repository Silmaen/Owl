/**
 * @file SceneBench.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <scene/Entity.h>
#include <scene/component/components.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace owl::bench {

namespace {

auto shapeName(const Shape iShape) -> const char* {
	switch (iShape) {
		case Shape::Flat:
			return "flat";
		case Shape::Chain:
			return "chain";
		case Shape::Wide:
			return "wide";
		case Shape::Forest:
			return "forest";
	}
	return "unknown";
}

auto fullWalkWorldX(const scene::Scene& iScene, const scene::Entity& iEntity) -> float {
	math::mat4 world = iEntity.getComponent<scene::component::Transform>().transform();
	core::UUID parentId = iEntity.getComponent<scene::component::Hierarchy>().parentId;
	while (parentId != core::UUID{0}) {
		const scene::Entity parent = iScene.findEntityByUUID(parentId);
		if (!parent)
			break;
		world = parent.getComponent<scene::component::Transform>().transform() * world;
		parentId = parent.getComponent<scene::component::Hierarchy>().parentId;
	}
	return world(0, 3);
}

void runCreation(Runner& ioRunner) {
	shared<scene::Scene> holder;
	for (const uint32_t count: {1000U, 10000U, 100000U}) {
		ioRunner.measureWithSetup(
				std::format("scene/create_entities/{}", count), count,
				[&]() -> void {
					holder.reset();
					holder = mkShared<scene::Scene>();
				},
				[&]() -> void {
					for (uint32_t i = 0; i < count; ++i) doNotOptimize(holder->createEntity("e"));
				});
	}
	holder.reset();
	if (ioRunner.wants("scene/memory")) {
		const size_t before = allocatedBytes();
		auto big = makeSpriteScene(100000, Shape::Flat);
		const size_t after = allocatedBytes();
		ioRunner.metric("scene/memory/bytes_per_sprite_entity", static_cast<double>(after - before) / 100000.0,
						"B/entity");
	}
}

void runWorldTransforms(Runner& ioRunner) {
	for (const auto& [count, shape]: {std::pair{1000U, Shape::Chain}, std::pair{10000U, Shape::Wide},
									  std::pair{10000U, Shape::Flat}, std::pair{10000U, Shape::Forest}}) {
		const std::string tag = std::format("{}{}", shapeName(shape), count);
		if (!ioRunner.wants(std::format("scene/world_transform/{}", tag)) &&
			!ioRunner.wants(std::format("scene/prepare_world_transforms/{}", tag)))
			continue;
		const auto scn = makeSpriteScene(count, shape);
		const auto entities = scn->getAllEntities();
		ioRunner.measure(std::format("scene/world_transform/{}_all", tag), count, [&]() -> void {
			for (const auto& ent: entities) doNotOptimize(scn->getWorldTransform(ent));
		});
		ioRunner.measure(std::format("scene/prepare_world_transforms/{}", tag), count,
						 [&]() -> void { scn->prepareWorldTransforms(); });
		if (shape == Shape::Chain) {
			scene::Entity leaf;
			for (const auto& ent: entities)
				if (ent.getComponent<scene::component::Hierarchy>().childrenIds.empty())
					leaf = ent;
			ioRunner.measure(std::format("scene/world_transform/{}_leaf", tag), 1,
							 [&]() -> void { doNotOptimize(scn->getWorldTransform(leaf)); });
			ioRunner.metric(std::format("scene/world_transform/{}_leaf_x_engine", tag),
							static_cast<double>(scn->getWorldTransform(leaf).translation().x()), "(expected 1000)");
			ioRunner.metric(std::format("scene/world_transform/{}_leaf_x_full_walk", tag),
							static_cast<double>(fullWalkWorldX(*scn, leaf)), "(expected 1000)");
		}
	}
}

void runSetParent(Runner& ioRunner) {
	if (!ioRunner.wants("scene/set_parent"))
		return;
	shared<scene::Scene> scn;
	std::vector<scene::Entity> ents;
	for (const auto& [count, shape]: {std::pair{1000U, Shape::Chain}, std::pair{10000U, Shape::Wide}}) {
		ioRunner.measureWithSetup(
				std::format("scene/set_parent/{}{}_build", shapeName(shape), count), count,
				[&]() -> void {
					scn = mkShared<scene::Scene>();
					ents.clear();
					for (uint32_t i = 0; i < count; ++i) {
						ents.push_back(scn->createEntity("e"));
						ents.back().getComponent<scene::component::Transform>().transform.translation().x() =
								static_cast<float>(i);
					}
				},
				[&]() -> void {
					for (uint32_t i = 1; i < count; ++i)
						scn->setParent(ents[i], shape == Shape::Chain ? ents[i - 1] : ents[0]);
				});
		if (shape == Shape::Chain)
			ioRunner.metric("scene/set_parent/chain1000_leaf_x_after_reparent",
							static_cast<double>(fullWalkWorldX(*scn, ents.back())),
							"(expected 999, world position must be preserved)");
	}
}

void runViewsAndCopy(Runner& ioRunner) {
	for (const uint32_t count: {1000U, 10000U, 100000U}) {
		const std::string tag = std::format("{}", count);
		if (!ioRunner.wants("scene/view_iterate") && !ioRunner.wants("scene/copy") && !ioRunner.wants("scene/find"))
			return;
		const auto scn = makeSpriteScene(count, Shape::Forest);
		ioRunner.measure(std::format("scene/view_iterate/transform_sprite/{}", tag), count, [&]() -> void {
			float sum = 0.f;
			for (const auto view = scn->registry.view<scene::component::Transform, scene::component::SpriteRenderer>();
				 const auto e: view) {
				const auto& [transform, sprite] =
						view.get<scene::component::Transform, scene::component::SpriteRenderer>(e);
				sum += transform.transform.translation().x() * sprite.color.x();
			}
			doNotOptimize(sum);
		});
		std::vector<core::UUID> uuids;
		for (const auto& ent: scn->getAllEntities()) uuids.push_back(ent.getUUID());
		ioRunner.measure(std::format("scene/find_entity_by_uuid/{}", tag), count, [&]() -> void {
			for (const auto& uuid: uuids) doNotOptimize(scn->findEntityByUUID(uuid));
		});
		if (count > 10000U)
			continue;
		shared<scene::Scene> copy;
		ioRunner.measureWithSetup(
				std::format("scene/copy/{}", tag), count, [&]() -> void { copy.reset(); },
				[&]() -> void { copy = scene::Scene::copy(scn); });
	}
}

}// namespace

auto makeSpriteScene(const uint32_t iCount, const Shape iShape) -> shared<scene::Scene> {
	auto scn = mkShared<scene::Scene>();
	std::vector<scene::Entity> ents;
	ents.reserve(iCount);
	for (uint32_t i = 0; i < iCount; ++i) {
		auto ent = scn->createEntity(std::format("sprite_{}", i));
		ent.getComponent<scene::component::Transform>().transform.translation().x() = 1.f;
		ent.addComponent<scene::component::SpriteRenderer>().color = {0.5f, 0.6f, 0.7f, 1.f};
		ents.push_back(ent);
	}
	const auto link = [&](const uint32_t iChild, const uint32_t iParent) -> void {
		ents[iChild].getComponent<scene::component::Hierarchy>().parentId = ents[iParent].getUUID();
		ents[iParent].getComponent<scene::component::Hierarchy>().childrenIds.push_back(ents[iChild].getUUID());
	};
	for (uint32_t i = 1; i < iCount; ++i) {
		switch (iShape) {
			case Shape::Flat:
				break;
			case Shape::Chain:
				link(i, i - 1);
				break;
			case Shape::Wide:
				link(i, 0);
				break;
			case Shape::Forest:
				if (i % 100 != 0)
					link(i, i - i % 100);
				break;
		}
	}
	return scn;
}

void runSceneBenches(Runner& ioRunner) {
	runCreation(ioRunner);
	runWorldTransforms(ioRunner);
	runSetParent(ioRunner);
	runViewsAndCopy(ioRunner);
}

}// namespace owl::bench
