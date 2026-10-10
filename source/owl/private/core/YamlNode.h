/**
 * @file YamlNode.h
 * @author Silmaen
 * @date 09/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/external/ryml.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace owl::core {

/**
 * @brief
 *  Error raised while reading YAML through rapidyaml: syntax error, missing node or failed conversion.
 */
class OWL_API YamlError final : public std::runtime_error {
public:
	YamlError(const YamlError&) = default;

	YamlError(YamlError&&) = default;

	auto operator=(const YamlError&) -> YamlError& = default;

	auto operator=(YamlError&&) -> YamlError& = default;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iMessage The error message.
	 */
	explicit YamlError(const std::string& iMessage) : std::runtime_error{iMessage} {}

	/**
	 * @brief
	 *  Destructor.
	 */
	~YamlError() override;
};

class YamlNode;

/**
 * @brief
 *  Conversion of a YAML node into a value, specialised per type (see `YamlNode::as`).
 * @tparam T The value type.
 */
template<typename T>
struct YamlRead;

/**
 * @brief
 *  Read-only view of a node of a rapidyaml tree, with the reading rules of yaml-cpp.
 *
 * A missing key gives an undefined node (false in a condition); a key without value, `~` or `null` gives a null
 * node; indexing a scalar throws, like yaml-cpp. The tree is owned by a `YamlDocument` that must outlive the view.
 */
class OWL_API YamlNode final {
public:
	/**
	 * @brief
	 *  Forward iterator over the children of a map or a sequence.
	 */
	class OWL_API Iterator final {
	public:
		using iterator_category = std::forward_iterator_tag;///< Iterator category.
		using value_type = YamlNode;///< Iterated value.
		using difference_type = std::ptrdiff_t;///< Distance type.
		using pointer = const YamlNode*;///< Pointer type (unused).
		using reference = YamlNode;///< Dereferenced type (a view, by value).

		/**
		 * @brief
		 *  Default constructor (end iterator).
		 */
		Iterator() = default;

		/**
		 * @brief
		 *  Constructor.
		 * @param[in] iTree The tree.
		 * @param[in] iId The current child id (`ryml::NONE` at the end).
		 */
		Iterator(const ryml::Tree* iTree, const ryml::id_type iId) noexcept : mp_tree{iTree}, m_id{iId} {}

		/**
		 * @brief
		 *  Access the current child.
		 * @return The child view.
		 */
		[[nodiscard]] auto operator*() const noexcept -> YamlNode;

		/**
		 * @brief
		 *  Move to the next sibling.
		 * @return This iterator.
		 */
		auto operator++() noexcept -> Iterator& {
			m_id = mp_tree->next_sibling(m_id);
			return *this;
		}

		/**
		 * @brief
		 *  Move to the next sibling (post-increment).
		 * @return The iterator before the move.
		 */
		auto operator++(int) noexcept -> Iterator {
			const Iterator old = *this;
			++*this;
			return old;
		}

		/**
		 * @brief
		 *  Comparison.
		 * @param[in] iOther The other iterator.
		 * @return True when both point at the same child.
		 */
		[[nodiscard]] auto operator==(const Iterator& iOther) const noexcept -> bool { return m_id == iOther.m_id; }

	private:
		/// The tree.
		const ryml::Tree* mp_tree = nullptr;
		/// The current child.
		ryml::id_type m_id = ryml::NONE;
	};

	/**
	 * @brief
	 *  Default constructor: an undefined node.
	 */
	YamlNode() = default;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iTree The tree.
	 * @param[in] iId The node id in the tree (`ryml::NONE` for an undefined node).
	 */
	YamlNode(const ryml::Tree* iTree, const ryml::id_type iId) noexcept : mp_tree{iTree}, m_id{iId} {}

	/**
	 * @brief
	 *  Check that the node exists (a present key, even with a null value).
	 * @return True when the node is defined.
	 */
	[[nodiscard]] explicit operator bool() const noexcept { return isDefined(); }

	/**
	 * @brief
	 *  Check that the node exists.
	 * @return True when the node is defined.
	 */
	[[nodiscard]] auto isDefined() const noexcept -> bool { return mp_tree != nullptr && m_id != ryml::NONE; }

	/**
	 * @brief
	 *  Check for a map.
	 * @return True when the node is a map.
	 */
	[[nodiscard]] auto isMap() const noexcept -> bool { return isDefined() && mp_tree->is_map(m_id); }

	/**
	 * @brief
	 *  Check for a sequence.
	 * @return True when the node is a sequence.
	 */
	[[nodiscard]] auto isSequence() const noexcept -> bool { return isDefined() && mp_tree->is_seq(m_id); }

