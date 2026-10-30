/**
 * @file MixerPanel.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../UndoManager.h"
#include "../commands/CommandRegistry.h"

#include <scene/Scene.h>
#include <sound/Bus.h>

#include <optional>

namespace owl::nest::panel {

/**
 * @brief
 *  The Mixer panel: one strip per bus (volume fader, mute, solo, live gain) and the Doppler factor.
 *
 * In Edit mode it edits the mix of the scene through `scene.set_mixer` (undoable, saved with the scene); while the
 * scene plays it changes the live mixer through `sound.set_live_bus`, which the end of Play forgets.
 */
class MixerPanel final {
public:
	MixerPanel(const MixerPanel&) = delete;

	MixerPanel(MixerPanel&&) = delete;

	auto operator=(const MixerPanel&) -> MixerPanel& = delete;

	auto operator=(MixerPanel&&) -> MixerPanel& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	MixerPanel() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~MixerPanel() = default;

	/**
	 * @brief
	 *  Show the panel.
	 */
	void open() { m_visible = true; }

	/**
	 * @brief
	 *  Whether the panel is shown.
	 * @return True when visible.
	 */
	[[nodiscard]] auto isVisible() const -> bool { return m_visible; }

	/**
	 * @brief
	 *  Set the scene whose mix is edited.
	 * @param[in] iScene The scene (may be null).
	 */
	void setScene(const shared<scene::Scene>& iScene) { m_scene = iScene; }

	/**
	 * @brief
	 *  Set the undo manager of the scene.
	 * @param[in] iUndoManager The undo manager (non-owning, may be null).
	 */
	void setUndoManager(SceneUndoManager* iUndoManager) { mp_undoManager = iUndoManager; }

	/**
	 * @brief
	 *  Set the command registry running the edits.
	 * @param[in] iCommands The registry (non-owning).
	 */
	void setCommandRegistry(const commands::CommandRegistry* iCommands) { mp_commands = iCommands; }

	/**
	 * @brief
	 *  Draw the panel.
	 */
	void onImGuiRender();

private:
	/**
	 * @brief
	 *  Draw the strip of a bus.
	 * @param[in] iBus The bus.
	 * @param[in] iPlaying Whether the scene plays (the live mixer is edited).
	 */
	void drawStrip(sound::Bus iBus, bool iPlaying);

	/**
	 * @brief
	 *  Run the command editing a bus.
	 * @param[in] iBus The bus.
	 * @param[in] iPlaying Whether the scene plays.
	 * @param[in] iArgs The edited fields (`volume`, `muted`, `solo`).
	 */
	void runBusCommand(sound::Bus iBus, bool iPlaying, commands::CommandArgs iArgs) const;

	/// Scene edited.
	shared<scene::Scene> m_scene;
	/// Undo manager of the scene (non-owning).
	SceneUndoManager* mp_undoManager{nullptr};
	/// Command registry (non-owning).
	const commands::CommandRegistry* mp_commands{nullptr};
	/// Volume of the fader being dragged, committed when released.
	std::optional<float> m_dragVolume;
	/// Whether the panel is shown.
	bool m_visible{false};
};

}// namespace owl::nest::panel
