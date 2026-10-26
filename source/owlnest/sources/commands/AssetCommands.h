/**
 * @file AssetCommands.h
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../UndoCommand.h"

#include <data/assets/AssetMeta.h>

#include <string>
#include <vector>

namespace owl::nest::commands {

class CommandRegistry;

/**
 * @brief
 *  Change of the import settings of assets: their sidecars before and after, written back on undo and redo.
 *
 * The scene the command is recorded in is not touched: the command only rides its undo history.
 */
class SetImportSettingsCommand final : public SceneUndoCommand {
public:
	/**
	 * @brief
	 *  Deleted copy constructor.
	 */
	SetImportSettingsCommand(const SetImportSettingsCommand&) = delete;

	/**
	 * @brief
	 *  Default move constructor.
	 */
	SetImportSettingsCommand(SetImportSettingsCommand&&) = default;

	/**
	 * @brief
	 *  Deleted copy affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(const SetImportSettingsCommand&) -> SetImportSettingsCommand& = delete;

	/**
	 * @brief
	 *  Default move affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(SetImportSettingsCommand&&) -> SetImportSettingsCommand& = default;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iBefore Sidecars before the change.
	 * @param[in] iAfter Sidecars after the change, in the same order.
	 */
	SetImportSettingsCommand(std::vector<data::assets::AssetMeta> iBefore, std::vector<data::assets::AssetMeta> iAfter);

	/**
	 * @brief
	 *  Destructor.
	 */
	~SetImportSettingsCommand() override;

	/**
	 * @brief
	 *  Write the sidecars back as they were.
	 * @param[in,out] ioScene The scene of the history (unchanged).
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Write the changed sidecars.
	 * @param[in,out] ioScene The scene of the history (unchanged).
	 */
	void redo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override;

private:
	/**
	 * @brief
	 *  Write sidecars and refilter the loaded textures they describe.
	 * @param[in] iMetas The sidecars.
	 */
	static void apply(const std::vector<data::assets::AssetMeta>& iMetas);

	/// Sidecars before the change.
	std::vector<data::assets::AssetMeta> m_before;
	/// Sidecars after the change.
	std::vector<data::assets::AssetMeta> m_after;
};

/**
 * @brief
 *  Register the asset commands: import settings, rename, move, delete, cook, cache and reference upgrade.
 * @param[in,out] ioRegistry The registry.
 */
void addAssetCommands(CommandRegistry& ioRegistry);

}// namespace owl::nest::commands