	/**
	 * @brief
	 *  Check for a scalar that is not null.
	 * @return True when the node holds a non-null scalar.
	 */
	[[nodiscard]] auto isScalar() const noexcept -> bool;

	/**
	 * @brief
	 *  Check for a null value (no value, `~`, `null`, `Null` or `NULL`, unquoted).
	 * @return True when the node is defined and null.
	 */
	[[nodiscard]] auto isNull() const noexcept -> bool;

	/**
	 * @brief
	 *  Number of children of a map or a sequence.
	 * @return The child count, 0 for a scalar or an undefined node.
	 */
	[[nodiscard]] auto size() const noexcept -> size_t;

	/**
	 * @brief
	 *  Look up a key of a map.
	 * @param[in] iKey The key.
	 * @return The value, undefined when missing or when this node is not a map.
	 * @throw YamlError When this node is a non-null scalar.
	 */
	[[nodiscard]] auto operator[](std::string_view iKey) const -> YamlNode;

	/**
	 * @brief
	 *  Access an element of a sequence.
	 * @param[in] iIndex The position.
	 * @return The element, undefined when out of range or when this node is not a sequence.
	 * @throw YamlError When this node is a non-null scalar.
	 */
	[[nodiscard]] auto operator[](size_t iIndex) const -> YamlNode;

	/**
	 * @brief
	 *  Access the scalar text.
	 * @return The scalar, unescaped.
	 * @throw YamlError When the node is not a non-null scalar.
	 */
	[[nodiscard]] auto getScalar() const -> std::string_view;

	/**
	 * @brief
	 *  Access the key of a map entry.
	 * @return The key, empty for a sequence element or an undefined node.
	 */
	[[nodiscard]] auto getKey() const noexcept -> std::string_view;

	/**
	 * @brief
	 *  Line of the node in its source text, for the error messages.
	 * @return The 1-based line, 0 when unknown.
	 */
	[[nodiscard]] auto getLine() const noexcept -> size_t;

	/**
	 * @brief
	 *  Structural comparison: same keys in the same order, same sequences and the same scalar texts.
	 * @param[in] iOther The other node.
	 * @return True when both nodes hold the same data (the layout style is ignored).
	 */
	[[nodiscard]] auto isSameAs(const YamlNode& iOther) const noexcept -> bool;

	/**
	 * @brief
	 *  Write the node back as YAML text.
	 * @return The text of a map or a sequence (block layout), the scalar text, empty for a null or undefined node.
	 */
	[[nodiscard]] auto emit() const -> std::string;

	/**
	 * @brief
	 *  Convert the node into a value.
	 * @tparam T The value type (arithmetic, `bool`, `std::string` or a `YamlRead` specialisation).
	 * @return The value.
	 * @throw YamlError When the node is undefined or cannot be converted.
	 */
	template<typename T>
	[[nodiscard]] auto as() const -> T {
		if (!isDefined())
			throw YamlError("YAML: Invalid node, the key is missing.");
		T value{};
		if (!YamlRead<T>::decode(*this, value))
			throw YamlError(std::format("YAML: Bad conversion at line {}.", getLine()));
		return value;
	}

	/**
	 * @brief
	 *  Convert the node into a value, with a fallback.
	 * @tparam T The value type.
	 * @param[in] iFallback Value returned when the node is undefined or cannot be converted.
	 * @return The value or the fallback.
	 */
	template<typename T>
	[[nodiscard]] auto as(const T& iFallback) const -> T {
		if (!isDefined())
			return iFallback;
		T value{};
		if (!YamlRead<T>::decode(*this, value))
			return iFallback;
		return value;
	}

	/**
	 * @brief
	 *  First child of a map or a sequence.
	 * @return Iterator on the first child.
	 */
	[[nodiscard]] auto begin() const noexcept -> Iterator;

	/**
	 * @brief
	 *  End of the children.
	 * @return The end iterator.
	 */
	[[nodiscard]] auto end() const noexcept -> Iterator { return {mp_tree, ryml::NONE}; }

private:
	/// The tree, owned by a YamlDocument.
	const ryml::Tree* mp_tree = nullptr;
	/// Node id in the tree.
	ryml::id_type m_id = ryml::NONE;
};

/**
 * @brief
 *  A parsed YAML text (rapidyaml tree), the owner of the `YamlNode` views on it.
 */
class OWL_API YamlDocument final {
public:
	YamlDocument(const YamlDocument&) = delete;

	YamlDocument(YamlDocument&&) = delete;

	auto operator=(const YamlDocument&) -> YamlDocument& = delete;

	auto operator=(YamlDocument&&) -> YamlDocument& = delete;

