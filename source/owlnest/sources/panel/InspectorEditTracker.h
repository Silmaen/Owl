/**
 * @file InspectorEditTracker.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owlgui.h>

#include "../commands/CommandRegistry.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace owl::nest::panel {

/**
 * @brief
 *  Turns the inspector's component edits into undo steps without serializing anything while nothing is
 *  edited.
 *
 * Each component body is wrapped between `beginComponent` and `endComponent`. An edit session opens only
 * when the user interacts with the body: a click or a release over it, its widget being active, or a key
 * typed while the keyboard focus is inside it. The session captures the component alone (never the whole
 * entity), stays open while a widget of the body is active, a popup it opened is shown or a mouse button
 * pressed over it is held, and ends with a single `ModifyEntityCommand` when the component changed —
 * prefab override marks included. A drag therefore records one step, and the undo manager's 1 s merge
 * still folds quick successive edits together.
 */
class InspectorEditTracker final {
public:
	/**
	 * @brief
	 *  Start an inspector frame.
	 *
	 * Ends the sessions left open on a previously inspected entity when the selection changed.
	 * @param[in] iEntity The inspected entity (may be invalid).
	 * @param[in] iCommands The scene and the registry the edits go through (`component.set`).
	 */
	void beginFrame(const scene::Entity& iEntity, const commands::CommandTarget& iCommands);

	/**
	 * @brief
	 *  Open the body of a component; call right before drawing its properties.
	 * @param[in] iEntity The inspected entity.
	 * @param[in] iComponentKey The component key (`Component::key()`).
	 */
	void beginComponent(const scene::Entity& iEntity, const std::string& iComponentKey);

	/**
	 * @brief
	 *  Close the body of a component; call right after drawing its properties.
	 * @param[in] iComponentKey The component key (`Component::key()`).
	 * @param[in] iComponentName The component display name, used in the undo step description.
	 * @param[in] iCommands The scene and the registry the edits go through.
	 */
	void endComponent(const std::string& iComponentKey, const std::string& iComponentName,
					  const commands::CommandTarget& iCommands);

	/**
	 * @brief
	 *  End an inspector frame: end the sessions of the components that were not drawn.
	 * @param[in] iCommands The scene and the registry the edits go through.
	 */
	void endFrame(const commands::CommandTarget& iCommands);

	/**
	 * @brief
	 *  End every open session now and forget the tracked entity.
	 * @param[in] iCommands The scene and the registry the edits go through.
	 */
	void flush(const commands::CommandTarget& iCommands);

	/**
	 * @brief
	 *  Check whether an edit session is open.
	 * @return True while at least one component is being edited.
	 */
	[[nodiscard]] auto isEditing() const -> bool;

	/**
	 * @brief
	 *  Number of serializations every tracker made since the process started (component, entity and
	 *  rebuilt entity YAML each count one).
	 * @return The serialization count.
	 */
	[[nodiscard]] static auto serializationCount() -> uint64_t;

private:
	/**
	 * @brief
	 *  What the tracker knows about one component body.
	 */
	struct ComponentState {
		/// Top-left corner of the body during the last frame it was drawn.
		math::vec2 rectMin{0.f, 0.f};
		/// Bottom-right corner of the body during the last frame it was drawn.
		math::vec2 rectMax{0.f, 0.f};
		/// True once the body rectangle is known.
		bool hasRect = false;
		/// True when the active widget belonged to the body last frame.
		bool containsActive = false;
		/// True when the keyboard navigation focus was inside the body last frame.
		bool hasNavFocus = false;
		/// Popup stack depth when the body started, while it shows a popup it opened (-1 otherwise).
		int ownedPopupDepth = -1;
		/// Popup stack depth at `beginComponent`.
		int popupDepthAtBegin = 0;
		/// True when the navigation focus was already seen this frame at `beginComponent`.
		bool navSeenAtBegin = false;
		/// True when the body was drawn during the current frame.
		bool drawn = false;
		/// Component YAML captured when the session opened (no value while no session is open).
		std::optional<std::string> before;
		/// Display name of the component.
		std::string name;
	};

	/**
	 * @brief
	 *  Check whether the user interacts with a body this frame.
	 * @param[in] iState The body state from the previous frame.
	 * @return True when an edit session has to open before the body is drawn.
	 */
	[[nodiscard]] static auto shouldOpen(const ComponentState& iState) -> bool;

	/**
	 * @brief
	 *  End a session: when the component changed, put it back as it was and run `component.set` with its new
	 *  fields, so the edit is a registry command (journaled, undoable, replayable).
	 * @param[in] iComponentKey The component key.
	 * @param[in,out] ioState The body state; its session is closed.
	 * @param[in] iCommands The scene and the registry the edit goes through (an incomplete target keeps the
	 *  edit unrecorded).
	 */
	void commit(const std::string& iComponentKey, ComponentState& ioState,
				const commands::CommandTarget& iCommands) const;

	/// UUID of the inspected entity.
	core::UUID m_entityUuid{0};
	/// Body state per component key.
	std::unordered_map<std::string, ComponentState> m_states;
};

}// namespace owl::nest::panel
