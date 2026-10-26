/**
 * @file YamlNode.cpp
 * @author Silmaen
 * @date 09/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/YamlNode.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <tuple>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>
#include <unordered_map>

namespace owl::core {

namespace {

constexpr uint32_t g_binaryVersion = 1;

/// Flags of a node in the binary form.
enum struct BinaryNode : uint8_t {
	Key = 1U << 0U,///< The node has a key.
	Val = 1U << 1U,///< The node has a value.
	Map = 1U << 2U,///< The node is a map.
	Seq = 1U << 3U,///< The node is a sequence.
	Null = 1U << 4U,///< The value is null.
	Quoted = 1U << 5U,///< The value was quoted (never read as null).
};

constexpr auto has(const uint8_t iFlags, const BinaryNode iFlag) -> bool {
	return (iFlags & static_cast<uint8_t>(iFlag)) != 0;
}

void appendU32(std::string& ioOut, const uint32_t iValue) {
	for (uint32_t shift = 0; shift < 32; shift += 8) ioOut.push_back(static_cast<char>((iValue >> shift) & 0xFFU));
}

// Bounds-checked reader of the binary form: the bytes may come from an untrusted pack.
class BinaryReader final {
public:
	BinaryReader(const std::string_view iBytes, const std::string_view iSourceName)
		: m_bytes{iBytes}, m_source{iSourceName} {}

	auto u8() -> uint8_t {
		need(1);
		return static_cast<uint8_t>(m_bytes[m_offset++]);
	}

	auto u32() -> uint32_t {
		need(4);
		uint32_t value = 0;
		for (uint32_t shift = 0; shift < 32; shift += 8)
			value |= static_cast<uint32_t>(static_cast<uint8_t>(m_bytes[m_offset++])) << shift;
		return value;
	}

	auto bytes(const size_t iCount) -> std::string_view {
		need(iCount);
		const auto view = m_bytes.substr(m_offset, iCount);
		m_offset += iCount;
		return view;
	}

	[[nodiscard]] auto remaining() const -> size_t { return m_bytes.size() - m_offset; }

	[[noreturn]] void fail(const std::string_view iWhat) const {
		throw YamlError(std::format("YAML: Binary tree '{}' is {}.", m_source, iWhat));
	}

private:
	void need(const size_t iCount) const {
		if (iCount > remaining())
			fail("truncated");
	}

	std::string_view m_bytes;
	std::string_view m_source;
	size_t m_offset{0};
};

auto toView(const ryml::csubstr iText) noexcept -> std::string_view { return {iText.str, iText.len}; }

[[noreturn]] void throwBasic(const ryml::csubstr iMsg, const ryml::ErrorDataBasic& /*iData*/, void* /*iUser*/) {
	throw YamlError(std::format("YAML: {}.", toView(iMsg)));
}

[[noreturn]] void throwParse(const ryml::csubstr iMsg, const ryml::ErrorDataParse& iData, void* /*iUser*/) {
	if (iData.ymlloc.line != ryml::npos)
		throw YamlError(std::format("YAML: {} at line {}, column {}.", toView(iMsg), iData.ymlloc.line + 1,
									iData.ymlloc.col + 1));
	throw YamlError(std::format("YAML: {}.", toView(iMsg)));
}

[[noreturn]] void throwVisit(const ryml::csubstr iMsg, const ryml::ErrorDataVisit& /*iData*/, void* /*iUser*/) {
	throw YamlError(std::format("YAML: {}.", toView(iMsg)));
}

auto throwingCallbacks() -> const ryml::Callbacks& {
	static const ryml::Callbacks callbacks = [] -> ryml::Callbacks {
		ryml::Callbacks cb;
		cb.set_error_basic(&throwBasic).set_error_parse(&throwParse).set_error_visit(&throwVisit);
		return cb;
	}();
	return callbacks;
}

