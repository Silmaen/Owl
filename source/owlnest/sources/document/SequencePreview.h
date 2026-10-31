/**
 * @file SequencePreview.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <math/matrices.h>
#include <scene/Entity.h>

#include <filesystem>
#include <optional>
#include <string>

namespace owl::scene {
class Scene;
}// namespace owl::scene

namespace owl::nest {

/**
 * @brief
 *  The `.owlseq` a sequence player names, on disk in the asset folders of the application.
 * @param[in] iAsset The `sequence` field of a `SequencePlayer`.
 * @return The normalised path, or nothing when the asset is empty or missing.
 */
[[nodiscard]] auto getSequenceDiskPath(const std::string& iAsset) -> std::optional<std::filesystem::path>;

/**
 * @brief
 *  Whether an entity has a `SequencePlayer` playing a given sequence file.
 * @param[in] iEntity The entity.
 * @param[in] iPath Normalised path of the `.owlseq`.
 * @return True when its player names this file.
 */
[[nodiscard]] auto isPlayerOf(const scene::Entity& iEntity, const std::filesystem::path& iPath) -> bool;

/**
 * @brief
 *  The first entity of a scene whose `SequencePlayer` plays a given sequence file.
 * @param[in] iScene The scene.
 * @param[in] iPath Normalised path of the `.owlseq`.
 * @return The player, or an invalid entity when none plays it.
 */
[[nodiscard]] auto findSequencePlayer(scene::Scene& iScene, const std::filesystem::path& iPath) -> scene::Entity;

/**
 * @brief
 *  The space of the keys of a player: the player, or its parent when it drives its own camera.
 * @param[in] iScene The scene of the player.
 * @param[in] iPlayer Entity with a `SequencePlayer`.
 * @return The world matrix of that space.
 */
[[nodiscard]] auto getSequenceAnchor(const scene::Scene& iScene, const scene::Entity& iPlayer) -> math::mat4;

}// namespace owl::nest
