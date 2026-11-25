/**
 * @file ExtensionPanel.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../UndoManager.h"
#include "../commands/CommandRegistry.h"
#include "../session/PluginHost.h"

#include <data/assets/pack/ExtensionManifest.h>
#include <scene/Scene.h>
#include <script/ScriptEngine.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace owl::nest::panel {

/**
 * @brief
 *  The Extensions panel: the extension packs of the project (`<project>/extensions`), the manifest of the selected
 *  one and its export as a `.owlmod`.
 *
 * Every change is a command: `extension.create`, `extension.set` (undoable, in the history of the active scene) and
 * `extension.export`.
 */
class ExtensionPanel final {
public:
	/**
	 * @brief
	 *  Deleted copy constructor.
	 */
	ExtensionPanel(const ExtensionPanel&) = delete;

	/**
	 * @brief
	 *  Deleted move constructor.
	 */
	ExtensionPanel(ExtensionPanel&&) = delete;

	/**
	 * @brief
	 *  Deleted copy affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(const ExtensionPanel&) -> ExtensionPanel& = delete;

	/**
	 * @brief
	 *  Deleted move affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(ExtensionPanel&&) -> ExtensionPanel& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	ExtensionPanel() = default;

	/**
	 * @brief
	 *  Default destructor.
	 */
	~ExtensionPanel() = default;

	/**
	 * @brief
	 *  Show the panel when hidden, hide it when shown.
	 */
	void toggle() {
		m_visible = !m_visible;
		m_focus = m_visible;
		m_dirty = true;
	}

	/**
	 * @brief
	 *  Whether the panel is shown.
	 * @return True when visible.
	 */
	[[nodiscard]] auto isVisible() const -> bool { return m_visible; }

	/**
	 * @brief
	 *  Set the Owl Nest plugins the panel shows the state of (non-owning).
	 * @param[in] iPlugins The plugin host, null for none.
	 */
	void setPlugins(session::PluginHost* iPlugins) { mp_plugins = iPlugins; }

	/**
	 * @brief
	 *  Set the project whose packs are listed.
	 * @param[in] iProjectDirectory The project folder, empty without a project.
	 */
	void setProjectDirectory(const std::filesystem::path& iProjectDirectory);

	/**
	 * @brief
	 *  Set the scene whose undo history records the manifest edits.
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

	/// Builds the context the commands run on (the editor's: project, its extension list, its callbacks).
	using ContextProvider = std::function<commands::CommandContext()>;

	/**
	 * @brief
	 *  Set what builds the context of the commands; without one, a context of the scene, its undo manager and the
	 *  project folder.
	 * @param[in] iProvider The provider.
	 */
	void setContextProvider(ContextProvider iProvider) { m_contextProvider = std::move(iProvider); }

	/**
	 * @brief
	 *  Set the ids of the packs disabled in the project, shown unchecked.
	 * @param[in] iDisabled The ids (non-owning, null: every pack shown enabled).
	 */
	void setDisabledExtensions(const std::vector<std::string>* iDisabled) { mp_disabled = iDisabled; }

	/**
	 * @brief
	 *  Check whether a pack is enabled in the project.
	 * @param[in] iId Id of the pack.
	 * @return True when it is not disabled.
	 */
	[[nodiscard]] auto isEnabled(const std::string& iId) const -> bool;

	/**
	 * @brief
	 *  Read the packs of the project folder again (after an undo, a change on disk).
	 */
	void refresh();

	/**
	 * @brief
	 *  A pack of the project, as the panel lists it.
	 */
	struct Entry {
		/// Folder of the pack.
		std::filesystem::path folder;
		/// Its manifest.
		data::assets::pack::ExtensionManifest manifest;
		/// Functions of its entry script, with their line.
		std::vector<script::ScriptFunction> functions;
		/// Components its entry script declares.
		std::vector<script::ScriptComponentDeclaration> components;
		/// Last write of its entry script (the panel reads it again when it changes).
		std::filesystem::file_time_type scriptTime;
	};

	/**
	 * @brief
	 *  Get the packs listed.
	 * @return The packs, sorted by folder.
	 */
	[[nodiscard]] auto getEntries() const -> const std::vector<Entry>& { return m_entries; }