// yaml-cpp accepts the lower, upper and capitalised forms of each name.
auto matchesName(const std::string_view iText, const std::string_view iLower) noexcept -> bool {
	if (iText.size() != iLower.size())
		return false;
	const auto upper = [](const char iChar) -> char { return static_cast<char>(iChar - 'a' + 'A'); };
	const bool lower = iText == iLower;
	bool allUpper = true;
	bool capitalised = upper(iLower.front()) == iText.front();
	for (size_t i = 0; i < iText.size(); ++i) {
		allUpper = allUpper && iText[i] == upper(iLower[i]);
		capitalised = capitalised && (i == 0 || iText[i] == iLower[i]);
	}
	return lower || allUpper || capitalised;
}

auto stripPlus(const std::string_view iText) noexcept -> std::string_view {
	if (iText.size() > 1 && iText.front() == '+' && iText[1] != '-')
		return iText.substr(1);
	return iText;
}

template<typename T>
auto readInteger(std::string_view iText, T& oValue) noexcept -> bool {
	iText = stripPlus(iText);
	bool negative = false;
	std::string_view digits = iText;
	if (!digits.empty() && digits.front() == '-') {
		negative = true;
		digits.remove_prefix(1);
	}
	int base = 10;
	if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
		base = 16;
		digits.remove_prefix(2);
	}
	if (digits.empty() || digits.front() == '-' || digits.front() == '+')
		return false;
	if (base == 10) {
		const auto [ptr, err] = std::from_chars(iText.data(), iText.data() + iText.size(), oValue);
		return err == std::errc{} && ptr == iText.data() + iText.size();
	}
	uint64_t magnitude = 0;
	if (const auto [ptr, err] = std::from_chars(digits.data(), digits.data() + digits.size(), magnitude, base);
		err != std::errc{} || ptr != digits.data() + digits.size())
		return false;
	if constexpr (std::is_signed_v<T>) {
		if (magnitude > static_cast<uint64_t>(std::numeric_limits<T>::max()) + (negative ? 1U : 0U))
			return false;
		oValue = negative ? static_cast<T>(-static_cast<T>(magnitude - 1) - 1) : static_cast<T>(magnitude);
	} else {
		if (negative)
			return false;
		oValue = static_cast<T>(magnitude);
	}
	return true;
}

template<typename T>
auto readFloating(std::string_view iText, T& oValue) noexcept -> bool {
	iText = stripPlus(iText);
	const bool negative = !iText.empty() && iText.front() == '-';
	if (const auto body = negative ? iText.substr(1) : iText; !body.empty() && body.front() == '.') {
		if (body == ".inf" || body == ".Inf" || body == ".INF") {
			oValue = negative ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity();
			return true;
		}
		if (!negative && (body == ".nan" || body == ".NaN" || body == ".NAN")) {
			oValue = std::numeric_limits<T>::quiet_NaN();
			return true;
		}
	}
	const auto [ptr, err] = std::from_chars(iText.data(), iText.data() + iText.size(), oValue);
	return err == std::errc{} && ptr == iText.data() + iText.size() && std::isfinite(oValue);
}

}// namespace

YamlError::~YamlError() = default;

namespace detail {

auto readBool(const std::string_view iText, bool& oValue) noexcept -> bool {
	static constexpr std::array<std::pair<std::string_view, std::string_view>, 4> names{
			{{"y", "n"}, {"yes", "no"}, {"true", "false"}, {"on", "off"}}};
	for (const auto& [yes, no]: names) {
		if (matchesName(iText, yes)) {
			oValue = true;
			return true;
		}
		if (matchesName(iText, no)) {
			oValue = false;
			return true;
		}
	}
	return false;
}

auto readSigned(const std::string_view iText, int64_t& oValue) noexcept -> bool { return readInteger(iText, oValue); }

auto readUnsigned(const std::string_view iText, uint64_t& oValue) noexcept -> bool {
	return readInteger(iText, oValue);
}

auto readFloat(const std::string_view iText, float& oValue) noexcept -> bool { return readFloating(iText, oValue); }

auto readDouble(const std::string_view iText, double& oValue) noexcept -> bool { return readFloating(iText, oValue); }

}// namespace detail