	/**
	 * @brief
	 *  Parse a YAML text (its first document, like `YAML::Load`).
	 * @param[in] iYaml The text, copied into the tree.
	 * @param[in] iSourceName File or buffer name for the error messages.
	 * @throw YamlError On a syntax error.
	 */
	explicit YamlDocument(std::string_view iYaml, std::string_view iSourceName = {});

	/**
	 * @brief
	 *  Destructor.
	 */
	~YamlDocument() = default;

	/**
	 * @brief
	 *  Access the root node.
	 * @return The root of the first document (null for an empty text).
	 */
	[[nodiscard]] auto getRoot() const noexcept -> YamlNode { return {&m_tree, m_root}; }

private:
	/// The parsed tree.
	ryml::Tree m_tree;
	/// Root of the first document.
	ryml::id_type m_root = ryml::NONE;
};

/// @cond
namespace detail {
OWL_API auto readBool(std::string_view iText, bool& oValue) noexcept -> bool;
OWL_API auto readSigned(std::string_view iText, int64_t& oValue) noexcept -> bool;
OWL_API auto readUnsigned(std::string_view iText, uint64_t& oValue) noexcept -> bool;
OWL_API auto readFloat(std::string_view iText, float& oValue) noexcept -> bool;
OWL_API auto readDouble(std::string_view iText, double& oValue) noexcept -> bool;
}// namespace detail
/// @endcond

/**
 * @brief
 *  Booleans: `true` / `false`, `yes` / `no`, `on` / `off`, `y` / `n`, in lower, upper or capitalised case.
 */
template<>
struct YamlRead<bool> {
	/**
	 * @brief
	 *  Decode a node.
	 * @param[in] iNode The node.
	 * @param[out] oValue The value.
	 * @return True on success.
	 */
	static auto decode(const YamlNode& iNode, bool& oValue) -> bool {
		return iNode.isScalar() && detail::readBool(iNode.getScalar(), oValue);
	}
};

/**
 * @brief
 *  Integers: decimal or `0x` hexadecimal, range-checked; a negative text never decodes into an unsigned type.
 * @tparam T The integer type.
 */
template<std::integral T>
	requires(!std::same_as<T, bool>)
struct YamlRead<T> {
	/**
	 * @brief
	 *  Decode a node.
	 * @param[in] iNode The node.
	 * @param[out] oValue The value.
	 * @return True on success.
	 */
	static auto decode(const YamlNode& iNode, T& oValue) -> bool {
		if (!iNode.isScalar())
			return false;
		if constexpr (std::is_signed_v<T>) {
			int64_t value = 0;
			if (!detail::readSigned(iNode.getScalar(), value) || value < std::numeric_limits<T>::min() ||
				value > std::numeric_limits<T>::max())
				return false;
			oValue = static_cast<T>(value);
		} else {
			uint64_t value = 0;
			if (!detail::readUnsigned(iNode.getScalar(), value) || value > std::numeric_limits<T>::max())
				return false;
			oValue = static_cast<T>(value);
		}
		return true;
	}
};

/**
 * @brief
 *  Single-precision floats, with `.inf`, `-.inf` and `.nan`.
 */
template<>
struct YamlRead<float> {
	/**
	 * @brief
	 *  Decode a node.
	 * @param[in] iNode The node.
	 * @param[out] oValue The value.
	 * @return True on success.
	 */
	static auto decode(const YamlNode& iNode, float& oValue) -> bool {
		return iNode.isScalar() && detail::readFloat(iNode.getScalar(), oValue);
	}
};

/**
 * @brief
 *  Double-precision floats, with `.inf`, `-.inf` and `.nan`.
 */
template<>
struct YamlRead<double> {
	/**
	 * @brief
	 *  Decode a node.
	 * @param[in] iNode The node.
	 * @param[out] oValue The value.
	 * @return True on success.
	 */
	static auto decode(const YamlNode& iNode, double& oValue) -> bool {
		return iNode.isScalar() && detail::readDouble(iNode.getScalar(), oValue);
	}
};

/**
 * @brief
 *  Strings: the scalar text; a null node reads `null`, like yaml-cpp.
 */
template<>
struct YamlRead<std::string> {
	/**
	 * @brief
	 *  Decode a node.
	 * @param[in] iNode The node.
	 * @param[out] oValue The value.
	 * @return True on success.
	 */
	static auto decode(const YamlNode& iNode, std::string& oValue) -> bool {
		if (iNode.isNull()) {
			oValue = "null";
			return true;
		}
		if (!iNode.isScalar())
			return false;
		oValue = iNode.getScalar();
		return true;
	}
};

}// namespace owl::core
