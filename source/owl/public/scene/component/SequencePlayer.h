/**
 * @file SequencePlayer.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "core/UUID.h"
#include "data/sequence/Sequence.h"

#include <cstdint>
#include <string>
#include <vector>

namespace owl::scene::component {

/**
 * @brief
 *  Plays a `.owlseq` cutscene: its camera tracks drive a camera, its event tracks call Lua functions.
 *
 * The keys are in the space of the entity holding the player, of its parent when it drives its own camera: moving it
 * moves the whole cutscene. While it plays, the
 * driven camera is the primary one; the cameras it replaced are primary again when it stops.
 */
struct OWL_API SequencePlayer {
	/// Sequence asset played (`.owlseq`).
	std::string sequence;
	/// Entity holding the driven `Camera` (0: this entity).
	core::UUID camera{0};
	/// Whether the sequence starts with the scene.
	bool playOnStart = true;
	/// Whether the sequence starts again at its end.
	bool loop = false;
	/// Playback speed (1: real time).
	float speed = 1.f;
	/// Lua function called when the sequence ends or is stopped, in the script of this entity, else the scene script.
	std::string onFinished;
	/// Runtime: the start of the scene was handled.
	bool m_started = false;
	/// Runtime: the sequence plays.
	bool m_playing = false;
	/// Runtime: playback time in seconds.
	float m_time = 0.f;
	/// Runtime: the events at the playback time were called (no event is called twice after a seek).
	bool m_eventsDone = false;
	/// Runtime: counts the plays, stops and seeks, so an event that moves the playback ends the frame.
	uint32_t m_generation = 0;
	/// Runtime: the cameras that were primary before the playback.
	std::vector<core::UUID> m_replacedCameras;
	/// Runtime: the sequence read when the playback started.
	shared<const data::sequence::Sequence> m_sequence;

	/**
	 * @brief
	 *  Name of the component.
	 * @return The name.
	 */
	static auto name() -> const char* { return "Sequence Player"; }

	/**
	 * @brief
	 *  Key of the component in scene files.
	 * @return The key.
	 */
	static auto key() -> const char* { return "SequencePlayer"; }

	/**
	 * @brief
	 *  Write the component.
	 * @param[in] iOut The serializer.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read the component.
	 * @param[in] iNode The serializer node.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
