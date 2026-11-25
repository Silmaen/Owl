/**
 * @file PluginHost.h
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../commands/CommandRegistry.h"
#include "PluginUi.h"

#include <core/Core.h>
#include <core/expected.h>
#include <data/assets/pack/ExtensionRegistry.h>
#include <scene/Entity.h>
#include <script/ScriptConsole.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest::session {

/**
 * @brief
 *  What an Owl Nest plugin can add to the editor.
 */
enum struct PluginContributionKind : uint8_t {
	Command,///< A command of the `CommandRegistry` (`nest.command`).
	Panel,///< A panel (`nest.panel`).
	Drawer,///< The inspector body of a component (`nest.drawer`).
	DocumentType,///< A document type: the tab a file extension opens in (`nest.document`).
	Importer///< An importer: a source file turned into an asset (`nest.importer`).
};

/**
 * @brief
 *  Name of a contribution kind, as shown in the Extensions panel and by `plugin.list`.
 * @param[in] iKind The kind.
 * @return The lower-case name (`command`, `panel`, `drawer`, `document`, `importer`).
 */
[[nodiscard]] auto getContributionKindName(PluginContributionKind iKind) -> std::string_view;

/**
 * @brief
 *  One thing a plugin adds to the editor, and the Lua function of its entry script that serves it.
 */
struct PluginContribution {
	/// What it is.
	PluginContributionKind kind{PluginContributionKind::Command};
	/// Its name: the command name, the panel, document type or importer id, the key of the component drawn.
	std::string id;
	/// Title of a panel or of a document type, description of a command or of an importer.
	std::string title;
	/// File extension, with its dot, of a document type or of an importer.
	std::string extension;
	/// Name of the global Lua function called.
	std::string handler;
	/// Line of that function in the entry script, 0 when the script does not define it.
	uint32_t line{0};
	/// Whether the panel is shown.
	bool visible{false};
};

/**
 * @brief
 *  An extension pack loaded as an Owl Nest plugin.
 */
struct PluginInfo {
	/// Id of the pack.
	std::string id;
	/// Display name of the pack.
	std::string name;
	/// Entry script of the plugin, an asset path in the pack (`editorScript` of the manifest).
	std::string script;
	/// The pack: its folder or its `.owlmod` file.
	std::filesystem::path source;
	/// What the plugin adds; empty when its script failed to load.
	std::vector<PluginContribution> contributions;
	/// Errors of the plugin, the load error first, then the ones of its handlers (each message once).
	std::vector<std::string> errors;
};

/**
 * @brief
 *  The asset an importer produced.
 */
struct PluginImport {
	/// File to write, as the handler gave it (relative to the folder of the source, or absolute).
	std::string path;
	/// Its content.
	std::string text;
};

/**
 * @brief
 *  Loads the Owl Nest plugins: the mounted extension packs whose manifest names an `editorScript`.
 *
 * Each plugin runs its entry script in a Lua state of its own (the sandbox and quotas of every script, the `editor`
 * module of the console). The script declares what it adds with the `nest` module; the host fills the editor
 * registries with it at run time:
 * - commands go into the `CommandRegistry`, so the console, the journal, macros, MCP and editor scripts see them; a
 *   plugin command is one undo step, undone when its handler fails;
 * - panels, inspector drawers and document types are drawn by a handler through a `PluginUi`, never through ImGui;
 * - importers serve the `asset.import` command.
 *
 * A plugin changes a scene or an asset only by running registry commands (`editor.*`), hence every edit is undoable.
 */
class PluginHost final {
public:
	/// Gives the context the commands of a panel, drawer or document handler run on.
	using ContextProvider = std::function<commands::CommandContext()>;
	/// Gives the entity selected in the editor (0 for none), passed to the panel handlers.
	using SelectionProvider = std::function<core::UUID()>;

	/// Most errors kept per plugin.
	static constexpr size_t maxErrors = 16;

	/**
	 * @brief
	 *  Create the host and register its commands (`plugin.list`, `plugin.reload`, `plugin.show_panel`,
	 *  `plugin.document_set`, `asset.import`).
	 * @param[in,out] ioRegistry The registry the plugin commands are added to (outlives the host).
	 */
	explicit PluginHost(commands::CommandRegistry& ioRegistry);