auto YamlNode::Iterator::operator*() const noexcept -> YamlNode { return {mp_tree, m_id}; }

auto YamlNode::isScalar() const noexcept -> bool {
	return isDefined() && mp_tree->has_val(m_id) && !mp_tree->val_is_null(m_id);
}

auto YamlNode::isNull() const noexcept -> bool {
	if (!isDefined() || mp_tree->is_container(m_id))
		return false;
	return !mp_tree->has_val(m_id) || mp_tree->val_is_null(m_id);
}

auto YamlNode::size() const noexcept -> size_t {
	if (!isDefined() || !mp_tree->is_container(m_id))
		return 0;
	return mp_tree->num_children(m_id);
}

auto YamlNode::operator[](const std::string_view iKey) const -> YamlNode {
	if (!isDefined())
		return {};
	if (isScalar())
		throw YamlError(std::format("YAML: Cannot look up '{}' in a scalar at line {}.", iKey, getLine()));
	if (!mp_tree->is_map(m_id))
		return {};
	for (auto child = mp_tree->first_child(m_id); child != ryml::NONE; child = mp_tree->next_sibling(child)) {
		if (toView(mp_tree->key(child)) == iKey)
			return {mp_tree, child};
	}
	return {mp_tree, ryml::NONE};
}

auto YamlNode::operator[](const size_t iIndex) const -> YamlNode {
	if (!isDefined())
		return {};
	if (isScalar())
		throw YamlError(std::format("YAML: Cannot index a scalar at line {}.", getLine()));
	if (!mp_tree->is_seq(m_id))
		return {};
	size_t pos = 0;
	for (auto child = mp_tree->first_child(m_id); child != ryml::NONE; child = mp_tree->next_sibling(child), ++pos) {
		if (pos == iIndex)
			return {mp_tree, child};
	}
	return {mp_tree, ryml::NONE};
}

auto YamlNode::getScalar() const -> std::string_view {
	if (!isScalar())
		throw YamlError(std::format("YAML: Expected a scalar at line {}.", getLine()));
	return toView(mp_tree->val(m_id));
}

auto YamlNode::getKey() const noexcept -> std::string_view {
	if (!isDefined() || !mp_tree->has_key(m_id))
		return {};
	return toView(mp_tree->key(m_id));
}

auto YamlNode::getLine() const noexcept -> size_t {
	if (!isDefined())
		return 0;
	const auto arena = mp_tree->arena();
	auto id = m_id;
	while (id != ryml::NONE && !mp_tree->has_key(id) && !mp_tree->has_val(id)) id = mp_tree->first_child(id);
	if (id == ryml::NONE)
		return 0;
	const auto text = mp_tree->has_key(id) ? mp_tree->key(id) : mp_tree->val(id);
	if (text.str == nullptr || text.str < arena.begin() || text.str > arena.end())
		return 0;
	return 1 + static_cast<size_t>(std::count(arena.begin(), text.str, '\n'));
}

// NOLINTNEXTLINE(misc-no-recursion): one call per nesting level of the document, bounded by its depth.
auto YamlNode::isSameAs(const YamlNode& iOther) const noexcept -> bool {
	if (isDefined() != iOther.isDefined())
		return false;
	if (!isDefined())
		return true;
	if (isMap() != iOther.isMap() || isSequence() != iOther.isSequence())
		return false;
	if (!mp_tree->is_container(m_id)) {
		if (isNull() || iOther.isNull())
			return isNull() == iOther.isNull();
		return toView(mp_tree->val(m_id)) == toView(iOther.mp_tree->val(iOther.m_id));
	}
	auto mine = mp_tree->first_child(m_id);
	auto theirs = iOther.mp_tree->first_child(iOther.m_id);
	for (; mine != ryml::NONE && theirs != ryml::NONE;
		 mine = mp_tree->next_sibling(mine), theirs = iOther.mp_tree->next_sibling(theirs)) {
		const YamlNode left{mp_tree, mine};
		const YamlNode right{iOther.mp_tree, theirs};
		if (left.getKey() != right.getKey() || !left.isSameAs(right))
			return false;
	}
	return mine == ryml::NONE && theirs == ryml::NONE;
}

