/**
 * @file AssetImport.h
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owlgui.h>

#include "../UndoManager.h"
#include "../commands/CommandRegistry.h"

#include <data/assets/AssetMeta.h>

#include <filesystem>
#include <optional>
#include <string>

namespace owl::nest::panel {

/**
 * @brief
 *  Import settings of the asset selected in the content browser: its GUID, its importer, the texture settings and the
 *  state of its cooked form. Every change is an `asset.set_import` command.
 */
class AssetImport final {
public:
	/**
	 * @brief
	 *  Deleted copy constructor.
	 */
	AssetImport(const AssetImport&) = delete;

	/**
	 * @brief
	 *  Deleted move constructor.
	 */
	AssetImport(AssetImport&&) = delete;

	/**
	 * @brief
	 *  Deleted copy affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(const AssetImport&) -> AssetImport& = delete;

	/**
	 * @brief
	 *  Deleted move affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(AssetImport&&) -> AssetImport& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	AssetImport() = default;

	/**
	 * @brief
	 *  Default destructor.
	 */
	~AssetImport() = default;

	/**
	 * @brief
	 *  Show the panel on an asset and give it the focus.
	 * @param[in] iPath The asset file.
	 */
	void show(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Follow the selection of the content browser.
	 * @param[in] iPath The selected file, empty for none.
	 */
	void setSelection(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Set the command registry (non-owning).
	 * @param[in] iCommands The registry.
	 */
	void setCommandRegistry(const commands::CommandRegistry* iCommands) { mp_commands = iCommands; }

	/**
	 * @brief
	 *  Set the scene whose history records the changes.
	 * @param[in] iScene The scene.
	 */
	void setScene(const shared<scene::Scene>& iScene) { m_scene = iScene; }

	/**
	 * @brief
	 *  Set the undo manager of that scene (non-owning).
	 * @param[in] iUndoManager The undo manager.
	 */
	void setUndoManager(SceneUndoManager* iUndoManager) { mp_undoManager = iUndoManager; }

	/**
	 * @brief
	 *  Draw the panel.
	 */
	void onImGuiRender();

	/**
	 * @brief
	 *  Get the asset shown.
	 * @return The asset file, empty for none.
	 */
	[[nodiscard]] auto getPath() const -> const std::filesystem::path& { return m_path; }

	/**
	 * @brief
	 *  Run a command on the asset shown, through the scene history.
	 * @param[in] iName Command name.
	 * @param[in] iArgs Command arguments (`path` added).
	 * @return True when the command ran.
	 */
	[[nodiscard]] auto run(const std::string& iName, commands::CommandArgs iArgs) const -> bool;

private:
	/**
	 * @brief
	 *  Draw the texture settings.
	 * @param[in] iMeta The current sidecar.
	 */
	void renderTextureSettings(const data::assets::AssetMeta& iMeta) const;

	/**
	 * @brief
	 *  Draw the mesh settings: the levels of detail the cook builds.
	 * @param[in] iMeta The current sidecar.
	 */
	void renderMeshSettings(const data::assets::AssetMeta& iMeta);

	/// Command registry (non-owning).
	const commands::CommandRegistry* mp_commands = nullptr;
	/// Scene whose history records the changes.
	shared<scene::Scene> m_scene;
	/// Undo manager of that scene (non-owning).
	SceneUndoManager* mp_undoManager = nullptr;
	/// The asset shown.
	std::filesystem::path m_path;
	/// Whether the panel window is visible.
	bool m_visible = false;
	/// Whether the window takes the focus on the next frame.
	bool m_focus = false;
	/// Cache state of the asset shown, read again when the asset or its settings change.
	std::optional<bool> m_cached;
	/// Triangle ratio being dragged, sent as one command when the drag ends.
	float m_lodReduction = 0.5f;
	/// Screen size being dragged, sent as one command when the drag ends.
	float m_lodScreenSize = 0.25f;
	/// Sidecar the cache state was read for.
	std::optional<data::assets::AssetMeta> m_cachedFor;
};

}// namespace owl::nest::panel