	/**
	 * @brief
	 *  Destructor: the commands of the plugins and of the host leave the registry.
	 */
	~PluginHost();

	PluginHost(const PluginHost&) = delete;

	PluginHost(PluginHost&&) = delete;

	auto operator=(const PluginHost&) -> PluginHost& = delete;

	auto operator=(PluginHost&&) -> PluginHost& = delete;

	/**
	 * @brief
	 *  Set where the handlers drawing a panel, a component or a document find the scene their commands edit.
	 * @param[in] iProvider The provider (none: such commands fail for want of a scene).
	 */
	void setContextProvider(ContextProvider iProvider) { m_contextProvider = std::move(iProvider); }

	/**
	 * @brief
	 *  Set where the panel handlers find the selected entity.
	 * @param[in] iProvider The provider.
	 */
	void setSelectionProvider(SelectionProvider iProvider) { m_selectionProvider = std::move(iProvider); }

	/**
	 * @brief
	 *  Unload every plugin, then load the plugin of each mounted pack, in load order.
	 *
	 * Called from a plugin handler (a plugin enabling a pack), the reload waits for the handler to return.
	 * @param[in] iExtensions The mounted packs (outlives the host, or the next `load`).
	 * @return Number of plugins loaded without error.
	 */
	auto load(const data::assets::pack::ExtensionRegistry& iExtensions) -> size_t;

	/**
	 * @brief
	 *  Unload every plugin: their commands leave the registry.
	 */
	void unload();

	/**
	 * @brief
	 *  The plugins, in load order, the ones that failed to load included.
	 * @return The plugins.
	 */
	[[nodiscard]] auto getPlugins() const -> const std::vector<PluginInfo>& { return m_plugins; }

	/**
	 * @brief
	 *  Find a plugin.
	 * @param[in] iId Id of its pack.
	 * @return The plugin, null when the pack is no loaded plugin.
	 */
	[[nodiscard]] auto find(std::string_view iId) const -> const PluginInfo*;

	/**
	 * @brief
	 *  Find something the plugins add.
	 * @param[in] iKind Its kind.
	 * @param[in] iId Its name (see PluginContribution::id).
	 * @return The contribution, null when no plugin adds it.
	 */
	[[nodiscard]] auto findContribution(PluginContributionKind iKind, std::string_view iId) const
			-> const PluginContribution*;

	/**
	 * @brief
	 *  Everything of a kind the plugins add, in load order.
	 * @param[in] iKind The kind.
	 * @return The contributions (valid until the next `load` or `unload`).
	 */
	[[nodiscard]] auto getContributions(PluginContributionKind iKind) const -> std::vector<const PluginContribution*>;

	/**
	 * @brief
	 *  Show or hide a plugin panel.
	 * @param[in] iId Id of the panel.
	 * @param[in] iVisible Whether it is shown.
	 * @return False when no plugin has this panel.
	 */
	auto setPanelVisible(std::string_view iId, bool iVisible) -> bool;

	/**
	 * @brief
	 *  Draw a plugin panel: its handler receives `playing`, `entity_count` and, with a selection, `selection` (its id)
	 *  and `selection_name`.
	 * @param[in] iId Id of the panel.
	 * @param[in,out] ioUi Where the widgets go.
	 * @return False when no plugin has this panel or when its handler failed.
	 */
	auto drawPanel(std::string_view iId, PluginUi& ioUi) -> bool;

	/**
	 * @brief
	 *  Draw the inspector body of a component a plugin has a drawer for: its handler receives `entity` (its id), `name`,
	 *  `component` and the fields of the component by name, a built-in one or a Lua one.
	 * @param[in] iEntity The entity inspected.
	 * @param[in] iKey Key of the component.
	 * @param[in,out] ioUi Where the widgets go.
	 * @return False when no plugin draws this component (the editor then draws it itself).
	 */
	auto drawComponent(const scene::Entity& iEntity, const std::string& iKey, PluginUi& ioUi) -> bool;