auto YamlNode::emit() const -> std::string {
	if (!isDefined() || isNull())
		return {};
	if (!mp_tree->is_container(m_id))
		return std::string{getScalar()};
	ryml::Tree copy{throwingCallbacks()};
	const auto root = copy.root_id();
	if (mp_tree->is_map(m_id))
		copy.to_map(root);
	else
		copy.to_seq(root);
	copy.duplicate_children(mp_tree, m_id, root, ryml::NONE);
	return ryml::emitrs_yaml<std::string>(copy);
}

auto YamlNode::begin() const noexcept -> Iterator {
	if (!isDefined() || !mp_tree->is_container(m_id))
		return end();
	return {mp_tree, mp_tree->first_child(m_id)};
}

YamlDocument::YamlDocument(const std::string_view iYaml, const std::string_view iSourceName)
	: m_tree{throwingCallbacks()} {
	if (isBinaryYaml(iYaml)) {
		loadBinary(iYaml, iSourceName);
		return;
	}
	const ryml::csubstr text{iYaml.data(), iYaml.size()};
	ryml::EventHandlerTree handler{throwingCallbacks()};
	ryml::Parser parser{&handler};
	ryml::parse_in_arena(&parser, ryml::csubstr{iSourceName.data(), iSourceName.size()}, text, &m_tree);
	if (iYaml.find('*') != std::string_view::npos)
		m_tree.resolve();
	m_root = m_tree.root_id();
	if (m_tree.is_stream(m_root))
		m_root = m_tree.first_child(m_root);
}

auto isBinaryYaml(const std::string_view iText) noexcept -> bool { return iText.starts_with(g_BinaryYamlMagic); }

auto YamlDocument::toBinary() const -> std::string {
	std::unordered_map<std::string_view, uint32_t> indices;
	std::vector<std::string_view> strings;
	const auto intern = [&indices, &strings](const ryml::csubstr iText) -> uint32_t {
		const auto view = toView(iText);
		const auto [found, inserted] = indices.try_emplace(view, static_cast<uint32_t>(strings.size()));
		if (inserted)
			strings.push_back(view);
		return found->second;
	};
	std::string nodes;
	uint32_t nodeCount = 0;
	std::vector<ryml::id_type> pending;
	if (m_root != ryml::NONE)
		pending.push_back(m_root);
	while (!pending.empty()) {
		const auto id = pending.back();
		pending.pop_back();
		uint8_t flags = 0;
		const bool hasKey = m_tree.has_key(id);
		const bool hasVal = m_tree.has_val(id);
		const bool null = hasVal && m_tree.val_is_null(id);
		if (hasKey)
			flags |= static_cast<uint8_t>(BinaryNode::Key);
		if (hasVal)
			flags |= static_cast<uint8_t>(BinaryNode::Val);
		if (m_tree.is_map(id))
			flags |= static_cast<uint8_t>(BinaryNode::Map);
		if (m_tree.is_seq(id))
			flags |= static_cast<uint8_t>(BinaryNode::Seq);
		if (null)
			flags |= static_cast<uint8_t>(BinaryNode::Null);
		if (hasVal && m_tree.is_val_quoted(id))
			flags |= static_cast<uint8_t>(BinaryNode::Quoted);
		nodes.push_back(static_cast<char>(flags));
		if (hasKey)
			appendU32(nodes, intern(m_tree.key(id)));
		if (hasVal && !null)
			appendU32(nodes, intern(m_tree.val(id)));
		if (m_tree.is_container(id)) {
			appendU32(nodes, static_cast<uint32_t>(m_tree.num_children(id)));
			const auto first = pending.size();
			for (auto child = m_tree.first_child(id); child != ryml::NONE; child = m_tree.next_sibling(child))
				pending.push_back(child);
			std::reverse(pending.begin() + static_cast<std::ptrdiff_t>(first), pending.end());
		}
		++nodeCount;
	}
	std::string blob;
	std::string out{g_BinaryYamlMagic};
	appendU32(out, g_binaryVersion);
	appendU32(out, nodeCount);
	appendU32(out, static_cast<uint32_t>(strings.size()));
	for (const auto& text: strings) {
		appendU32(out, static_cast<uint32_t>(blob.size()));
		appendU32(out, static_cast<uint32_t>(text.size()));
		blob.append(text);
	}
	appendU32(out, static_cast<uint32_t>(blob.size()));
	out += blob;
	out += nodes;
	return out;
}