	/**
	 * @brief
	 *  Select a pack by id.
	 * @param[in] iId The id.
	 * @return True when a listed pack has this id.
	 */
	auto select(const std::string& iId) -> bool;

	/**
	 * @brief
	 *  Get the selected pack.
	 * @return The pack, or null when none is selected.
	 */
	[[nodiscard]] auto getSelected() const -> const Entry*;

	/**
	 * @brief
	 *  Run a command on the selected pack (its `path` is added) or on the project, then list the packs again.
	 * @param[in] iName Command name.
	 * @param[in] iArgs Command arguments.
	 * @return True when the command ran.
	 */
	auto run(const std::string& iName, commands::CommandArgs iArgs) -> bool;

	/**
	 * @brief
	 *  Draw the panel.
	 */
	void onImGuiRender();

private:
	/**
	 * @brief
	 *  Draw the list of the packs and the creation row.
	 */
	void renderList();

	/**
	 * @brief
	 *  Draw the manifest of the selected pack.
	 * @param[in] iEntry The selected pack.
	 */
	void renderManifest(const Entry& iEntry);

	/**
	 * @brief
	 *  Draw the dependencies of the selected pack, with their removal and the addition row.
	 * @param[in] iEntry The selected pack.
	 */
	void renderDependencies(const Entry& iEntry);

	/**
	 * @brief
	 *  Draw the entry script of the selected pack: its file, the game events it handles and its components.
	 * @param[in] iEntry The selected pack.
	 */
	void renderScript(const Entry& iEntry);

	/**
	 * @brief
	 *  Draw the Owl Nest plugin of a Script pack: its script, what it adds, the Lua function serving each and its
	 *  errors.
	 * @param[in] iEntry The pack.
	 */
	void renderPlugin(const Entry& iEntry);

	/**
	 * @brief
	 *  Draw the assets several mounted packs hold, and the pack serving each one.
	 */
	static void renderConflicts();

	/**
	 * @brief
	 *  Fill the text fields from the selected manifest.
	 */
	void loadDrafts();

	/**
	 * @brief
	 *  Build the context of a command: the provider's, else one of the scene and the project folder.
	 * @return The context.
	 */
	[[nodiscard]] auto makeContext() const -> commands::CommandContext;

	/// Builds the context of the commands, empty for the default one.
	ContextProvider m_contextProvider;
	/// Ids of the packs disabled in the project (non-owning, may be null).
	const std::vector<std::string>* mp_disabled{nullptr};
	/// Project folder.
	std::filesystem::path m_projectDirectory;
	/// Scene whose history records the edits.
	shared<scene::Scene> m_scene;
	/// Undo manager of the scene (non-owning).
	SceneUndoManager* mp_undoManager{nullptr};
	/// Command registry (non-owning).
	const commands::CommandRegistry* mp_commands{nullptr};
	/// Packs of the project.
	std::vector<Entry> m_entries;
	/// Index of the selected pack.
	std::optional<size_t> m_selected;
	/// Id typed for a new pack.
	std::string m_newId;
	/// Name field being edited.
	std::string m_name;
	/// Version field being edited.
	std::string m_version;
	/// Author field being edited.
	std::string m_author;
	/// Description field being edited.
	std::string m_description;
	/// Draft of the entry script path.
	std::string m_script;
	/// Draft of the Owl Nest plugin script path.
	std::string m_editorScript;
	/// Owl Nest plugins loaded (non-owning, may be null).
	session::PluginHost* mp_plugins{nullptr};
	/// Id of the dependency to add.
	std::string m_dependencyId;
	/// Version of the dependency to add.
	std::string m_dependencyVersion;
	/// Write time of the selected manifest when it was read: another time means an undo or an edit on disk.
	std::filesystem::file_time_type m_manifestTime;
	/// Result of the last command, shown under the list.
	std::string m_status;
	/// Whether the packs must be listed again before the next draw.
	bool m_dirty{true};
	/// Whether the panel is shown.
	bool m_visible{false};
	/// Whether the panel takes the focus on its next frame.
	bool m_focus{false};
};

}// namespace owl::nest::panel
