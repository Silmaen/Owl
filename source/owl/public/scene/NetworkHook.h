/**
 * @file NetworkHook.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Timestep.h"

#include <cstdint>

namespace owl::scene {

class Scene;

/**
 * @brief
 *  What a network session does at the steps of a scene's runtime (implemented by `network::GameSession`).
 *
 * The scene takes the hook of its `app::EngineContext` when it starts its runtime, so a session goes on across the
 * levels of a game.
 */
class OWL_API NetworkHook {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	NetworkHook() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~NetworkHook();

	/**
	 * @brief
	 *  Copy constructor (deleted).
	 */
	NetworkHook(const NetworkHook&) = delete;

	/**
	 * @brief
	 *  Move constructor (deleted).
	 */
	NetworkHook(NetworkHook&&) = delete;

	/**
	 * @brief
	 *  Copy assignment (deleted).
	 * @return This hook.
	 */
	auto operator=(const NetworkHook&) -> NetworkHook& = delete;

	/**
	 * @brief
	 *  Move assignment (deleted).
	 * @return This hook.
	 */
	auto operator=(NetworkHook&&) -> NetworkHook& = delete;

	/**
	 * @brief
	 *  The scene starts its runtime: before its physics, its sounds and its scripts start.
	 * @param[in,out] ioScene The scene.
	 */
	virtual void onRuntimeStart(Scene& ioScene) = 0;

	/**
	 * @brief
	 *  A runtime frame begins, before the simulation clock counts its ticks.
	 * @param[in,out] ioScene The scene.
	 * @param[in] iTimeStep The frame duration.
	 */
	virtual void onFrameBegin(Scene& ioScene, const core::Timestep& iTimeStep) = 0;

	/**
	 * @brief
	 *  A fixed tick begins, its input sampled, before its systems run.
	 * @param[in,out] ioScene The scene.
	 * @param[in] iTick Number of the tick since the runtime started (0 for the first one).
	 */
	virtual void onTick(Scene& ioScene, uint64_t iTick) = 0;

	/**
	 * @brief
	 *  The world of the frame is settled (after the `Late` systems), before it is drawn.
	 * @param[in,out] ioScene The scene.
	 */
	virtual void onFrameEnd(Scene& ioScene) = 0;
};

}// namespace owl::scene