void YamlDocument::loadBinary(const std::string_view iBytes, const std::string_view iSourceName) {
	BinaryReader reader{iBytes, iSourceName};
	std::ignore = reader.bytes(g_BinaryYamlMagic.size());
	if (reader.u32() != g_binaryVersion)
		reader.fail("of an unknown version");
	const auto nodeCount = reader.u32();
	const auto stringCount = reader.u32();
	// Each string costs 8 bytes of table and each node at least one byte: larger counts are lies.
	if (stringCount > reader.remaining() / 8 || nodeCount > reader.remaining())
		reader.fail("inconsistent");
	std::vector<std::pair<uint32_t, uint32_t>> ranges(stringCount);
	for (auto& [offset, length]: ranges) {
		offset = reader.u32();
		length = reader.u32();
	}
	const auto blobSize = reader.u32();
	const auto blob = reader.bytes(blobSize);
	for (const auto& [offset, length]: ranges)
		if (offset > blobSize || length > blobSize - offset)
			reader.fail("inconsistent");
	m_tree.reserve(static_cast<ryml::id_type>(nodeCount) + 1);
	m_tree.reserve_arena(blobSize);
	const auto arena = m_tree.copy_to_arena(ryml::csubstr{blob.data(), blob.size()});
	const auto text = [&reader, &ranges, &arena](const uint32_t iIndex) -> ryml::csubstr {
		if (iIndex >= ranges.size())
			reader.fail("inconsistent");
		return arena.sub(ranges[iIndex].first, ranges[iIndex].second);
	};

	// Containers being filled, with the number of children still to read.
	std::vector<std::pair<ryml::id_type, uint32_t>> parents;
	for (uint32_t index = 0; index < nodeCount; ++index) {
		while (!parents.empty() && parents.back().second == 0) parents.pop_back();
		if (index > 0 && parents.empty())
			reader.fail("inconsistent");
		const auto flags = reader.u8();
		const ryml::id_type id = parents.empty() ? m_tree.root_id() : m_tree.append_child(parents.back().first);
		if (!parents.empty())
			--parents.back().second;
		const ryml::csubstr key = has(flags, BinaryNode::Key) ? text(reader.u32()) : ryml::csubstr{};
		const bool hasKey = has(flags, BinaryNode::Key);
		if (has(flags, BinaryNode::Map) || has(flags, BinaryNode::Seq)) {
			const bool isMap = has(flags, BinaryNode::Map);
			if (hasKey)
				isMap ? m_tree.to_map(id, key) : m_tree.to_seq(id, key);
			else
				isMap ? m_tree.to_map(id) : m_tree.to_seq(id);
			const auto children = reader.u32();
			if (children > nodeCount)
				reader.fail("inconsistent");
			parents.emplace_back(id, children);
			continue;
		}
		const ryml::csubstr val =
				has(flags, BinaryNode::Null) || !has(flags, BinaryNode::Val) ? ryml::csubstr{} : text(reader.u32());
		const ryml::type_bits style = has(flags, BinaryNode::Quoted) ? ryml::VAL_SQUO : ryml::type_bits{0};
		if (hasKey)
			m_tree.to_keyval(id, key, val, style);
		else
			m_tree.to_val(id, val, style);
	}
	m_root = nodeCount == 0 ? ryml::NONE : m_tree.root_id();
}

}// namespace owl::core
