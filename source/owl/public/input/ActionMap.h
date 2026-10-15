/**
 * @file ActionMap.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/FormatVersion.h"
#include "core/Macros.h"
#include "core/expected.h"
#include "input/Gamepad.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::input {

/**
 * @brief
 *  What an action reports.
 */
enum struct ActionKind : uint8_t {
	Button,///< On or off: held, with press and release edges.
	Axis,///< One value in [-1, 1].
	Vector///< Two values in the unit disc (x right, y down, like a gamepad stick).
};

/**
 * @brief
 *  The device input a binding reads.
 */
enum struct BindingSource : uint8_t {
	Key,///< A keyboard key (`ActionBinding::code` is a `KeyCode`).
	MouseButton,///< A mouse button (`MouseCode`).
	GamepadButton,///< A gamepad button (`GamepadButton`).
	GamepadAxis,///< A gamepad axis, through its dead zone and curve (`GamepadAxis`).
	GamepadStick///< A gamepad stick (0 left, 1 right), through its radial dead zone and curve.
};

/**
 * @brief
 *  Which component of a `Vector` action a one-dimensional binding feeds.
 */
enum struct BindingComponent : uint8_t {
	Both,///< The whole stick; for a one-dimensional source, same as `X`.
	X,///< The horizontal component.
	Y///< The vertical component.
};

/**
 * @brief
 *  One physical input feeding an action.
 *
 * A one-dimensional input (key, button, axis) reads 0 or 1 (an axis reads its shaped value) and is multiplied by
 * `scale`: a `Key` bound with `scale = -1` pushes an `Axis` action towards -1. Bindings of an action add up, then the
 * result is clamped to the unit range (a `Vector` longer than 1 is shortened, so a diagonal is not faster).
 */
