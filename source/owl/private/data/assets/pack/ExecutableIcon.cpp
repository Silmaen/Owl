/**
 * @file ExecutableIcon.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/pack/ExecutableIcon.h"

#include "core/Macros.h"
#include "platform/AtomicFile.h"

#include <stb_image.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Weverything")
OWL_DIAG_DISABLE_GCC("-Wall")
OWL_DIAG_DISABLE_GCC("-Wextra")
OWL_DIAG_DISABLE_GCC("-Wconversion")
OWL_DIAG_DISABLE_GCC("-Wsign-conversion")
OWL_DIAG_DISABLE_GCC("-Wmissing-field-initializers")
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::data::assets::pack {

namespace {

// PE layout constants (Microsoft PE/COFF specification).
constexpr size_t g_dosHeaderSize = 64;
constexpr size_t g_lfanewOffset = 0x3c;
constexpr size_t g_coffHeaderSize = 20;
constexpr size_t g_sectionHeaderSize = 40;
constexpr uint16_t g_pe32Magic = 0x10b;
constexpr uint16_t g_pe32PlusMagic = 0x20b;
constexpr size_t g_resourceDirectory = 2;
constexpr size_t g_securityDirectory = 4;
constexpr uint32_t g_highBit = 0x80000000U;
constexpr uint32_t g_rsrcCharacteristics = 0x40000040U;// initialized data, readable
constexpr size_t g_maxResourceDepth = 3;
constexpr uint16_t g_defaultLanguage = 0x0409;
constexpr size_t g_maxResourceEntries = 65536;
constexpr std::array<uint8_t, 8> g_rsrcName = {'.', 'r', 's', 'r', 'c', 0, 0, 0};
constexpr std::array<uint8_t, 8> g_unusedRsrcName = {'.', 'r', 's', 'r', 'c', 'o', 0, 0};
constexpr size_t g_maxImageSide = 16384;

auto readU16(const std::span<const uint8_t> iData, const size_t iOffset) -> std::optional<uint16_t> {
	if (iOffset > iData.size() || iData.size() - iOffset < 2)
		return std::nullopt;
	return static_cast<uint16_t>(iData[iOffset] | (iData[iOffset + 1] << 8U));
}

auto readU32(const std::span<const uint8_t> iData, const size_t iOffset) -> std::optional<uint32_t> {
	if (iOffset > iData.size() || iData.size() - iOffset < 4)
		return std::nullopt;
	return static_cast<uint32_t>(iData[iOffset]) | (static_cast<uint32_t>(iData[iOffset + 1]) << 8U) |
		   (static_cast<uint32_t>(iData[iOffset + 2]) << 16U) | (static_cast<uint32_t>(iData[iOffset + 3]) << 24U);
}

void putU16(std::vector<uint8_t>& ioData, const size_t iOffset, const uint16_t iValue) {
	ioData[iOffset] = static_cast<uint8_t>(iValue & 0xffU);
	ioData[iOffset + 1] = static_cast<uint8_t>(iValue >> 8U);
}

void putU32(std::vector<uint8_t>& ioData, const size_t iOffset, const uint32_t iValue) {
	for (size_t i = 0; i < 4; ++i) ioData[iOffset + i] = static_cast<uint8_t>((iValue >> (8U * i)) & 0xffU);
}

void appendU16(std::vector<uint8_t>& ioData, const uint16_t iValue) {
	ioData.push_back(static_cast<uint8_t>(iValue & 0xffU));
	ioData.push_back(static_cast<uint8_t>(iValue >> 8U));
}

void appendU32(std::vector<uint8_t>& ioData, const uint32_t iValue) {
	for (uint32_t i = 0; i < 4; ++i) ioData.push_back(static_cast<uint8_t>((iValue >> (8U * i)) & 0xffU));
}

constexpr auto alignUp(const size_t iValue, const size_t iAlignment) -> size_t {
	return iAlignment == 0 ? iValue : (iValue + iAlignment - 1) / iAlignment * iAlignment;
}

struct Section {
	size_t headerOffset = 0;
	uint32_t virtualSize = 0;
	uint32_t virtualAddress = 0;
	uint32_t rawSize = 0;
	uint32_t rawPointer = 0;
};

struct PeLayout {
	size_t optionalHeader = 0;
	size_t dataDirectories = 0;
	uint32_t dataDirectoryCount = 0;
	size_t sectionTable = 0;
	uint32_t sectionAlignment = 0;
	uint32_t fileAlignment = 0;
	uint32_t sizeOfHeaders = 0;
	std::vector<Section> sections;
	uint32_t resourceRva = 0;
	uint32_t resourceSize = 0;
	uint32_t securitySize = 0;

	[[nodiscard]] auto rvaToOffset(const uint32_t iRva, const uint32_t iSize) const -> std::optional<size_t> {
		for (const auto& section: sections) {
			if (iRva < section.virtualAddress)
				continue;
			const uint64_t delta = iRva - section.virtualAddress;
			if (delta + iSize <= section.rawSize)
				return section.rawPointer + static_cast<size_t>(delta);
		}
		return std::nullopt;
	}

	[[nodiscard]] auto resourceSection() const -> std::optional<size_t> {
		for (size_t i = 0; i < sections.size(); ++i) {
			const auto& section = sections[i];
			if (resourceRva >= section.virtualAddress &&
				resourceRva < section.virtualAddress + std::max(section.virtualSize, section.rawSize))
				return i;
		}
		return std::nullopt;
	}
};

auto parseSections(const std::span<const uint8_t> iData, PeLayout& ioLayout, const uint16_t iCount)
		-> expected<void, ExecutableIconError> {
	for (uint16_t i = 0; i < iCount; ++i) {
		const size_t offset = ioLayout.sectionTable + i * g_sectionHeaderSize;
		const auto virtualSize = readU32(iData, offset + 8);
		const auto virtualAddress = readU32(iData, offset + 12);
		const auto rawSize = readU32(iData, offset + 16);
		const auto rawPointer = readU32(iData, offset + 20);
		if (!virtualSize || !virtualAddress || !rawSize || !rawPointer ||
			static_cast<uint64_t>(*rawPointer) + *rawSize > iData.size()) {
			OWL_CORE_WARN("ExecutableIcon: Section {} lies outside the file.", i)
			return unexpected{ExecutableIconError::Corrupted};
		}
		ioLayout.sections.push_back({.headerOffset = offset,
									 .virtualSize = *virtualSize,
									 .virtualAddress = *virtualAddress,
									 .rawSize = *rawSize,
									 .rawPointer = *rawPointer});
	}
	return {};
}

auto parsePe(const std::span<const uint8_t> iData) -> expected<PeLayout, ExecutableIconError> {
	if (iData.size() < g_dosHeaderSize || iData[0] != 'M' || iData[1] != 'Z') {
		OWL_CORE_WARN("ExecutableIcon: No MZ signature, not a Windows executable.")
		return unexpected{ExecutableIconError::NotExecutable};
	}
	const auto peOffset = readU32(iData, g_lfanewOffset).value_or(0);
	if (const auto signature = readU32(iData, peOffset); !signature || *signature != 0x00004550U) {
		OWL_CORE_WARN("ExecutableIcon: No PE signature, not a Windows executable.")
		return unexpected{ExecutableIconError::NotExecutable};
	}
	const size_t coff = static_cast<size_t>(peOffset) + 4;
	const auto sectionCount = readU16(iData, coff + 2);
	const auto optionalSize = readU16(iData, coff + 16);
	PeLayout layout;
	layout.optionalHeader = coff + g_coffHeaderSize;
	const auto magic = readU16(iData, layout.optionalHeader);
	if (!sectionCount || !optionalSize || !magic || (*magic != g_pe32Magic && *magic != g_pe32PlusMagic)) {
		OWL_CORE_WARN("ExecutableIcon: Unknown optional header, not a Windows executable.")
		return unexpected{ExecutableIconError::NotExecutable};
	}
	const size_t countOffset = layout.optionalHeader + (*magic == g_pe32Magic ? 92 : 108);
	layout.dataDirectories = countOffset + 4;
	layout.sectionTable = layout.optionalHeader + *optionalSize;
	const auto alignment = readU32(iData, layout.optionalHeader + 32);
	const auto fileAlignment = readU32(iData, layout.optionalHeader + 36);
	const auto headers = readU32(iData, layout.optionalHeader + 60);
	const auto count = readU32(iData, countOffset);
	if (!alignment || !fileAlignment || !headers || !count || *count <= g_securityDirectory ||
		layout.dataDirectories + 8 * static_cast<size_t>(*count) > layout.sectionTable ||
		layout.sectionTable + *sectionCount * g_sectionHeaderSize > iData.size() || *alignment == 0 ||
		*fileAlignment == 0) {
		OWL_CORE_WARN("ExecutableIcon: Corrupted optional header or section table.")
		return unexpected{ExecutableIconError::Corrupted};
	}
	layout.sectionAlignment = *alignment;
	layout.fileAlignment = *fileAlignment;
	layout.sizeOfHeaders = *headers;
	layout.dataDirectoryCount = *count;
	layout.resourceRva = readU32(iData, layout.dataDirectories + 8 * g_resourceDirectory).value_or(0);
	layout.resourceSize = readU32(iData, layout.dataDirectories + 8 * g_resourceDirectory + 4).value_or(0);
	layout.securitySize = readU32(iData, layout.dataDirectories + 8 * g_securityDirectory + 4).value_or(0);
	if (auto sections = parseSections(iData, layout, *sectionCount); !sections)
		return unexpected{sections.error()};
	return layout;
}

// Reads the resource tree; offsets of directories and strings are relative to the start of the tree.
class ResourceReader {
public:
	ResourceReader(const std::span<const uint8_t> iFile, const std::span<const uint8_t> iTree, const PeLayout& iLayout)
		: m_file{iFile}, m_tree{iTree}, m_layout{iLayout} {}

	auto read() -> expected<std::vector<ExecutableResource>, ExecutableIconError> {
		std::array<ResourceId, g_maxResourceDepth> path{};
		if (!readDirectory(0, 0, path))
			return unexpected{ExecutableIconError::Corrupted};
		return std::move(m_resources);
	}

private:
	[[nodiscard]] auto readName(const uint32_t iOffset) const -> std::optional<std::u16string> {
		const auto length = readU16(m_tree, iOffset);
		if (!length || iOffset + 2 + 2 * static_cast<size_t>(*length) > m_tree.size())
			return std::nullopt;
		std::u16string name;
		for (size_t i = 0; i < *length; ++i)
			name.push_back(static_cast<char16_t>(*readU16(m_tree, iOffset + 2 + 2 * i)));
		return name;
	}

	auto readLeaf(const uint32_t iOffset, const std::array<ResourceId, g_maxResourceDepth>& iPath) -> bool {
		const auto rva = readU32(m_tree, iOffset);
		const auto size = readU32(m_tree, iOffset + 4);
		const auto codePage = readU32(m_tree, iOffset + 8);
		if (!rva || !size || !codePage)
			return false;
		const auto offset = m_layout.rvaToOffset(*rva, *size);
		if (!offset)
			return false;
		const auto bytes = m_file.subspan(*offset, *size);
		m_resources.push_back({.type = iPath[0],
							   .name = iPath[1],
							   .language = iPath[2].id,
							   .codePage = *codePage,
							   .data = std::vector<uint8_t>(bytes.begin(), bytes.end())});
		return true;
	}

	// NOLINTNEXTLINE(misc-no-recursion): one call per level of the tree, three levels at most.
	auto readDirectory(const uint32_t iOffset, const size_t iDepth, std::array<ResourceId, g_maxResourceDepth>& ioPath)
			-> bool {
		const auto named = readU16(m_tree, iOffset + 12);
		const auto ids = readU16(m_tree, iOffset + 14);
		if (!named || !ids || iOffset + 16 + 8 * (static_cast<size_t>(*named) + *ids) > m_tree.size())
			return false;
		m_entries += static_cast<size_t>(*named) + *ids;
		if (m_entries > g_maxResourceEntries)
			return false;
		for (size_t i = 0; i < static_cast<size_t>(*named) + *ids; ++i) {
			const size_t entry = iOffset + 16 + 8 * i;
			const uint32_t nameField = *readU32(m_tree, entry);
			const uint32_t target = *readU32(m_tree, entry + 4);
			ResourceId id;
			if ((nameField & g_highBit) != 0) {
				auto name = readName(nameField & ~g_highBit);
				if (!name)
					return false;
				id.name = std::move(*name);
			} else {
				id.id = static_cast<uint16_t>(nameField & 0xffffU);
			}
			ioPath[iDepth] = std::move(id);
			const bool isDirectory = (target & g_highBit) != 0;
			const uint32_t childOffset = target & ~g_highBit;
			if (childOffset <= iOffset || isDirectory != (iDepth + 1 < g_maxResourceDepth))
				return false;
			if (isDirectory ? !readDirectory(childOffset, iDepth + 1, ioPath) : !readLeaf(childOffset, ioPath))
				return false;
		}
		return true;
	}

	std::span<const uint8_t> m_file;
	std::span<const uint8_t> m_tree;
	const PeLayout& m_layout;
	std::vector<ExecutableResource> m_resources;
	size_t m_entries = 0;
};

auto readResources(const std::span<const uint8_t> iData, const PeLayout& iLayout)
		-> expected<std::vector<ExecutableResource>, ExecutableIconError> {
	if (iLayout.resourceRva == 0)
		return std::vector<ExecutableResource>{};
	const auto section = iLayout.resourceSection();
	if (!section) {
		OWL_CORE_WARN("ExecutableIcon: The resource directory lies in no section.")
		return unexpected{ExecutableIconError::Corrupted};
	}
	const auto& rsrc = iLayout.sections[*section];
	const size_t start = rsrc.rawPointer + (iLayout.resourceRva - rsrc.virtualAddress);
	if (start >= rsrc.rawPointer + rsrc.rawSize) {
		OWL_CORE_WARN("ExecutableIcon: The resource directory lies outside its section data.")
		return unexpected{ExecutableIconError::Corrupted};
	}
	const auto tree = iData.subspan(start, rsrc.rawPointer + rsrc.rawSize - start);
	auto resources = ResourceReader{iData, tree, iLayout}.read();
	if (!resources)
		OWL_CORE_WARN("ExecutableIcon: Corrupted resource tree.")
	return resources;
}

// Named entries come first, sorted by name, then numeric ones in ascending order.
auto lessId(const ResourceId& iLeft, const ResourceId& iRight) -> bool {
	if (iLeft.name.empty() != iRight.name.empty())
		return !iLeft.name.empty();
	if (!iLeft.name.empty())
		return iLeft.name < iRight.name;
	return iLeft.id < iRight.id;
}

struct NameNode {
	ResourceId id;
	std::vector<size_t> resources;
	size_t offset = 0;
};

struct TypeNode {
	ResourceId id;
	std::vector<NameNode> names;
	size_t offset = 0;
};

auto buildTree(const std::vector<ExecutableResource>& iResources) -> std::vector<TypeNode> {
	std::vector<size_t> order(iResources.size());
	for (size_t i = 0; i < order.size(); ++i) order[i] = i;
	std::ranges::stable_sort(order, [&iResources](const size_t iLeft, const size_t iRight) -> bool {
		const auto& left = iResources[iLeft];
		const auto& right = iResources[iRight];
		if (!(left.type == right.type))
			return lessId(left.type, right.type);
		if (!(left.name == right.name))
			return lessId(left.name, right.name);
		return left.language < right.language;
	});
	std::vector<TypeNode> types;
	for (const size_t index: order) {
		const auto& resource = iResources[index];
		if (types.empty() || !(types.back().id == resource.type))
			types.push_back({.id = resource.type, .names = {}, .offset = 0});
		auto& names = types.back().names;
		if (names.empty() || !(names.back().id == resource.name))
			names.push_back({.id = resource.name, .resources = {}, .offset = 0});
		names.back().resources.push_back(index);
	}
	return types;
}

void writeDirectoryHeader(std::vector<uint8_t>& ioTree, const size_t iOffset, const uint16_t iNamed,
						  const uint16_t iIds) {
	putU16(ioTree, iOffset + 12, iNamed);
	putU16(ioTree, iOffset + 14, iIds);
}

template<typename Node>
auto countNamed(const std::vector<Node>& iNodes) -> uint16_t {
	return static_cast<uint16_t>(
			std::ranges::count_if(iNodes, [](const Node& iNode) -> bool { return !iNode.id.name.empty(); }));
}

// Serialises the resource tree for a section placed at `iRva`.
auto buildResourceSection(const std::vector<ExecutableResource>& iResources, const uint32_t iRva)
		-> std::vector<uint8_t> {
	auto types = buildTree(iResources);
	size_t offset = 16 + 8 * types.size();
	for (auto& type: types) {
		type.offset = offset;
		offset += 16 + 8 * type.names.size();
	}
	for (auto& type: types)
		for (auto& name: type.names) {
			name.offset = offset;
			offset += 16 + 8 * name.resources.size();
		}
	std::map<std::u16string, size_t> strings;
	auto addString = [&strings, &offset](const ResourceId& iId) -> void {
		if (!iId.name.empty() && !strings.contains(iId.name)) {
			strings.emplace(iId.name, offset);
			offset += 2 + 2 * iId.name.size();
		}
	};
	for (const auto& type: types) {
		addString(type.id);
		for (const auto& name: type.names) addString(name.id);
	}
	offset = alignUp(offset, 8);
	const size_t dataEntries = offset;
	offset += 16 * iResources.size();
	std::vector<size_t> blobOffsets(iResources.size());
	for (const auto& type: types)
		for (const auto& name: type.names)
			for (const size_t index: name.resources) {
				offset = alignUp(offset, 8);
				blobOffsets[index] = offset;
				offset += iResources[index].data.size();
			}
	std::vector<uint8_t> tree(alignUp(offset, 8), 0);

	auto entryName = [&strings](const ResourceId& iId) -> uint32_t {
		return iId.name.empty() ? iId.id : static_cast<uint32_t>(strings.at(iId.name)) | g_highBit;
	};
	writeDirectoryHeader(tree, 0, countNamed(types), static_cast<uint16_t>(types.size() - countNamed(types)));
	size_t dataEntry = dataEntries;
	for (size_t t = 0; t < types.size(); ++t) {
		const auto& type = types[t];
		putU32(tree, 16 + 8 * t, entryName(type.id));
		putU32(tree, 16 + 8 * t + 4, static_cast<uint32_t>(type.offset) | g_highBit);
		writeDirectoryHeader(tree, type.offset, countNamed(type.names),
							 static_cast<uint16_t>(type.names.size() - countNamed(type.names)));
		for (size_t n = 0; n < type.names.size(); ++n) {
			const auto& name = type.names[n];
			putU32(tree, type.offset + 16 + 8 * n, entryName(name.id));
			putU32(tree, type.offset + 16 + 8 * n + 4, static_cast<uint32_t>(name.offset) | g_highBit);
			writeDirectoryHeader(tree, name.offset, 0, static_cast<uint16_t>(name.resources.size()));
			for (size_t l = 0; l < name.resources.size(); ++l) {
				const auto& resource = iResources[name.resources[l]];
				putU32(tree, name.offset + 16 + 8 * l, resource.language);
				putU32(tree, name.offset + 16 + 8 * l + 4, static_cast<uint32_t>(dataEntry));
				putU32(tree, dataEntry, iRva + static_cast<uint32_t>(blobOffsets[name.resources[l]]));
				putU32(tree, dataEntry + 4, static_cast<uint32_t>(resource.data.size()));
				putU32(tree, dataEntry + 8, resource.codePage);
				std::ranges::copy(resource.data,
								  tree.begin() + static_cast<std::ptrdiff_t>(blobOffsets[name.resources[l]]));
				dataEntry += 16;
			}
		}
	}
	for (const auto& [name, stringOffset]: strings) {
		putU16(tree, stringOffset, static_cast<uint16_t>(name.size()));
		for (size_t i = 0; i < name.size(); ++i) putU16(tree, stringOffset + 2 + 2 * i, static_cast<uint16_t>(name[i]));
	}
	return tree;
}

auto computeChecksum(const std::vector<uint8_t>& iData, const size_t iChecksumOffset) -> uint32_t {
	uint64_t sum = 0;
	for (size_t i = 0; i < iData.size(); i += 2) {
		if (i == iChecksumOffset || i == iChecksumOffset + 2)
			continue;
		const uint32_t word = iData[i] | (i + 1 < iData.size() ? static_cast<uint32_t>(iData[i + 1]) << 8U : 0U);
		sum += word;
		sum = (sum & 0xffffU) + (sum >> 16U);
	}
	sum = (sum & 0xffffU) + (sum >> 16U);
	return static_cast<uint32_t>(sum + iData.size());
}

// The resource section can be rewritten in place: it holds only the tree and ends the image and the file.
auto isRewritableInPlace(const PeLayout& iLayout, const size_t iFileSize) -> std::optional<size_t> {
	if (iLayout.resourceRva == 0)
		return std::nullopt;
	const auto index = iLayout.resourceSection();
	if (!index)
		return std::nullopt;
	const auto& rsrc = iLayout.sections[*index];
	const bool lastInImage = std::ranges::all_of(iLayout.sections, [&rsrc](const Section& iSection) -> bool {
		return iSection.virtualAddress <= rsrc.virtualAddress;
	});
	if (!lastInImage || iLayout.resourceRva != rsrc.virtualAddress ||
		static_cast<size_t>(rsrc.rawPointer) + rsrc.rawSize != iFileSize || rsrc.rawPointer == 0)
		return std::nullopt;
	return index;
}

void setSection(std::vector<uint8_t>& ioData, const size_t iHeader, const Section& iSection) {
	putU32(ioData, iHeader + 8, iSection.virtualSize);
	putU32(ioData, iHeader + 12, iSection.virtualAddress);
	putU32(ioData, iHeader + 16, iSection.rawSize);
	putU32(ioData, iHeader + 20, iSection.rawPointer);
}

auto appendSection(std::vector<uint8_t>& ioData, const PeLayout& iLayout) -> expected<Section, ExecutableIconError> {
	const size_t header = iLayout.sectionTable + iLayout.sections.size() * g_sectionHeaderSize;
	size_t firstRaw = iLayout.sizeOfHeaders;
	uint64_t imageEnd = iLayout.sizeOfHeaders;
	for (const auto& section: iLayout.sections) {
		if (section.rawSize > 0)
			firstRaw = std::min<size_t>(firstRaw, section.rawPointer);
		imageEnd = std::max<uint64_t>(imageEnd, static_cast<uint64_t>(section.virtualAddress) +
														std::max(section.virtualSize, section.rawSize));
	}
	if (header + g_sectionHeaderSize > firstRaw || header + g_sectionHeaderSize > ioData.size()) {
		OWL_CORE_WARN("ExecutableIcon: No room in the headers for a new resource section.")
		return unexpected{ExecutableIconError::NoHeaderRoom};
	}
	if (const auto rsrc = iLayout.resourceSection(); rsrc) {
		const auto name = ioData.begin() + static_cast<std::ptrdiff_t>(iLayout.sections[*rsrc].headerOffset);
		if (std::equal(name, name + 8, g_rsrcName.begin()))
			std::ranges::copy(g_unusedRsrcName, name);
	}
	std::fill_n(ioData.begin() + static_cast<std::ptrdiff_t>(header), g_sectionHeaderSize, uint8_t{0});
	std::ranges::copy(g_rsrcName, ioData.begin() + static_cast<std::ptrdiff_t>(header));
	putU32(ioData, header + 36, g_rsrcCharacteristics);
	const auto countOffset = iLayout.optionalHeader - g_coffHeaderSize + 2;
	putU16(ioData, countOffset, static_cast<uint16_t>(iLayout.sections.size() + 1));
	ioData.resize(alignUp(ioData.size(), iLayout.fileAlignment), 0);
	return Section{.headerOffset = header,
				   .virtualSize = 0,
				   .virtualAddress = static_cast<uint32_t>(alignUp(imageEnd, iLayout.sectionAlignment)),
				   .rawSize = 0,
				   .rawPointer = static_cast<uint32_t>(ioData.size())};
}

struct Rgba {
	std::vector<uint8_t> pixels;
	size_t width = 0;
	size_t height = 0;
};

auto decodeImage(const std::span<const uint8_t> iImageFile) -> expected<Rgba, ExecutableIconError> {
	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_uc* raw = stbi_load_from_memory(iImageFile.data(), static_cast<int>(iImageFile.size()), &width, &height,
										 &channels, 4);
	if (raw == nullptr || width <= 0 || height <= 0 || std::cmp_greater(width, g_maxImageSide) ||
		std::cmp_greater(height, g_maxImageSide)) {
		OWL_CORE_WARN("ExecutableIcon: Cannot decode the icon image: {}.",
					  raw == nullptr ? stbi_failure_reason() : "unsupported size")
		stbi_image_free(raw);
		return unexpected{ExecutableIconError::ImageDecode};
	}
	Rgba image{.pixels = {}, .width = static_cast<size_t>(width), .height = static_cast<size_t>(height)};
	image.pixels.assign(raw, raw + image.width * image.height * 4);
	stbi_image_free(raw);
	return image;
}

// Fits the image in a transparent square, averaging the source pixels each target pixel covers (premultiplied).
auto fitSquare(const Rgba& iImage, const size_t iSize) -> std::vector<uint8_t> {
	std::vector<uint8_t> out(iSize * iSize * 4, 0);
	const double scale = static_cast<double>(iSize) / static_cast<double>(std::max(iImage.width, iImage.height));
	const auto contentWidth =
			std::max<size_t>(1, static_cast<size_t>(std::lround(static_cast<double>(iImage.width) * scale)));
	const auto contentHeight =
			std::max<size_t>(1, static_cast<size_t>(std::lround(static_cast<double>(iImage.height) * scale)));
	const size_t left = (iSize - std::min(contentWidth, iSize)) / 2;
	const size_t top = (iSize - std::min(contentHeight, iSize)) / 2;
	const auto samples = std::clamp<size_t>(static_cast<size_t>(std::ceil(1.0 / scale)), 1, 16);
	for (size_t y = 0; y < std::min(contentHeight, iSize); ++y) {
		for (size_t x = 0; x < std::min(contentWidth, iSize); ++x) {
			std::array<double, 4> sum{};
			for (size_t sy = 0; sy < samples; ++sy) {
				for (size_t sx = 0; sx < samples; ++sx) {
					const double fx =
							(static_cast<double>(x) + (static_cast<double>(sx) + 0.5) / static_cast<double>(samples)) /
							scale;
					const double fy =
							(static_cast<double>(y) + (static_cast<double>(sy) + 0.5) / static_cast<double>(samples)) /
							scale;
					const size_t px = std::min(iImage.width - 1, static_cast<size_t>(fx));
					const size_t py = std::min(iImage.height - 1, static_cast<size_t>(fy));
					const auto* pixel = &iImage.pixels[(py * iImage.width + px) * 4];
					const double alpha = pixel[3] / 255.0;
					for (size_t c = 0; c < 3; ++c) sum[c] += pixel[c] * alpha;
					sum[3] += alpha;
				}
			}
			auto* target = &out[((top + y) * iSize + left + x) * 4];
			const auto count = static_cast<double>(samples * samples);
			if (sum[3] > 0.0)
				for (size_t c = 0; c < 3; ++c)
					target[c] = static_cast<uint8_t>(std::lround(std::clamp(sum[c] / sum[3], 0.0, 255.0)));
			target[3] = static_cast<uint8_t>(std::lround(std::clamp(sum[3] / count * 255.0, 0.0, 255.0)));
		}
	}
	return out;
}

// A 32-bit bottom-up BGRA bitmap followed by its 1-bit mask, transparent where alpha is 0.
auto encodeBitmap(const std::vector<uint8_t>& iPixels, const size_t iSize) -> std::vector<uint8_t> {
	const size_t maskStride = alignUp((iSize + 7) / 8, 4);
	std::vector<uint8_t> out;
	out.reserve(40 + iSize * iSize * 4 + maskStride * iSize);
	appendU32(out, 40);
	appendU32(out, static_cast<uint32_t>(iSize));
	appendU32(out, static_cast<uint32_t>(iSize * 2));
	appendU16(out, 1);
	appendU16(out, 32);
	appendU32(out, 0);
	appendU32(out, static_cast<uint32_t>(iSize * iSize * 4 + maskStride * iSize));
	for (size_t i = 0; i < 4; ++i) appendU32(out, 0);
	for (const size_t y: std::views::iota(size_t{0}, iSize) | std::views::reverse)
		for (size_t x = 0; x < iSize; ++x) {
			const auto* pixel = &iPixels[(y * iSize + x) * 4];
			out.insert(out.end(), {pixel[2], pixel[1], pixel[0], pixel[3]});
		}
	for (const size_t y: std::views::iota(size_t{0}, iSize) | std::views::reverse) {
		std::vector<uint8_t> row(maskStride, 0);
		for (size_t x = 0; x < iSize; ++x)
			if (iPixels[(y * iSize + x) * 4 + 3] == 0)
				row[x / 8] |= static_cast<uint8_t>(0x80U >> (x % 8));
		out.insert(out.end(), row.begin(), row.end());
	}
	return out;
}

void appendBytes(void* iContext, void* iData, const int iSize) {
	auto* out = static_cast<std::vector<uint8_t>*>(iContext);
	const auto* bytes = static_cast<const uint8_t*>(iData);
	out->insert(out->end(), bytes, bytes + iSize);
}

auto encodePng(const std::vector<uint8_t>& iPixels, const size_t iSize) -> std::optional<std::vector<uint8_t>> {
	std::vector<uint8_t> out;
	const int side = static_cast<int>(iSize);
	if (stbi_write_png_to_func(appendBytes, &out, side, side, 4, iPixels.data(), side * 4) == 0 || out.empty())
		return std::nullopt;
	return out;
}

// Sizes in the group entry: 0 stands for 256 pixels.
auto groupEntrySide(const uint16_t iSize) -> uint8_t { return iSize >= 256 ? 0 : static_cast<uint8_t>(iSize); }

auto buildGroup(const std::vector<std::vector<uint8_t>>& iImages, const std::vector<uint16_t>& iIds)
		-> std::vector<uint8_t> {
	std::vector<uint8_t> group;
	appendU16(group, 0);
	appendU16(group, 1);
	appendU16(group, static_cast<uint16_t>(iImages.size()));
	for (size_t i = 0; i < iImages.size(); ++i) {
		const auto side = groupEntrySide(g_executableIconSizes[i]);
		group.insert(group.end(), {side, side, 0, 0});
		appendU16(group, 1);
		appendU16(group, 32);
		appendU32(group, static_cast<uint32_t>(iImages[i].size()));
		appendU16(group, iIds[i]);
	}
	return group;
}

auto groupIconIds(const std::vector<uint8_t>& iGroup) -> std::set<uint16_t> {
	std::set<uint16_t> ids;
	const auto count = readU16(iGroup, 4).value_or(0);
	for (size_t i = 0; i < count; ++i)
		if (const auto id = readU16(iGroup, 6 + 14 * i + 12); id)
			ids.insert(*id);
	return ids;
}

auto readFile(const std::filesystem::path& iFile) -> std::optional<std::vector<uint8_t>> {
	std::ifstream in(iFile, std::ios::binary);
	if (!in.is_open())
		return std::nullopt;
	std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
	if (in.bad())
		return std::nullopt;
	return bytes;
}

}// namespace

auto getErrorMessage(const ExecutableIconError iError) -> std::string_view {
	switch (iError) {
		case ExecutableIconError::ReadFailed:
			return "The executable or the icon image cannot be read";
		case ExecutableIconError::NotExecutable:
			return "The file is not a Windows executable";
		case ExecutableIconError::Corrupted:
			return "The Windows executable is corrupted";
		case ExecutableIconError::Signed:
			return "The Windows executable is signed";
		case ExecutableIconError::NoHeaderRoom:
			return "The Windows executable has no room for a new resource section";
		case ExecutableIconError::ImageDecode:
			return "The icon image cannot be decoded";
		case ExecutableIconError::ImageEncode:
			return "The icon image cannot be encoded";
		case ExecutableIconError::WriteFailed:
			return "The patched executable cannot be written";
	}
	return "Unknown error";
}

auto readExecutableResources(const std::span<const uint8_t> iExecutable)
		-> expected<std::vector<ExecutableResource>, ExecutableIconError> {
	const auto layout = parsePe(iExecutable);
	if (!layout)
		return unexpected{layout.error()};
	return readResources(iExecutable, *layout);
}

auto writeExecutableResources(const std::span<const uint8_t> iExecutable,
							  const std::vector<ExecutableResource>& iResources)
		-> expected<std::vector<uint8_t>, ExecutableIconError> {
	const auto layout = parsePe(iExecutable);
	if (!layout)
		return unexpected{layout.error()};
	if (layout->securitySize != 0) {
		OWL_CORE_WARN("ExecutableIcon: The executable is signed, its resources cannot change.")
		return unexpected{ExecutableIconError::Signed};
	}
	std::vector<uint8_t> out(iExecutable.begin(), iExecutable.end());
	Section section;
	if (const auto inPlace = isRewritableInPlace(*layout, out.size()); inPlace) {
		section = layout->sections[*inPlace];
		out.resize(section.rawPointer);
	} else {
		auto appended = appendSection(out, *layout);
		if (!appended)
			return unexpected{appended.error()};
		section = *appended;
	}
	const auto tree = buildResourceSection(iResources, section.virtualAddress);
	section.virtualSize = static_cast<uint32_t>(tree.size());
	section.rawSize = static_cast<uint32_t>(alignUp(tree.size(), layout->fileAlignment));
	out.insert(out.end(), tree.begin(), tree.end());
	out.resize(static_cast<size_t>(section.rawPointer) + section.rawSize, 0);
	setSection(out, section.headerOffset, section);
	putU32(out, layout->dataDirectories + 8 * g_resourceDirectory, section.virtualAddress);
	putU32(out, layout->dataDirectories + 8 * g_resourceDirectory + 4, section.virtualSize);
	uint64_t imageEnd =
			alignUp(static_cast<size_t>(section.virtualAddress) + section.virtualSize, layout->sectionAlignment);
	for (const auto& other: layout->sections | std::views::filter([&section](const Section& iSection) -> bool {
								return iSection.headerOffset != section.headerOffset;
							}))
		imageEnd = std::max<uint64_t>(imageEnd, alignUp(static_cast<size_t>(other.virtualAddress) +
																std::max(other.virtualSize, other.rawSize),
														layout->sectionAlignment));
	putU32(out, layout->optionalHeader + 56, static_cast<uint32_t>(imageEnd));
	putU32(out, layout->optionalHeader + 64, computeChecksum(out, layout->optionalHeader + 64));
	return out;
}

auto encodeIconImages(const std::span<const uint8_t> iImageFile)
		-> expected<std::vector<std::vector<uint8_t>>, ExecutableIconError> {
	const auto image = decodeImage(iImageFile);
	if (!image)
		return unexpected{image.error()};
	std::vector<std::vector<uint8_t>> images;
	for (const uint16_t size: g_executableIconSizes) {
		const auto pixels = fitSquare(*image, size);
		if (size < 256) {
			images.push_back(encodeBitmap(pixels, size));
			continue;
		}
		auto png = encodePng(pixels, size);
		if (!png) {
			OWL_CORE_WARN("ExecutableIcon: Cannot encode the {}-pixel icon image.", size)
			return unexpected{ExecutableIconError::ImageEncode};
		}
		images.push_back(std::move(*png));
	}
	return images;
}

auto setExecutableIcon(const std::span<const uint8_t> iExecutable, const std::span<const uint8_t> iImageFile)
		-> expected<std::vector<uint8_t>, ExecutableIconError> {
	auto resources = readExecutableResources(iExecutable);
	if (!resources)
		return unexpected{resources.error()};
	const auto images = encodeIconImages(iImageFile);
	if (!images)
		return unexpected{images.error()};
	ResourceId groupName{.id = 1, .name = {}};
	uint16_t language = g_defaultLanguage;
	std::set<uint16_t> replaced;
	if (const auto group = std::ranges::find_if(resources.value(),
												[](const ExecutableResource& iResource) -> bool {
													return iResource.type.name.empty() &&
														   iResource.type.id == g_resourceTypeGroupIcon;
												});
		group != resources->end()) {
		groupName = group->name;
		language = group->language;
		replaced = groupIconIds(group->data);
		resources->erase(group);
	}
	std::erase_if(resources.value(), [&replaced](const ExecutableResource& iResource) -> bool {
		return iResource.type.name.empty() && iResource.type.id == g_resourceTypeIcon && iResource.name.name.empty() &&
			   replaced.contains(iResource.name.id);
	});
	std::set<uint16_t> used;
	for (const auto& resource: resources.value())
		if (resource.type.name.empty() && resource.type.id == g_resourceTypeIcon && resource.name.name.empty())
			used.insert(resource.name.id);
	std::vector<uint16_t> ids;
	for (uint16_t id = 1; ids.size() < images->size(); ++id)
		if (!used.contains(id))
			ids.push_back(id);
	for (size_t i = 0; i < images->size(); ++i)
		resources->push_back({.type = {.id = g_resourceTypeIcon, .name = {}},
							  .name = {.id = ids[i], .name = {}},
							  .language = language,
							  .codePage = 0,
							  .data = (*images)[i]});
	resources->push_back({.type = {.id = g_resourceTypeGroupIcon, .name = {}},
						  .name = groupName,
						  .language = language,
						  .codePage = 0,
						  .data = buildGroup(*images, ids)});
	return writeExecutableResources(iExecutable, *resources);
}

auto setExecutableIcon(const std::filesystem::path& iExecutable, const std::filesystem::path& iImageFile)
		-> expected<void, ExecutableIconError> {
	const auto executable = readFile(iExecutable);
	const auto image = readFile(iImageFile);
	if (!executable || !image) {
		OWL_CORE_WARN("ExecutableIcon: Cannot read '{}'.", (!executable ? iExecutable : iImageFile).string())
		return unexpected{ExecutableIconError::ReadFailed};
	}
	const auto patched = setExecutableIcon(*executable, *image);
	if (!patched)
		return unexpected{patched.error()};
	const std::string_view bytes{reinterpret_cast<const char*>(patched->data()), patched->size()};
	if (!platform::writeFileAtomic(iExecutable, bytes)) {
		OWL_CORE_WARN("ExecutableIcon: Cannot write '{}'.", iExecutable.string())
		return unexpected{ExecutableIconError::WriteFailed};
	}
	OWL_CORE_DEBUG("ExecutableIcon: Icon of '{}' set from '{}'.", iExecutable.string(), iImageFile.string())
	return {};
}

}// namespace owl::data::assets::pack
