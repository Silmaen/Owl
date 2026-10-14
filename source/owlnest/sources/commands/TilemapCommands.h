/**
 * @file TilemapCommands.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../UndoCommand.h"

#include <math/vectors.h>
#include <scene/TilemapAsset.h>
#include <scene/component/Tilemap.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace owl::nest::commands {

/// Undo command acting on a tilemap asset.
using TilemapUndoCommand = UndoCommand<scene::TilemapAsset>;

/**
 * @brief
 *  One chunk recorded by a tilemap edit: where it lives and the content the command holds for it.
 */
struct TilemapChunkRecord {
	/// Index of the layer in the asset.
	uint32_t layer = 0;
	/// Chunk coordinates in the layer.
	math::vec2ui chunk{0, 0};
	/// Content kept by the command (the one to put back on the next undo or redo).
	scene::component::TilemapChunkRef content;
};

/**
 * @brief
 *  Undoable cell edit (paint stroke, cell property) recorded as the chunks it changed.
 *
 * The command holds one version of each changed chunk and swaps it with the live one on undo and redo, so an edit
 * of one cell costs one chunk whatever the size of the map.
 */
class ModifyTilemapChunksCommand final : public TilemapUndoCommand {
public:
	/**
	 * @brief
	 *  Constructor, from the content of the chunks before the edit (the edit is already applied).
	 * @param[in] iBefore The changed chunks, with their content before the edit.
	 * @param[in] iDescription Human-readable description for menus.
	 */
	ModifyTilemapChunksCommand(std::vector<TilemapChunkRecord> iBefore, std::string iDescription);

	/**
	 * @brief
	 *  Put back the content before the edit.
	 * @param[in,out] ioTarget The tilemap asset.
	 */
	void undo(scene::TilemapAsset& ioTarget) override;

	/**
	 * @brief
	 *  Put back the content after the edit.
	 * @param[in,out] ioTarget The tilemap asset.
	 */
	void redo(scene::TilemapAsset& ioTarget) override;

	/**
	 * @brief
	 *  Human-readable description for the Edit menu / tooltips.
	 * @return The description string.
	 */
	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	/**
	 * @brief
	 *  The chunks the command records.
	 * @return The records.
	 */
	[[nodiscard]] auto getRecords() const -> const std::vector<TilemapChunkRecord>& { return m_records; }

	/**
	 * @brief
	 *  Cell memory held by the command.
	 * @return The bytes of the non-empty chunks it records.
	 */
	[[nodiscard]] auto getStoredBytes() const -> size_t;

private:
	/**
	 * @brief
	 *  Exchange the recorded chunks with the live ones.
	 * @param[in,out] ioTarget The tilemap asset.
	 */
	void swapChunks(scene::TilemapAsset& ioTarget);

	/// The recorded chunks.
	std::vector<TilemapChunkRecord> m_records;
	/// Description shown in menus.
	std::string m_description;
};

/**
 * @brief
 *  Undoable change of the asset layout: tileset, grid size, cell size, layer list or layer properties.
 *
 * Keeps the asset before and after the edit. Layer copies share their chunks, so the command costs the chunk
 * tables plus the chunks the edit dropped (a shrink, a deleted layer), never a copy of the cells.
 */
class ModifyTilemapLayoutCommand final : public TilemapUndoCommand {
public:
	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iBefore The asset before the edit.
	 * @param[in] iAfter The asset after the edit.
	 * @param[in] iDescription Human-readable description for menus.
	 */
	ModifyTilemapLayoutCommand(scene::TilemapAsset iBefore, scene::TilemapAsset iAfter, std::string iDescription);

	/**
	 * @brief
	 *  Put back the asset before the edit.
	 * @param[in,out] ioTarget The tilemap asset.
	 */
	void undo(scene::TilemapAsset& ioTarget) override;

	/**
	 * @brief
	 *  Put back the asset after the edit.
	 * @param[in,out] ioTarget The tilemap asset.
	 */
	void redo(scene::TilemapAsset& ioTarget) override;

	/**
	 * @brief
	 *  Human-readable description for the Edit menu / tooltips.
	 * @return The description string.
	 */
	[[nodiscard]] auto description() const -> std::string override { return m_description; }

private:
	/// The asset before the edit.
	scene::TilemapAsset m_before;
	/// The asset after the edit.
	scene::TilemapAsset m_after;
	/// Description shown in menus.
	std::string m_description;
};