struct OWL_API ActionBinding {
	/// Device input read.
	BindingSource source{BindingSource::Key};
	/// Code of the input: key, mouse button, gamepad button, gamepad axis, or stick (0 left, 1 right).
	uint16_t code{0};
	/// Gamepad slot read by a gamepad source, -1 for any connected gamepad (the strongest reading wins).
	int32_t pad{-1};
	/// Component of a `Vector` action fed by a one-dimensional source.
	BindingComponent component{BindingComponent::Both};
	/// Factor applied to the reading (-1 reverses it).
	float scale{1.f};
	/// Dead zone and response curve of an axis or stick source.
	AxisResponse response{};

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Equality comparison.
	 * @param[in] iOther The other binding.
	 * @return True if equal.
	 */
	auto operator==(const ActionBinding& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  A named action and its bindings.
 */
struct OWL_API ActionDefinition {
	/// Unique name, the key scripts and systems read the action by.
	std::string name;
	/// What the action reports.
	ActionKind kind{ActionKind::Button};
	/// Physical inputs feeding the action.
	std::vector<ActionBinding> bindings;

	/**
	 * @brief
	 *  Equality comparison.
	 * @param[in] iOther The other definition.
	 * @return True if equal.
	 */
	auto operator==(const ActionDefinition& iOther) const -> bool = default;
};

/**
 * @brief
 *  The actions of a game and their bindings: the `input_actions.yml` file of a project.
 *
 * Gameplay reads actions, never keys: a player rebinds the game, a gamepad plays it, a scenario holds an action
 * by name. The map is plain data that loads from and saves to YAML.
 *
 * The file format (`FormatVersion` 1, a list of `actions` with `name`, `type` and `bindings`) is described in
 * `doc/pages/event_input.md`.
 *
 * Binding keys: one source among `key`, `mouse_button`, `gamepad_button`, `gamepad_axis` (see `input/InputNames.h`),
 * `gamepad_stick` (`left`, `right`); optional `pad` (slot, any when absent), `component` (`x` or `y`), `scale`,
 * `dead_zone`, `curve` (`linear`, `quadratic`, `cubic`, `custom`) and `exponent`.
 */
class OWL_API ActionMap {
public:
	/// Name of the file in a project, in the pack and in the asset folders.
	static constexpr std::string_view fileName = "input_actions.yml";

	/**
	 * @brief
	 *  The built-in actions engine systems read: `move`, `look`, `jump`, `descend`, `sprint`, `boost`, `interact`,
	 *  `primary`, `secondary` and the `ui_*` navigation.
	 * @return The default map.
	 */
	[[nodiscard]] static auto createDefault() -> ActionMap;

	/**
	 * @brief
	 *  Shared read-only default map, for a scene that has no application context.
	 * @return The default map.
	 */
	[[nodiscard]] static auto getDefault() -> shared<const ActionMap>;

	/**
	 * @brief
	 *  Format descriptor of the file (name and migrations).
	 * @return The descriptor.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/**
	 * @brief
	 *  Parse a map from YAML text.
	 * @param[in] iText The YAML text.
	 * @param[in] iSourceName File or buffer name for the messages.
	 * @return The map, or the reason it cannot be read (unknown action type, binding without source, duplicate name...).
	 */
	[[nodiscard]] static auto fromYaml(std::string_view iText, std::string_view iSourceName = "input actions")
			-> expected<ActionMap, std::string>;

	/**
	 * @brief
	 *  Read a map file.
	 * @param[in] iFile The `input_actions.yml` file.
	 * @return The map, or the reason it cannot be read.
	 */
	[[nodiscard]] static auto loadFile(const std::filesystem::path& iFile) -> expected<ActionMap, std::string>;

	/**
	 * @brief
	 *  Write the map as YAML text (with its `FormatVersion`).
	 * @return The YAML text.
	 */
	[[nodiscard]] auto toYaml() const -> std::string;

	/**
	 * @brief
	 *  Write the map to a file, atomically.
	 * @param[in] iFile The file to write.
	 * @return True if the file was written.
	 */
	[[nodiscard]] auto saveFile(const std::filesystem::path& iFile) const -> bool;

	/**
	 * @brief
	 *  The actions, in file order.
	 * @return The definitions.
	 */
	[[nodiscard]] auto getActions() const -> const std::vector<ActionDefinition>& { return m_actions; }

	/**
	 * @brief
	 *  Number of actions.
	 * @return The count.
	 */
	[[nodiscard]] auto size() const -> std::size_t { return m_actions.size(); }

	/**
	 * @brief
	 *  Position of an action by name.
	 * @param[in] iName The action name.
	 * @return The index in `getActions()`, or nothing for an unknown name.
	 */
	[[nodiscard]] auto indexOf(std::string_view iName) const -> std::optional<std::size_t>;

	/**
	 * @brief
	 *  Definition of an action by name.
	 * @param[in] iName The action name.
	 * @return The definition, or nullptr for an unknown name.
	 */
	[[nodiscard]] auto find(std::string_view iName) const -> const ActionDefinition*;

	/**
	 * @brief
	 *  Add an action, or replace the one of the same name in place.
	 * @param[in] iAction The definition (a definition without a name is refused).
	 * @return True if stored.
	 */
	auto setAction(const ActionDefinition& iAction) -> bool;

	/**
	 * @brief
	 *  Remove an action.
	 * @param[in] iName The action name.
	 * @return True if the action existed.
	 */
	auto removeAction(std::string_view iName) -> bool;

	/**
	 * @brief
	 *  This map completed with the built-in actions it does not define: engine systems always find their actions.
	 * @return The completed map; an action of this map replaces the built-in one of the same name.
	 */
	[[nodiscard]] auto withDefaults() const -> ActionMap;

	/**
	 * @brief
	 *  Equality comparison.
	 * @param[in] iOther The other map.
	 * @return True if both hold the same actions in the same order.
	 */
	auto operator==(const ActionMap& iOther) const -> bool = default;

private:
	/// Actions, in file order.
	std::vector<ActionDefinition> m_actions;
};

}// namespace owl::input