	/**
	 * @brief
	 *  The document type a file opens in.
	 * @param[in] iPath The file.
	 * @return The document type whose extension the file has, null when no plugin declares one.
	 */
	[[nodiscard]] auto findDocumentType(const std::filesystem::path& iPath) const -> const PluginContribution*;

	/**
	 * @brief
	 *  Draw a document: its handler receives `path`, `name` and `text`, and edits the text with
	 *  `editor.plugin.document_set`.
	 * @param[in] iType Id of the document type.
	 * @param[in] iPath File of the document.
	 * @param[in] iText Its text as it is.
	 * @param[in,out] ioUi Where the widgets go.
	 * @return False when no plugin has this document type or when its handler failed.
	 */
	auto drawDocument(std::string_view iType, const std::filesystem::path& iPath, const std::string& iText,
					  PluginUi& ioUi) -> bool;

	/**
	 * @brief
	 *  The importer of a file.
	 * @param[in] iPath The source file.
	 * @param[in] iId Id of the importer wanted, empty for the first one that takes the extension of the file.
	 * @return The importer, null when none fits.
	 */
	[[nodiscard]] auto findImporter(const std::filesystem::path& iPath, std::string_view iId = {}) const
			-> const PluginContribution*;

	/**
	 * @brief
	 *  Run an importer: its handler receives `path`, `name` (the file name without extension) and `text`, and returns
	 *  the `path` and the `text` of the asset to write.
	 * @param[in] iId Id of the importer.
	 * @param[in] iSource The source file.
	 * @param[in] iText Its content.
	 * @return The asset to write, or why the importer gave none.
	 */
	[[nodiscard]] auto runImporter(std::string_view iId, const std::filesystem::path& iSource, const std::string& iText)
			-> expected<PluginImport, std::string>;

private:
	/// A loaded plugin: its Lua state and what its script declared.
	struct State;

	/**
	 * @brief
	 *  Load the plugin of a pack.
	 * @param[in] iPack The pack.
	 * @return True when its script ran and every declaration was accepted.
	 */
	auto loadPlugin(const data::assets::pack::ExtensionPack& iPack) -> bool;

	/**
	 * @brief
	 *  Give the Lua state of a plugin the `nest` and `ui` modules.
	 * @param[in] iIndex Index of the plugin.
	 */
	void bindModules(size_t iIndex);

	/**
	 * @brief
	 *  Record a declaration of a plugin script.
	 * @param[in] iIndex Index of the plugin.
	 * @param[in] iKind What is declared.
	 * @param[in] iArgs Fields of the Lua table.
	 * @return Nothing, or the refusal raised in the script.
	 */
	auto declare(size_t iIndex, PluginContributionKind iKind, const std::map<std::string, script::HostValue>& iArgs)
			-> script::HostResult;

	/**
	 * @brief
	 *  Record an argument of a command a plugin script declared.
	 * @param[in] iIndex Index of the plugin.
	 * @param[in] iArgs Fields of the Lua table.
	 * @return Nothing, or the refusal raised in the script.
	 */
	auto declareArgument(size_t iIndex, const std::map<std::string, script::HostValue>& iArgs) -> script::HostResult;

	/**
	 * @brief
	 *  Draw a widget asked by a plugin script.
	 * @param[in] iKind The widget.
	 * @param[in] iArgs Fields of the Lua table.
	 * @return The fields given back to the script, or the refusal.
	 */
	auto drawWidget(PluginWidgetKind iKind, const std::map<std::string, script::HostValue>& iArgs)
			-> script::HostResult;

	/**
	 * @brief
	 *  Call a handler of a plugin; a failure is recorded in the errors of the plugin.
	 * @param[in] iIndex Index of the plugin.
	 * @param[in] iHandler Name of the Lua function.
	 * @param[in] iArgs Fields of the table it receives.
	 * @param[in] iContext Context its commands run on, null for the one of the context provider.
	 * @param[in,out] ioUi Where its widgets go, null when it draws nothing.
	 * @return What the function returned.
	 */
	auto invoke(size_t iIndex, const std::string& iHandler, const std::map<std::string, script::HostValue>& iArgs,
				const commands::CommandContext* iContext, PluginUi* ioUi) -> script::ScriptConsole::CallResult;

