/**
 * @file ScriptDebugPanel.h
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <scene/Entity.h>
#include <script/ScriptDebugger.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace owl::scene {
class Scene;
}// namespace owl::scene

namespace owl::script {
class ScriptInstance;
}// namespace owl::script

namespace owl::nest::panel {

/**
 * @brief
 *  A running instance of a script: the one of the selected entity when it runs the script, else the scene script,
 *  else the first entity running it.
 * @param[in] iScene The scene in Play.
 * @param[in] iSelected The selected entity.
 * @param[in] iScriptPath Path of the script.
 * @return The instance, or null when no entity runs the script.
 */
[[nodiscard]] auto findScriptInstance(scene::Scene& iScene, const scene::Entity& iSelected,
									  const std::filesystem::path& iScriptPath) -> const script::ScriptInstance*;

/**
 * @brief
 *  The Script Debug panel: the breakpoint the game stopped on, with its locals, and the variables of the running
 *  scripts.
 */
class ScriptDebugPanel final {
public:
	/**
	 * @brief
	 *  What the user asked from the panel this frame.
	 */
	enum struct Request : uint8_t {
		None,///< Nothing.
		Continue,///< Resume the game and forget the hit.
		OpenHit///< Open the script of the hit at its line.
	};

	/**
	 * @brief
	 *  What the panel shows: the running scene and the selected entity.
	 */
	struct Context {
		/// Scene in Play, or null in Edit.
		scene::Scene* scene{nullptr};
		/// Selected entity, shown first.
		scene::Entity selected;
		/// Whether the game is paused.
		bool paused{false};
	};

	ScriptDebugPanel() = default;

	~ScriptDebugPanel() = default;

	ScriptDebugPanel(const ScriptDebugPanel&) = delete;

	ScriptDebugPanel(ScriptDebugPanel&&) = delete;

	auto operator=(const ScriptDebugPanel&) -> ScriptDebugPanel& = delete;

	auto operator=(ScriptDebugPanel&&) -> ScriptDebugPanel& = delete;

	/**
	 * @brief
	 *  Show a breakpoint hit and bring the panel to the front.
	 * @param[in] iHit The hit.
	 */
	void setHit(script::BreakpointHit iHit);

	/**
	 * @brief
	 *  Forget the hit shown.
	 */
	void clearHit() { m_hit.reset(); }

	/**
	 * @brief
	 *  The hit shown.
	 * @return The hit, or nothing.
	 */
	[[nodiscard]] auto getHit() const -> const std::optional<script::BreakpointHit>& { return m_hit; }

	/**
	 * @brief
	 *  Draw the panel.
	 * @param[in] iContext Scene and selection.
	 * @return The request of the user.
	 */
	[[nodiscard]] auto onImGuiRender(const Context& iContext) -> Request;

private:
	/// Last hit, kept until Continue or Stop.
	std::optional<script::BreakpointHit> m_hit;
	/// Focus the window on the next frame.
	bool m_focus{false};
	/// Text typed in the filter of the variables.
	std::string m_filter;

	/**
	 * @brief
	 *  Draw the hit section.
	 * @param[in] iPaused Whether the game is paused.
	 * @return The request of the user.
	 */
	auto renderHit(bool iPaused) -> Request;

	/**
	 * @brief
	 *  Draw the variables of the scene script and the entity scripts.
	 * @param[in] iContext Scene and selection.
	 */
	void renderWatch(const Context& iContext);
};

}// namespace owl::nest::panel
