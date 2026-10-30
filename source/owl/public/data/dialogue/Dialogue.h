/**
 * @file Dialogue.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/FormatVersion.h"
#include "core/UUID.h"
#include "math/vectors.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief
 *  Branching conversations (`.owldlg`): the graph, its file and its playback.
 */
namespace owl::data::dialogue {

/**
 * @brief
 *  Kind of a dialogue node.
 */
enum struct NodeKind : uint8_t {
	Start,///< Entry of the conversation, followed by its `next` node.
	Line,///< A speaker says a text, then the conversation goes on to `next`.
	Choice,///< A speaker says a text, then the player picks one of the options.
	Branch,///< A Lua condition picks `next` (true) or `nextFalse` (false).
	Event,///< Calls a Lua function, then goes on to `next`.
	End,///< Ends the conversation.
};

/**
 * @brief
 *  A text shown to the player: its value, and the localisation key that translates it.
 */
struct OWL_API DialogueText {
	/// Text shown when the key is empty or has no translation.
	std::string text;
	/// Key of the string tables (`localisation.yml`) translating the text; empty: the text as written.
	std::string key;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other text.
	 * @return True when both are the same.
	 */
	auto operator==(const DialogueText& iOther) const -> bool = default;
};

/**
 * @brief
 *  An answer the player can pick at a Choice node.
 */
struct OWL_API DialogueOption {
	/// Text of the answer.
	DialogueText text;
	/// Lua function returning true when the option is offered; empty: always offered.
	std::string condition;
	/// Node the option leads to; a null id ends the conversation.
	core::UUID next{0};

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other option.
	 * @return True when both are the same.
	 */
	auto operator==(const DialogueOption& iOther) const -> bool = default;
};

/**
 * @brief
 *  A node of a dialogue graph.
 */
struct OWL_API DialogueNode {
	/// Identifier, unique in the dialogue and stable across saves.
	core::UUID id{0};
	/// What the node does.
	NodeKind kind{NodeKind::Line};
	/// Top-left corner in the graph editor.
	math::vec2f position{0.f, 0.f};
	/// Who speaks (Line and Choice).
	DialogueText speaker;
	/// What is said (Line and Choice).
	DialogueText text;
	/// Voice line or sound played with the text (Line and Choice); empty: none.
	std::string sound;
	/// Lua function: the condition of a Branch, the action of an Event.
	std::string function;
	/// Answers of a Choice, in the order they are offered.
	std::vector<DialogueOption> options;
	/// Following node (Start, Line, Event, the true side of a Branch); a null id ends the conversation.
	core::UUID next{0};
	/// Following node when the condition of a Branch is false; a null id ends the conversation.
	core::UUID nextFalse{0};

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other node.
	 * @return True when both are the same.
	 */
	auto operator==(const DialogueNode& iOther) const -> bool = default;
};

/**
 * @brief
 *  Bends a link of the graph editor goes through, kept with the dialogue.
 */
struct OWL_API DialogueRoute {
	/// Node the link leaves.
	core::UUID node{0};
	/// Output of the node it leaves (see `Dialogue::getOutput`).
	uint32_t output{0};
	/// Waypoints in canvas space, from the output to the next node.
	std::vector<math::vec2f> points;
	/// Colour name of the link in the editor (`Red`, `Green`...), empty for the default one.
	std::string color{};

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other route.
	 * @return True when both are the same.
	 */
	auto operator==(const DialogueRoute& iOther) const -> bool = default;
};

/**
 * @brief
 *  Problem found by `Dialogue::getProblems`, shown by the editor.
 */
struct OWL_API DialogueProblem {
	/// Node concerned; a null id for the whole dialogue.
	core::UUID node{0};
	/// Sentence describing the problem.
	std::string message;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other problem.
	 * @return True when both are the same.
	 */
	auto operator==(const DialogueProblem& iOther) const -> bool = default;
};

/**
 * @brief
 *  A branching conversation: the nodes of a `.owldlg` file, entered at its Start node.
 */
struct OWL_API Dialogue {
	/// Nodes, in the order of the file.
	std::vector<DialogueNode> nodes;
	/// Bends of the links drawn by the graph editor.
	std::vector<DialogueRoute> routes;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other dialogue.
	 * @return True when both are the same.
	 */
	auto operator==(const Dialogue& iOther) const -> bool = default;

	/**
	 * @brief
	 *  Extension of a dialogue file.
	 * @return `.owldlg`.
	 */
	static auto fileExtension() noexcept -> const char* { return ".owldlg"; }

