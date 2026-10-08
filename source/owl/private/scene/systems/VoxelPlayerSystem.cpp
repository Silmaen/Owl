/**
 * @file VoxelPlayerSystem.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EngineSystems.h"

#include "app/Application.h"
#include "data/voxel/VoxelCollision.h"
#include "data/voxel/VoxelRaycast.h"
#include "input/Input.h"
#include "input/MouseCode.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"
#include "window/Window.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace owl::scene::systems {

namespace {

void applyPlayerLook(component::VoxelPlayer& ioPlayer, math::Transform& ioTransform, const float iDt,
					 const bool iCursorCaptured) {
	if (iCursorCaptured) {
		const math::vec2 mouse = input::Input::getMousePos();
		if (ioPlayer.mouseValid) {
			ioPlayer.yaw -= (mouse.x() - ioPlayer.lastMouse.x()) * ioPlayer.mouseSensitivity;
			ioPlayer.pitch -= (mouse.y() - ioPlayer.lastMouse.y()) * ioPlayer.mouseSensitivity;
		}
		ioPlayer.lastMouse = mouse;
		ioPlayer.mouseValid = true;
	} else {
		ioPlayer.mouseValid = false;
	}
	if (input::Input::isKeyPressed(input::key::Left))
		ioPlayer.yaw += ioPlayer.lookSpeed * iDt;
	if (input::Input::isKeyPressed(input::key::Right))
		ioPlayer.yaw -= ioPlayer.lookSpeed * iDt;
	if (input::Input::isKeyPressed(input::key::Up))
		ioPlayer.pitch += ioPlayer.lookSpeed * iDt;
	if (input::Input::isKeyPressed(input::key::Down))
		ioPlayer.pitch -= ioPlayer.lookSpeed * iDt;
	ioPlayer.pitch = std::clamp(ioPlayer.pitch, -1.5f, 1.5f);
	ioTransform.rotation() = math::vec3{ioPlayer.pitch, ioPlayer.yaw, 0.f};
}

auto updatePlayerModes(component::VoxelPlayer& ioPlayer, const float iDt) -> std::string {
	std::string toast;
	ioPlayer.doubleTapTimer = std::max(0.f, ioPlayer.doubleTapTimer - iDt);
	const bool space = input::Input::isKeyPressed(input::key::Space);
	if (const bool spaceEdge = space && !ioPlayer.spaceWasPressed; spaceEdge) {
		if (ioPlayer.doubleTapTimer > 0.f) {
			ioPlayer.flyMode = !ioPlayer.flyMode;
			ioPlayer.velocityY = 0.f;
			ioPlayer.doubleTapTimer = 0.f;
			if (!ioPlayer.flyMode)
				ioPlayer.superSpeed = false;
			toast = ioPlayer.flyMode ? "Fly mode ON" : "Fly mode OFF";
		} else {
			ioPlayer.doubleTapTimer = 0.3f;
		}
	}
	ioPlayer.spaceWasPressed = space;
	const bool superKey = input::Input::isKeyPressed(input::key::J);
	if (const bool superEdge = superKey && !ioPlayer.superSpeedWasPressed; superEdge && ioPlayer.flyMode) {
		ioPlayer.superSpeed = !ioPlayer.superSpeed;
		toast = ioPlayer.superSpeed ? "Super speed ON" : "Super speed OFF";
	}
	ioPlayer.superSpeedWasPressed = superKey;
	return toast;
}

auto cellOverlapsBox(const math::vec3i& iCell, const math::vec3& iCenter, const math::vec3& iHalf) -> bool {
	const auto cx = static_cast<float>(iCell.x());
	const auto cy = static_cast<float>(iCell.y());
	const auto cz = static_cast<float>(iCell.z());
	return iCenter.x() + iHalf.x() > cx && iCenter.x() - iHalf.x() < cx + 1.f && iCenter.y() + iHalf.y() > cy &&
		   iCenter.y() - iHalf.y() < cy + 1.f && iCenter.z() + iHalf.z() > cz && iCenter.z() - iHalf.z() < cz + 1.f;
}

void applyPlayerMovement(component::VoxelPlayer& ioPlayer, math::Transform& ioTransform, const float iDt,
						 const data::voxel::SolidPredicate& iIsSolid) {
	const float sy = std::sin(ioPlayer.yaw);
	const float cy = std::cos(ioPlayer.yaw);
	math::vec3 move{0.f, 0.f, 0.f};
	if (input::Input::isKeyPressed(input::key::W))
		move += math::vec3{-sy, 0.f, -cy};
	if (input::Input::isKeyPressed(input::key::S))
		move -= math::vec3{-sy, 0.f, -cy};
	if (input::Input::isKeyPressed(input::key::D))
		move += math::vec3{cy, 0.f, -sy};
	if (input::Input::isKeyPressed(input::key::A))
		move -= math::vec3{cy, 0.f, -sy};
	const float horizLen = std::sqrt(move.x() * move.x() + move.z() * move.z());
	const bool space = input::Input::isKeyPressed(input::key::Space);
	math::vec3 velocity{0.f, 0.f, 0.f};
	if (ioPlayer.flyMode) {
		const float speed = ioPlayer.flySpeed * (ioPlayer.superSpeed ? ioPlayer.superSpeedMultiplier : 1.f);
		if (horizLen > 0.f) {
			velocity.x() = move.x() / horizLen * speed;
			velocity.z() = move.z() / horizLen * speed;
		}
		if (space)
			velocity.y() += speed;
		if (input::Input::isKeyPressed(input::key::LeftShift))
			velocity.y() -= speed;
		ioPlayer.velocityY = 0.f;
		ioPlayer.grounded = false;
	} else {
		const float speed = input::Input::isKeyPressed(input::key::LeftShift) ? ioPlayer.runSpeed : ioPlayer.walkSpeed;
		if (horizLen > 0.f) {
			velocity.x() = move.x() / horizLen * speed;
			velocity.z() = move.z() / horizLen * speed;
		}
		if (ioPlayer.grounded && space)
			ioPlayer.velocityY = ioPlayer.jumpSpeed;
		ioPlayer.velocityY -= ioPlayer.gravity * iDt;
		velocity.y() = ioPlayer.velocityY;
	}
	const auto result =
			data::voxel::moveAabb(iIsSolid, ioTransform.translation(), ioPlayer.halfExtents, velocity * iDt);
	ioTransform.translation() = result.position;
	if (!ioPlayer.flyMode) {
		if (result.onGround) {
			ioPlayer.velocityY = 0.f;
			ioPlayer.grounded = true;
		} else {
			ioPlayer.grounded = false;
			if (result.hitCeiling)
				ioPlayer.velocityY = 0.f;
		}
	}
}


void updatePlayerInteraction(Scene& ioScene, component::VoxelPlayer& ioPlayer, const math::vec3& iEye,
							 const bool iCursorCaptured) {
	const float cp = std::cos(ioPlayer.pitch);
	const math::vec3 forward{-cp * std::sin(ioPlayer.yaw), std::sin(ioPlayer.pitch), -cp * std::cos(ioPlayer.yaw)};
	const auto worlds = ioScene.registry.view<component::VoxelWorld>();
	const auto targetable = [&](const int32_t iX, const int32_t iY, const int32_t iZ) -> bool {
		const math::vec3i block{iX, iY, iZ};
		return std::ranges::any_of(worlds, [&](const auto entity) -> bool {
			return !worlds.get<component::VoxelWorld>(entity).registry.isAir(
					worlds.get<component::VoxelWorld>(entity).world.getBlock(block));
		});
	};
	const auto hit = data::voxel::raycastVoxel(targetable, iEye, forward, ioPlayer.reach);
	ioPlayer.hasTarget = hit.has_value();
	if (hit) {
		ioPlayer.targetBlock = hit->block;
		ioScene.setEditorVoxelHighlight(true, hit->block, hit->normal);
	}

	const bool leftHeld = input::Input::isMouseButtonPressed(input::mouse::ButtonLeft);
	const bool rightHeld = input::Input::isMouseButtonPressed(input::mouse::ButtonRight);
	bool breakEdge = false;
	bool placeEdge = false;
	if (iCursorCaptured) {
		breakEdge = leftHeld && !ioPlayer.breakWasPressed;
		placeEdge = rightHeld && !ioPlayer.placeWasPressed;
	}
	// Track the raw button state (even uncaptured) so the click that captures the cursor is never read as an edit edge.
	ioPlayer.breakWasPressed = leftHeld;
	ioPlayer.placeWasPressed = rightHeld;
	if (!hit || !(breakEdge || placeEdge))
		return;

	for (const auto entity: worlds) {
		auto& vw = worlds.get<component::VoxelWorld>(entity);
		if (vw.registry.isAir(vw.world.getBlock(hit->block)))
			continue;
		if (breakEdge) {
			vw.world.setBlock(hit->block, data::voxel::g_AirBlock);
			vw.world.markNeighborChunksDirty(hit->block);
		} else {
			const math::vec3i target{hit->block.x() + hit->normal.x(), hit->block.y() + hit->normal.y(),
									 hit->block.z() + hit->normal.z()};
			if (vw.registry.isAir(vw.world.getBlock(target)) && !cellOverlapsBox(target, iEye, ioPlayer.halfExtents)) {
				vw.world.setBlock(target, ioPlayer.placeBlock);
				vw.world.markNeighborChunksDirty(target);
			}
		}
		break;
	}
}

}// namespace

void updateVoxelPlayers(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	const float dt = iContext.timeStep.getSeconds();
	if (dt <= 0.f)
		return;
	ioScene.setEditorVoxelHighlight(false);
	ioScene.setShowCrosshair(false);
	const auto players = ioScene.registry.view<component::Transform, component::VoxelPlayer>();
	if (players.begin() == players.end())
		return;
	// A voxel player is active this frame: show the aiming crosshair whether or not the cursor is captured.
	ioScene.setShowCrosshair(true);

	const auto isSolid = [&ioScene](const int32_t iBx, const int32_t iBy, const int32_t iBz) -> bool {
		const math::vec3i block{iBx, iBy, iBz};
		const auto view = ioScene.registry.view<component::VoxelWorld>();
		return std::ranges::any_of(view, [&](const auto entity) -> bool {
			const auto& vw = view.get<component::VoxelWorld>(entity);
			const auto id = vw.world.getBlock(block);
			return id != data::voxel::g_AirBlock && vw.registry.get(id).solid;
		});
	};
	const auto hasChunk = [&ioScene](const math::vec3& iPos) -> bool {
		const math::vec3i block{static_cast<int32_t>(std::floor(iPos.x())), static_cast<int32_t>(std::floor(iPos.y())),
								static_cast<int32_t>(std::floor(iPos.z()))};
		const math::vec3i coord = data::voxel::worldToChunk(block);
		const auto view = ioScene.registry.view<component::VoxelWorld>();
		return std::ranges::any_of(view, [&](const auto entity) -> bool {
			return view.get<component::VoxelWorld>(entity).world.getChunk(coord) != nullptr;
		});
	};

	const bool cursorCaptured = app::Application::get().getWindow().getCursorMode() == window::CursorMode::Disabled;
	for (const auto entity: players) {
		auto [transform, player] = players.get<component::Transform, component::VoxelPlayer>(entity);
		auto& tr = transform.transform;
		if (!player.initialized) {
			player.yaw = tr.rotation().y();
			player.pitch = tr.rotation().x();
			player.initialized = true;
		}
		applyPlayerLook(player, tr, dt, cursorCaptured);
		if (const auto toast = updatePlayerModes(player, dt); !toast.empty())
			ioScene.showToast(toast);
		updatePlayerInteraction(ioScene, player, tr.translation(), cursorCaptured);

		// Hold still until the player's chunk has streamed in, so it never falls through an ungenerated world.
		if (!hasChunk(tr.translation())) {
			player.velocityY = 0.f;
			continue;
		}
		applyPlayerMovement(player, tr, dt, isSolid);
	}
}

}// namespace owl::scene::systems