	/**
	 * @brief
	 *  List the entities of the scene of the running handler (`nest.entities`), read only.
	 * @param[in] iArgs Fields of the Lua table: `component`, `max`.
	 * @return Their count, ids and names, and the selection; or the refusal.
	 */
	[[nodiscard]] auto listEntities(const std::map<std::string, script::HostValue>& iArgs) const -> script::HostResult;

	/**
	 * @brief
	 *  Read the fields of a component of an entity (`nest.fields`, `nest.field`), read only.
	 * @param[in] iArgs Fields of the Lua table: `entity`, `component` and, for one field, `field`.
	 * @param[in] iOneField True to give the value of the field named, false to list the field names.
	 * @return What was found.
	 */
	[[nodiscard]] auto readFields(const std::map<std::string, script::HostValue>& iArgs, bool iOneField) const
			-> script::HostResult;

	/**
	 * @brief
	 *  Run a plugin command: its handler, as one undo step.
	 * @param[in] iPlugin Id of the plugin.
	 * @param[in] iCommand Name of the command.
	 * @param[in] iContext The context of the command.
	 * @param[in] iArgs Its arguments.
	 * @return What the handler did, or why it failed (what it did is then undone).
	 */
	auto runCommand(const std::string& iPlugin, const std::string& iCommand, const commands::CommandContext& iContext,
					const commands::CommandArgs& iArgs) -> expected<commands::CommandResult, commands::CommandFailure>;

	/**
	 * @brief
	 *  Run a registry command asked by a plugin script, on the context of the handler running.
	 * @param[in] iName Command name.
	 * @param[in] iArgs Command arguments.
	 * @return What the command did, or why it did not run.
	 */
	[[nodiscard]] auto executeFromScript(std::string_view iName, const commands::CommandArgs& iArgs) const
			-> expected<commands::CommandResult, commands::CommandFailure>;

	/**
	 * @brief
	 *  The context the commands of the running handler act on.
	 * @return The context of the plugin command running, else the one of the provider.
	 */
	[[nodiscard]] auto currentContext() const -> commands::CommandContext;

	/**
	 * @brief
	 *  Find a contribution and the plugin holding it.
	 * @param[in] iKind Its kind.
	 * @param[in] iId Its name.
	 * @return The plugin index and the contribution, null when no plugin adds it.
	 */
	[[nodiscard]] auto locate(PluginContributionKind iKind, std::string_view iId)
			-> std::pair<size_t, PluginContribution*>;

	/**
	 * @brief
	 *  Record an error of a plugin, once per message.
	 * @param[in] iIndex Index of the plugin.
	 * @param[in] iMessage The error.
	 */
	void report(size_t iIndex, const std::string& iMessage);

	/**
	 * @brief
	 *  Register the commands of the host itself.
	 */
	void addHostCommands();

	/// The registry the commands go into.
	commands::CommandRegistry& m_registry;
	/// The plugins, in load order.
	std::vector<PluginInfo> m_plugins;
	/// Their Lua states, by the same index.
	std::vector<uniq<State>> m_states;
	/// The packs loaded last, read again by `plugin.reload` (non-owning).
	const data::assets::pack::ExtensionRegistry* mp_extensions{nullptr};
	/// Context of the handlers that draw.
	ContextProvider m_contextProvider;
	/// Selected entity, for the panel handlers.
	SelectionProvider m_selectionProvider;
	/// Context of the plugin command running (non-owning, null outside one).
	const commands::CommandContext* mp_context{nullptr};
	/// Where the running handler draws (non-owning, null when it draws nothing).
	PluginUi* mp_ui{nullptr};
	/// Whether the section being drawn is open: the widgets of a closed section are skipped.
	bool m_sectionOpen{true};
	/// Handlers running, one inside the other.
	uint32_t m_depth{0};
	/// A `load` asked while a handler runs, done when it returns.
	bool m_reloadPending{false};
};

}// namespace owl::nest::session