	/**
	 * @brief
	 *  Whether a path names a dialogue file.
	 * @param[in] iPath The path.
	 * @return True for a `.owldlg` file.
	 */
	[[nodiscard]] static auto isDialogue(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Versioned format of the file.
	 * @return The format descriptor.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/**
	 * @brief
	 *  A dialogue holding one Start node linked to an End node, as Owl Nest creates it.
	 * @return The new dialogue.
	 */
	[[nodiscard]] static auto makeDefault() -> Dialogue;

	/**
	 * @brief
	 *  Write the dialogue as YAML.
	 * @return The YAML text.
	 */
	[[nodiscard]] auto serializeToString() const -> std::string;

	/**
	 * @brief
	 *  Read a dialogue from YAML; nodes without id or with an id seen before are skipped.
	 * @param[in] iYaml The YAML text.
	 * @return The dialogue, or nothing when the text is not a dialogue.
	 */
	[[nodiscard]] static auto deserializeFromString(std::string_view iYaml) -> std::optional<Dialogue>;

	/**
	 * @brief
	 *  Write the dialogue to a file, atomically.
	 * @param[in] iPath The file path.
	 * @return True when written.
	 */
	[[nodiscard]] auto saveToFile(const std::filesystem::path& iPath) const -> bool;

	/**
	 * @brief
	 *  Read a dialogue from a file on disk.
	 * @param[in] iPath The file path.
	 * @return The dialogue, or nothing when it cannot be read.
	 */
	[[nodiscard]] static auto loadFile(const std::filesystem::path& iPath) -> std::optional<Dialogue>;

	/**
	 * @brief
	 *  Read a dialogue asset from the pack or the asset folders.
	 * @param[in] iAsset Asset path of the dialogue.
	 * @return The dialogue, or nothing when it cannot be read.
	 */
	[[nodiscard]] static auto load(const std::filesystem::path& iAsset) -> std::optional<Dialogue>;

	/**
	 * @brief
	 *  A node by id.
	 * @param[in] iId The node id.
	 * @return The node, null when absent.
	 */
	[[nodiscard]] auto findNode(core::UUID iId) const -> const DialogueNode*;

	/**
	 * @brief
	 *  A node by id, for editing.
	 * @param[in] iId The node id.
	 * @return The node, null when absent.
	 */
	[[nodiscard]] auto findNode(core::UUID iId) -> DialogueNode*;

	/**
	 * @brief
	 *  Number of outputs of a node: one for Start, Line and Event, two for Branch (true, false), one per option of a
	 *  Choice, none for End.
	 * @param[in] iNode The node.
	 * @return The count.
	 */
	[[nodiscard]] static auto getOutputCount(const DialogueNode& iNode) -> size_t;

	/**
	 * @brief
	 *  The node an output leads to.
	 * @param[in] iNode The node.
	 * @param[in] iOutput Index of the output.
	 * @return The node id, null when the output is not linked or does not exist.
	 */
	[[nodiscard]] static auto getOutput(const DialogueNode& iNode, size_t iOutput) -> core::UUID;

	/**
	 * @brief
	 *  Link an output to a node.
	 * @param[in,out] ioNode The node.
	 * @param[in] iOutput Index of the output.
	 * @param[in] iTarget The node it leads to, null to unlink.
	 * @return False when the node has no such output.
	 */
	static auto setOutput(DialogueNode& ioNode, size_t iOutput, core::UUID iTarget) -> bool;

	/**
	 * @brief
	 *  The bends of the link of an output.
	 * @param[in] iNode The node the link leaves.
	 * @param[in] iOutput Index of the output.
	 * @return The route, null when the link is straight.
	 */
	[[nodiscard]] auto findRoute(core::UUID iNode, size_t iOutput) const -> const DialogueRoute*;

	/**
	 * @brief
	 *  The node the conversation starts at.
	 * @return The first Start node, null when there is none.
	 */
	[[nodiscard]] auto findStart() const -> const DialogueNode*;

	/**
	 * @brief
	 *  Problems of the graph: no or several Start nodes, links to missing nodes, a Choice without option, a Branch or
	 *  an Event without function, an unreachable node.
	 * @return The problems, empty for a sound dialogue.
	 */
	[[nodiscard]] auto getProblems() const -> std::vector<DialogueProblem>;

	/**
	 * @brief
	 *  Localisation keys used by the texts of the dialogue, each once.
	 * @return The keys, in the order of the nodes.
	 */
	[[nodiscard]] auto getTextKeys() const -> std::vector<std::string>;

	/**
	 * @brief
	 *  Lua functions named by the conditions and actions of the dialogue, each once.
	 * @return The function names, in the order of the nodes.
	 */
	[[nodiscard]] auto getFunctions() const -> std::vector<std::string>;
};

/**
 * @brief
 *  Name of a node kind, as written in the file.
 * @param[in] iKind The kind.
 * @return Its name.
 */
OWL_API auto getNodeKindName(NodeKind iKind) -> std::string_view;

/**
 * @brief
 *  Node kind from its name.
 * @param[in] iName The name, as written in the file.
 * @return The kind, nothing for an unknown name.
 */
OWL_API auto parseNodeKind(std::string_view iName) -> std::optional<NodeKind>;

/**
 * @brief
 *  Every node kind, in declaration order.
 * @return The kinds.
 */
OWL_API auto getNodeKinds() -> std::span<const NodeKind>;

}// namespace owl::data::dialogue