/**
 * @brief
 *  Undoable terraforming: the corners a raise, lower or level operation moved.
 *
 * The command is pushed once the operation is applied; only the changed corners are kept.
 */
class ModifyTerrainHeightsCommand final : public TilemapUndoCommand {
public:
	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iChanges The corners that changed, with their heights before and after.
	 * @param[in] iDescription Human-readable description for menus.
	 */
	ModifyTerrainHeightsCommand(std::vector<scene::CornerHeightChange> iChanges, std::string iDescription);

	/**
	 * @brief
	 *  Put the corners back to their previous heights.
	 * @param[in,out] ioTarget The tilemap asset.
	 */
	void undo(scene::TilemapAsset& ioTarget) override;

	/**
	 * @brief
	 *  Move the corners to their new heights again.
	 * @param[in,out] ioTarget The tilemap asset.
	 */
	void redo(scene::TilemapAsset& ioTarget) override;

	/**
	 * @brief
	 *  Human-readable description for the Edit menu / tooltips.
	 * @return The description string.
	 */
	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	/**
	 * @brief
	 *  The corners the command moves.
	 * @return The changes.
	 */
	[[nodiscard]] auto getChanges() const -> const std::vector<scene::CornerHeightChange>& { return m_changes; }

private:
	/// The corners that changed.
	std::vector<scene::CornerHeightChange> m_changes;
	/// Description shown in menus.
	std::string m_description;
};

/**
 * @brief
 *  Gathers the terraforming operations of one gesture (a click or a drag) into a single undo step.
 */
class TerrainStroke final {
public:
	/**
	 * @brief
	 *  Add the corners one operation moved.
	 * @param[in] iChanges The result of a terraforming call of the asset.
	 */
	void add(const std::vector<scene::CornerHeightChange>& iChanges);

	/**
	 * @brief
	 *  Whether nothing has been recorded.
	 * @return True when the stroke holds no change.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return m_changes.empty(); }

	/**
	 * @brief
	 *  Close the stroke into one command.
	 * @param[in] iDescription Description for the Edit menu.
	 * @return The command, or null when the gesture changed nothing.
	 */
	[[nodiscard]] auto finish(const std::string& iDescription) -> uniq<ModifyTerrainHeightsCommand>;

private:
	/// Changes of the operations so far.
	std::vector<scene::CornerHeightChange> m_changes;
};

/**
 * @brief
 *  Cell edits gathered into one `ModifyTilemapChunksCommand` (a paint stroke).
 *
 * Every write goes through the stroke, which records the content of a chunk the first time one of its cells
 * changes; `finish` builds the command from the chunks that really differ at the end.
 */
class TilemapStroke final {
public:
	/**
	 * @brief
	 *  Write the terrain tile of one cell.
	 * @param[in,out] ioAsset The tilemap asset.
	 * @param[in] iLayer Layer index.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @param[in] iValue Tile index, or `g_EmptyTileIndex`.
	 * @return True when the cell changed.
	 */
	auto setTile(scene::TilemapAsset& ioAsset, uint32_t iLayer, uint32_t iX, uint32_t iY, int32_t iValue) -> bool;

	/**
	 * @brief
	 *  Write one whole cell.
	 * @param[in,out] ioAsset The tilemap asset.
	 * @param[in] iLayer Layer index.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @param[in] iCell The new cell.
	 * @return True when the cell changed.
	 */
	auto setCell(scene::TilemapAsset& ioAsset, uint32_t iLayer, uint32_t iX, uint32_t iY,
				 const scene::component::TileCell& iCell) -> bool;

	/**
	 * @brief
	 *  Whether no chunk was recorded yet.
	 * @return True before the first change.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return m_before.empty(); }

	/**
	 * @brief
	 *  End the stroke.
	 * @param[in] iAsset The tilemap asset after the edits.
	 * @param[in] iDescription Description of the command.
	 * @return The command, or nullptr when no chunk ends up different.
	 */
	[[nodiscard]] auto finish(const scene::TilemapAsset& iAsset, const std::string& iDescription)
			-> uniq<ModifyTilemapChunksCommand>;

private:
	/**
	 * @brief
	 *  Record the chunk holding a cell before its first change in the stroke.
	 * @param[in] iAsset The tilemap asset.
	 * @param[in] iLayer Layer index.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 */
	void record(const scene::TilemapAsset& iAsset, uint32_t iLayer, uint32_t iX, uint32_t iY);

	/// Chunks recorded before their first change.
	std::vector<TilemapChunkRecord> m_before;
};

}// namespace owl::nest::commands
