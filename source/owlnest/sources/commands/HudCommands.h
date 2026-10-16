/**
 * @file HudCommands.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../EntitySnapshot.h"
#include "../UndoCommand.h"

#include <math/vectors.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/UiRect.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::commands {

class CommandRegistry;

/**
 * @brief
 *  The change of the `UiRect` of one widget.
 */
struct HudRectChange {
	/// The widget.
	core::UUID entity{0};
	/// Rectangle before the change.
	scene::component::UiRect before;
	/// Rectangle after the change.
	scene::component::UiRect after;
};

/**
 * @brief
 *  Undoable change of the `UiRect` of one or several widgets (move, resize, anchors, pivot, group move), as one step.
 *
 * A change of the same widgets right after merges into it, so a drag or a typed value is one entry of the history.
 */
class HudRectsCommand final : public SceneUndoCommand {
public:
	HudRectsCommand(const HudRectsCommand&) = delete;

	HudRectsCommand(HudRectsCommand&&) = default;

	auto operator=(const HudRectsCommand&) -> HudRectsCommand& = delete;

	auto operator=(HudRectsCommand&&) -> HudRectsCommand& = default;

	/**
	 * @brief
	 *  Build the command from the changes.
	 * @param[in] iChanges One change per widget.
	 * @param[in] iDescription Text of the history entry.
	 */
	HudRectsCommand(std::vector<HudRectChange> iChanges, std::string iDescription);

	/**
	 * @brief
	 *  Destructor.
	 */
	~HudRectsCommand() override;

	/**
	 * @brief
	 *  Put the rectangles back.
	 * @param[in,out] ioScene The scene holding the widgets.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Apply the new rectangles.
	 * @param[in,out] ioScene The scene holding the widgets.
	 */
	void redo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Description of the history entry.
	 * @return The description.
	 */
	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	/**
	 * @brief
	 *  Absorb a later change of the same widgets.
	 * @param[in] iOther The newer command.
	 * @return True when it was merged.
	 */
	[[nodiscard]] auto mergeWith(const SceneUndoCommand& iOther) -> bool override;

	/**
	 * @brief
	 *  Identifier used to merge: the command kind and the widgets.
	 * @return The identifier.
	 */
	[[nodiscard]] auto typeId() const -> size_t override;

private:
	/// One change per widget.
	std::vector<HudRectChange> m_changes;
	/// Text of the history entry.
	std::string m_description;
};

/**
 * @brief
 *  Undoable move of a widget under another parent (or inside its parent), with the rectangle that keeps it in place.
 */
class HudReparentCommand final : public SceneUndoCommand {
public:
	HudReparentCommand(const HudReparentCommand&) = delete;

	HudReparentCommand(HudReparentCommand&&) = default;

	auto operator=(const HudReparentCommand&) -> HudReparentCommand& = delete;

	auto operator=(HudReparentCommand&&) -> HudReparentCommand& = default;

	/**
	 * @brief
	 *  Build the command.
	 * @param[in] iChange The widget and its rectangle before and after.
	 * @param[in] iOldSlot Parent and index before.
	 * @param[in] iNewSlot Parent and index after.
	 * @param[in] iName Tag of the widget, for the description.
	 */
	HudReparentCommand(const HudRectChange& iChange, const HierarchySlot& iOldSlot, const HierarchySlot& iNewSlot,
					   std::string iName);

	/**
	 * @brief
	 *  Destructor.
	 */
	~HudReparentCommand() override;

	/**
	 * @brief
	 *  Put the widget back under its parent, with its rectangle.
	 * @param[in,out] ioScene The scene holding the widget.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Move the widget under its new parent, with its new rectangle.
	 * @param[in,out] ioScene The scene holding the widget.
	 */
	void redo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Description of the history entry.
	 * @return The description.
	 */
	[[nodiscard]] auto description() const -> std::string override;

private:
	/// The widget and its rectangle before and after.
	HudRectChange m_change;
	/// Parent and index before.
	HierarchySlot m_oldSlot;
	/// Parent and index after.
	HierarchySlot m_newSlot;
	/// Tag of the widget.
	std::string m_name;
};

/**
 * @brief
 *  Names of the widgets `hud.add_widget` creates, in the order of the palette.
 * @return The kinds (`canvas`, `panel`, `label`...).
 */
[[nodiscard]] auto getHudWidgetKinds() -> const std::vector<std::string>&;

/**
 * @brief
 *  Find the rectangle that places a widget on a target, its anchors and pivot kept.
 * @param[in] iBase The rectangle whose anchors and pivot are kept.
 * @param[in] iParent The parent rectangle, in reference pixels.
 * @param[in] iTarget The rectangle the widget must cover, in the same coordinates.
 * @return The rectangle (size and offset) for which `computeBounds(iParent)` gives `iTarget`.
 */
[[nodiscard]] auto solveHudRect(const scene::component::UiRect& iBase, const scene::component::UiRect::Bounds& iParent,
								const scene::component::UiRect::Bounds& iTarget) -> scene::component::UiRect;

/**
 * @brief
 *  Get the rectangle a widget lives in, as the layout computes it.
 * @param[in,out] ioScene The scene (its HUD layout is recomputed for `iResolution`).
 * @param[in] iEntity The widget.
 * @param[in] iResolution Size in pixels the HUD is laid out at.
 * @return The parent content area in reference pixels (the whole canvas for a direct child), or nothing when the
 *  widget is not laid out (hidden, or under no canvas).
 */
[[nodiscard]] auto getHudParentBounds(scene::Scene& ioScene, const scene::Entity& iEntity,
									  const math::vec2ui& iResolution)
		-> std::optional<scene::component::UiRect::Bounds>;

/**
 * @brief
 *  Get the reference resolution of the canvas a widget hangs under.
 * @param[in] iScene The scene.
 * @param[in] iEntity The widget (or the canvas).
 * @return The resolution in pixels, or nothing when the entity is under no canvas.
 */
[[nodiscard]] auto getHudReferenceResolution(const scene::Scene& iScene, const scene::Entity& iEntity)
		-> std::optional<math::vec2ui>;

/**
 * @brief
 *  Register the `hud.*` commands.
 * @param[in,out] ioRegistry The registry that receives them.
 */
void addHudCommands(CommandRegistry& ioRegistry);

}// namespace owl::nest::commands
