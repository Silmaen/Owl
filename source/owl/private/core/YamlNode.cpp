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

namespace owl::core {

namespace {

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

}// namespace owl::core
