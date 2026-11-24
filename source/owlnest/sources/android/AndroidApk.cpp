/**
 * @file AndroidApk.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "AndroidApk.h"

#include <core/Log.h>
#include <data/assets/pack/ExportReport.h>

#include <openssl/evp.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest::android {

namespace {

using Bytes = std::vector<uint8_t>;

constexpr std::string_view g_androidUri = "http://schemas.android.com/apk/res/android";
constexpr std::string_view g_abi = "arm64-v8a";
constexpr std::string_view g_runnerLibrary = "libOwlGameRunner.so";
// The list of the game files the engine extracts at the first launch (platform/AndroidApp.h).
constexpr std::string_view g_assetList = "owl_assets.txt";
constexpr uint32_t g_none = 0xffffffff;
constexpr uint32_t g_iconResource = 0x7f010000;
// @android:style/Theme.NoTitleBar.Fullscreen.
constexpr uint32_t g_fullscreenTheme = 0x01030007;
// orientation|screenSize|screenLayout|keyboardHidden|keyboard|navigation|uiMode|density.
constexpr uint32_t g_configChanges = 0x17f0;
constexpr uint32_t g_sensorLandscape = 6;
constexpr uint32_t g_vulkan13 = 0x00403000;

// Res_value types.
constexpr uint8_t g_typeReference = 0x01;
constexpr uint8_t g_typeString = 0x03;
constexpr uint8_t g_typeIntDec = 0x10;
constexpr uint8_t g_typeIntHex = 0x11;
constexpr uint8_t g_typeBoolean = 0x12;

void putU8(Bytes& ioOut, const uint8_t iValue) { ioOut.push_back(iValue); }

void putU16(Bytes& ioOut, const uint32_t iValue) {
	ioOut.push_back(static_cast<uint8_t>(iValue));
	ioOut.push_back(static_cast<uint8_t>(iValue >> 8u));
}

void putU32(Bytes& ioOut, const uint32_t iValue) {
	for (uint32_t i = 0; i < 4; ++i) ioOut.push_back(static_cast<uint8_t>(iValue >> (8 * i)));
}

void setU32(Bytes& ioOut, const size_t iOffset, const uint32_t iValue) {
	for (uint32_t i = 0; i < 4; ++i) ioOut[iOffset + i] = static_cast<uint8_t>(iValue >> (8 * i));
}

// A chunk header (`ResChunk_header`); the size is patched by `closeChunk`.
auto openChunk(Bytes& ioOut, const uint32_t iType, const uint32_t iHeaderSize) -> size_t {
	const auto start = ioOut.size();
	putU16(ioOut, iType);
	putU16(ioOut, iHeaderSize);
	putU32(ioOut, 0);
	return start;
}

void closeChunk(Bytes& ioOut, const size_t iStart) {
	setU32(ioOut, iStart + 4, static_cast<uint32_t>(ioOut.size() - iStart));
}

void putLength8(Bytes& ioOut, const size_t iLength) {
	if (iLength > 0x7f)
		putU8(ioOut, static_cast<uint8_t>(0x80u | (iLength >> 8u)));
	putU8(ioOut, static_cast<uint8_t>(iLength & 0xffu));
}

auto utf16Length(const std::string_view iText) -> size_t {
	size_t length = 0;
	for (const char c: iText) {
		const auto byte = static_cast<uint8_t>(c);
		if ((byte & 0xc0u) != 0x80u)
			length += (byte >= 0xf0u) ? 2 : 1;
	}
	return length;
}

// A UTF-8 string pool (`ResStringPool`).
void putStringPool(Bytes& ioOut, const std::vector<std::string>& iStrings) {
	const auto start = openChunk(ioOut, 0x0001, 28);
	putU32(ioOut, static_cast<uint32_t>(iStrings.size()));
	putU32(ioOut, 0);
	putU32(ioOut, 0x100);
	putU32(ioOut, static_cast<uint32_t>(28 + 4 * iStrings.size()));
	putU32(ioOut, 0);
	Bytes data;
	for (const auto& text: iStrings) {
		putU32(ioOut, static_cast<uint32_t>(data.size()));
		putLength8(data, utf16Length(text));
		putLength8(data, text.size());
		data.insert(data.end(), text.begin(), text.end());
		putU8(data, 0);
	}
	while (data.size() % 4 != 0) putU8(data, 0);
	ioOut.insert(ioOut.end(), data.begin(), data.end());
	closeChunk(ioOut, start);
}

struct XmlAttribute {
	std::string name;
	// Resource id of an `android:` attribute, 0 for an attribute without namespace.
	uint32_t resource = 0;
	uint8_t type = g_typeString;
	uint32_t data = 0;
	std::string text;
};

// An element of a document listed in order, its depth giving its parent (the last element one level up).
struct XmlElement {
	std::string name;
	uint32_t depth = 0;
	std::vector<XmlAttribute> attributes;
};

using XmlDocument = std::vector<XmlElement>;

auto text(std::string iName, const uint32_t iResource, std::string iValue) -> XmlAttribute {
	return {.name = std::move(iName),
			.resource = iResource,
			.type = g_typeString,
			.data = 0,
			.text = std::move(iValue)};
}

auto value(std::string iName, const uint32_t iResource, const uint8_t iType, const uint32_t iData) -> XmlAttribute {
	return {.name = std::move(iName), .resource = iResource, .type = iType, .data = iData, .text = {}};
}

auto flag(std::string iName, const uint32_t iResource, const bool iValue) -> XmlAttribute {
	return value(std::move(iName), iResource, g_typeBoolean, iValue ? g_none : 0);
}

// The strings of a binary XML: the names of the `android:` attributes first, in the order of the resource map.
class XmlStrings {
public:
	void collect(const XmlDocument& iDocument) {
		for (const auto& element: iDocument)
			for (const auto& attribute: element.attributes)
				if (attribute.resource != 0 && !m_index.contains(attribute.name)) {
					m_index[attribute.name] = static_cast<uint32_t>(m_strings.size());
					m_strings.push_back(attribute.name);
					m_resources.push_back(attribute.resource);
				}
	}

	void collectOthers(const XmlDocument& iDocument) {
		for (const auto& element: iDocument) {
			add(element.name);
			for (const auto& attribute: element.attributes) {
				add(attribute.name);
				if (attribute.type == g_typeString)
					add(attribute.text);
			}
		}
	}

	auto add(const std::string& iText) -> uint32_t {
		if (const auto found = m_index.find(iText); found != m_index.end())
			return found->second;
		m_index[iText] = static_cast<uint32_t>(m_strings.size());
		m_strings.push_back(iText);
		return m_index[iText];
	}

	[[nodiscard]] auto at(const std::string& iText) const -> uint32_t { return m_index.at(iText); }

	[[nodiscard]] auto getStrings() const -> const std::vector<std::string>& { return m_strings; }

	[[nodiscard]] auto getResources() const -> const std::vector<uint32_t>& { return m_resources; }

private:
	std::vector<std::string> m_strings;
	std::vector<uint32_t> m_resources;
	std::map<std::string, uint32_t, std::less<>> m_index;
};

void putStartElement(Bytes& ioOut, const XmlElement& iElement, const XmlStrings& iStrings, const uint32_t iUri) {
	auto attributes = iElement.attributes;
	// Android looks the attributes up by resource id: sorted as aapt2 sorts them, the ones without id last.
	std::ranges::stable_sort(attributes, [](const XmlAttribute& iLeft, const XmlAttribute& iRight) -> bool {
		const auto left = iLeft.resource == 0 ? g_none : iLeft.resource;
		const auto right = iRight.resource == 0 ? g_none : iRight.resource;
		return left < right;
	});
	const auto start = openChunk(ioOut, 0x0102, 16);
	putU32(ioOut, 1);
	putU32(ioOut, g_none);
	putU32(ioOut, g_none);
	putU32(ioOut, iStrings.at(iElement.name));
	putU16(ioOut, 20);
	putU16(ioOut, 20);
	putU16(ioOut, static_cast<uint32_t>(attributes.size()));
	putU16(ioOut, 0);
	putU16(ioOut, 0);
	putU16(ioOut, 0);
	for (const auto& attribute: attributes) {
		const bool isText = attribute.type == g_typeString;
		putU32(ioOut, attribute.resource != 0 ? iUri : g_none);
		putU32(ioOut, iStrings.at(attribute.name));
		putU32(ioOut, isText ? iStrings.at(attribute.text) : g_none);
		putU16(ioOut, 8);
		putU8(ioOut, 0);
		putU8(ioOut, attribute.type);
		putU32(ioOut, isText ? iStrings.at(attribute.text) : attribute.data);
	}
	closeChunk(ioOut, start);
}

void putEndElement(Bytes& ioOut, const std::string& iName, const XmlStrings& iStrings) {
	const auto end = openChunk(ioOut, 0x0103, 16);
	putU32(ioOut, 1);
	putU32(ioOut, g_none);
	putU32(ioOut, g_none);
	putU32(ioOut, iStrings.at(iName));
	closeChunk(ioOut, end);
}

void putElements(Bytes& ioOut, const XmlDocument& iDocument, const XmlStrings& iStrings, const uint32_t iUri) {
	std::vector<std::string> open;
	for (const auto& element: iDocument) {
		while (open.size() > element.depth) {
			putEndElement(ioOut, open.back(), iStrings);
			open.pop_back();
		}
		putStartElement(ioOut, element, iStrings, iUri);
		open.push_back(element.name);
	}
	for (const auto& name: std::views::reverse(open)) putEndElement(ioOut, name, iStrings);
}

void putNamespace(Bytes& ioOut, const uint32_t iType, const uint32_t iPrefix, const uint32_t iUri) {
	const auto start = openChunk(ioOut, iType, 16);
	putU32(ioOut, 1);
	putU32(ioOut, g_none);
	putU32(ioOut, iPrefix);
	putU32(ioOut, iUri);
	closeChunk(ioOut, start);
}

auto manifestTree(const ApkManifest& iManifest) -> XmlDocument {
	std::vector<XmlAttribute> application{
			text("label", 0x01010001, iManifest.label), flag("hasCode", 0x0101000c, false),
			flag("debuggable", 0x0101000f, iManifest.debuggable), flag("extractNativeLibs", 0x010104ea, true)};
	if (iManifest.hasIcon)
		application.push_back(value("icon", 0x01010002, g_typeReference, g_iconResource));
	return {{.name = "manifest",
			 .depth = 0,
			 .attributes = {value("versionCode", 0x0101021b, g_typeIntDec, iManifest.versionCode),
							text("versionName", 0x0101021c, iManifest.versionName),
							text("package", 0, iManifest.package)}},
			{.name = "uses-sdk",
			 .depth = 1,
			 .attributes = {value("minSdkVersion", 0x0101020c, g_typeIntDec, iManifest.minSdk),
							value("targetSdkVersion", 0x01010270, g_typeIntDec, iManifest.targetSdk)}},
			{.name = "uses-feature",
			 .depth = 1,
			 .attributes = {text("name", 0x01010003, "android.hardware.vulkan.version"),
							flag("required", 0x0101028e, true),
							value("version", 0x01010519, g_typeIntHex, g_vulkan13)}},
			{.name = "uses-permission",
			 .depth = 1,
			 .attributes = {text("name", 0x01010003, "android.permission.INTERNET")}},
			{.name = "application", .depth = 1, .attributes = std::move(application)},
			{.name = "activity",
			 .depth = 2,
			 .attributes = {value("theme", 0x01010000, g_typeReference, g_fullscreenTheme),
							text("name", 0x01010003, "android.app.NativeActivity"), flag("exported", 0x01010010, true),
							value("screenOrientation", 0x0101001e, g_typeIntDec, g_sensorLandscape),
							value("configChanges", 0x0101001f, g_typeIntHex, g_configChanges)}},
			{.name = "meta-data",
			 .depth = 3,
			 .attributes = {text("name", 0x01010003, "android.app.lib_name"),
							text("value", 0x01010024, "OwlGameRunner")}},
			{.name = "intent-filter", .depth = 3, .attributes = {}},
			{.name = "action", .depth = 4, .attributes = {text("name", 0x01010003, "android.intent.action.MAIN")}},
			{.name = "category",
			 .depth = 4,
			 .attributes = {text("name", 0x01010003, "android.intent.category.LAUNCHER")}}};
}

auto sha256Hex(const std::span<const uint8_t> iData) -> std::string {
	std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
	unsigned int length = 0;
	EVP_Digest(iData.data(), iData.size(), digest.data(), &length, EVP_sha256(), nullptr);
	std::string hex;
	for (unsigned int i = 0; i < length; ++i) hex += std::format("{:02x}", digest[i]);
	return hex;
}

auto readFile(const std::filesystem::path& iFile) -> Bytes {
	std::ifstream in(iFile, std::ios::binary);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// A zip written in memory: deflated entries, or stored ones aligned for `mmap` (the 0xd935 extra field of zipalign).
class ZipBuilder {
public:
	void add(const std::string& iName, const std::span<const uint8_t> iData, const bool iCompress,
			 const size_t iAlignment = 4) {
		const auto crc = static_cast<uint32_t>(crc32(0, iData.data(), static_cast<uInt>(iData.size())));
		Bytes stored;
		std::span<const uint8_t> payload = iData;
		if (iCompress) {
			stored = deflateRaw(iData);
			payload = stored;
		}
		const auto offset = static_cast<uint32_t>(m_data.size());
		Bytes extra;
		if (!iCompress) {
			const size_t dataStart = m_data.size() + 30 + iName.size() + 6;
			const size_t padding = (iAlignment - dataStart % iAlignment) % iAlignment;
			putU16(extra, 0xd935);
			putU16(extra, static_cast<uint32_t>(2 + padding));
			putU16(extra, static_cast<uint32_t>(iAlignment));
			extra.insert(extra.end(), padding, 0);
		}
		putU32(m_data, 0x04034b50);
		putHeaderFields(m_data, iCompress, crc, payload.size(), iData.size(), iName.size());
		putU16(m_data, static_cast<uint32_t>(extra.size()));
		m_data.insert(m_data.end(), iName.begin(), iName.end());
		m_data.insert(m_data.end(), extra.begin(), extra.end());
		m_data.insert(m_data.end(), payload.begin(), payload.end());
		m_entries.push_back({.name = iName,
							 .compressed = iCompress,
							 .crc = crc,
							 .packedSize = payload.size(),
							 .size = iData.size(),
							 .offset = offset});
	}

	[[nodiscard]] auto finish() -> Bytes {
		const auto directory = static_cast<uint32_t>(m_data.size());
		for (const auto& entry: m_entries) {
			putU32(m_data, 0x02014b50);
			putU16(m_data, 20);
			putHeaderFields(m_data, entry.compressed, entry.crc, entry.packedSize, entry.size, entry.name.size());
			putU16(m_data, 0);
			putU16(m_data, 0);
			putU16(m_data, 0);
			putU16(m_data, 0);
			putU32(m_data, 0);
			putU32(m_data, entry.offset);
			m_data.insert(m_data.end(), entry.name.begin(), entry.name.end());
		}
		const auto directorySize = static_cast<uint32_t>(m_data.size() - directory);
		putU32(m_data, 0x06054b50);
		putU16(m_data, 0);
		putU16(m_data, 0);
		putU16(m_data, static_cast<uint32_t>(m_entries.size()));
		putU16(m_data, static_cast<uint32_t>(m_entries.size()));
		putU32(m_data, directorySize);
		putU32(m_data, directory);
		putU16(m_data, 0);
		return std::move(m_data);
	}

private:
	struct Entry {
		std::string name;
		bool compressed = false;
		uint32_t crc = 0;
		size_t packedSize = 0;
		size_t size = 0;
		uint32_t offset = 0;
	};

	// Version, flags, method, time, date (1980-01-01), CRC, sizes and name length of a local or central header.
	static void putHeaderFields(Bytes& ioOut, const bool iCompress, const uint32_t iCrc, const size_t iPacked,
								const size_t iSize, const size_t iNameLength) {
		putU16(ioOut, 20);
		putU16(ioOut, 0x0800);
		putU16(ioOut, iCompress ? 8 : 0);
		putU16(ioOut, 0);
		putU16(ioOut, 0x21);
		putU32(ioOut, iCrc);
		putU32(ioOut, static_cast<uint32_t>(iPacked));
		putU32(ioOut, static_cast<uint32_t>(iSize));
		putU16(ioOut, static_cast<uint32_t>(iNameLength));
	}

	static auto deflateRaw(const std::span<const uint8_t> iData) -> Bytes {
		z_stream stream{};
		deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY);
		Bytes out(deflateBound(&stream, static_cast<uLong>(iData.size())));
		stream.next_in = const_cast<Bytef*>(iData.data());// zlib's input is not const.
		stream.avail_in = static_cast<uInt>(iData.size());
		stream.next_out = out.data();
		stream.avail_out = static_cast<uInt>(out.size());
		deflate(&stream, Z_FINISH);
		out.resize(stream.total_out);
		deflateEnd(&stream);
		return out;
	}

	Bytes m_data;
	std::vector<Entry> m_entries;
};

}// namespace

auto getErrorMessage(const ApkError iError) -> std::string_view {
	switch (iError) {
		case ApkError::NoGame:
			return "no exported game";
		case ApkError::NoRunner:
			return "no Android runner (download or import it)";
		case ApkError::Signing:
			return "the APK could not be signed";
		case ApkError::Write:
			return "the APK could not be written";
	}
	return "unknown error";
}

auto makeBinaryManifest(const ApkManifest& iManifest) -> std::vector<uint8_t> {
	const auto tree = manifestTree(iManifest);
	XmlStrings strings;
	strings.collect(tree);
	const auto prefix = strings.add("android");
	const auto uri = strings.add(std::string{g_androidUri});
	strings.collectOthers(tree);
	Bytes xml;
	const auto start = openChunk(xml, 0x0003, 8);
	putStringPool(xml, strings.getStrings());
	const auto map = openChunk(xml, 0x0180, 8);
	for (const auto resource: strings.getResources()) putU32(xml, resource);
	closeChunk(xml, map);
	putNamespace(xml, 0x0100, prefix, uri);
	putElements(xml, tree, strings, uri);
	putNamespace(xml, 0x0101, prefix, uri);
	closeChunk(xml, start);
	return xml;
}

auto makeIconResources(const std::string_view iPackage) -> std::vector<uint8_t> {
	Bytes table;
	const auto start = openChunk(table, 0x0002, 12);
	putU32(table, 1);
	putStringPool(table, {"res/mipmap/icon.png"});
	const auto package = openChunk(table, 0x0200, 288);
	putU32(table, 0x7f);
	for (size_t i = 0; i < 128; ++i) putU16(table, i < iPackage.size() ? static_cast<uint8_t>(iPackage[i]) : 0);
	const auto typeStringsField = table.size();
	putU32(table, 0);
	putU32(table, 0);
	const auto keyStringsField = table.size();
	putU32(table, 0);
	putU32(table, 0);
	putU32(table, 0);
	setU32(table, typeStringsField, static_cast<uint32_t>(table.size() - package));
	putStringPool(table, {"mipmap"});
	setU32(table, keyStringsField, static_cast<uint32_t>(table.size() - package));
	putStringPool(table, {"icon"});
	const auto spec = openChunk(table, 0x0202, 16);
	putU8(table, 1);
	putU8(table, 0);
	putU16(table, 0);
	putU32(table, 1);
	putU32(table, 0);
	closeChunk(table, spec);
	// ResTable_type with the default configuration (`ResTable_config` of 64 bytes, every field 0).
	constexpr uint32_t configSize = 64;
	constexpr uint32_t typeHeader = 20 + configSize;
	const auto type = openChunk(table, 0x0201, typeHeader);
	putU8(table, 1);
	putU8(table, 0);
	putU16(table, 0);
	putU32(table, 1);
	putU32(table, typeHeader + 4);
	putU32(table, configSize);
	table.insert(table.end(), configSize - 4, 0);
	putU32(table, 0);
	putU16(table, 8);
	putU16(table, 0);
	putU32(table, 0);
	putU16(table, 8);
	putU8(table, 0);
	putU8(table, g_typeString);
	putU32(table, 0);
	closeChunk(table, type);
	closeChunk(table, package);
	closeChunk(table, start);
	return table;
}

auto getVersionCode(const std::string_view iVersion) -> uint32_t {
	std::array<uint32_t, 3> parts{};
	size_t part = 0;
	const auto* cursor = iVersion.data();
	const auto* end = iVersion.data() + iVersion.size();
	while (cursor < end && part < parts.size()) {
		const auto [next, error] = std::from_chars(cursor, end, parts[part]);
		if (error != std::errc{})
			break;
		++part;
		cursor = next < end && *next == '.' ? next + 1 : end;
	}
	return std::max(1u, parts[0] * 10000 + std::min(parts[1], 99u) * 100 + std::min(parts[2], 99u));
}

auto writeApk(const std::filesystem::path& iGameDirectory, const std::filesystem::path& iRunnerDirectory,
			  const std::filesystem::path& iIcon, const ApkManifest& iManifest, const SigningKey& iKey,
			  const std::filesystem::path& iOutput) -> expected<void, ApkError> {
	if (!is_directory(iGameDirectory)) {
		OWL_CORE_WARN("AndroidApk: No exported game in '{}'.", iGameDirectory.string())
		return unexpected{ApkError::NoGame};
	}
	const auto libraries = iRunnerDirectory / "lib" / std::string{g_abi};
	if (!is_regular_file(libraries / std::string{g_runnerLibrary})) {
		OWL_CORE_WARN("AndroidApk: No {} in '{}'.", g_runnerLibrary, libraries.string())
		return unexpected{ApkError::NoRunner};
	}
	auto manifest = iManifest;
	manifest.hasIcon = !iIcon.empty() && is_regular_file(iIcon);
	ZipBuilder zip;
	zip.add("AndroidManifest.xml", makeBinaryManifest(manifest), true);
	if (manifest.hasIcon) {
		zip.add("res/mipmap/icon.png", readFile(iIcon), false);
		zip.add("resources.arsc", makeIconResources(manifest.package), false);
	}
	std::vector<std::filesystem::path> sharedLibraries;
	for (const auto& entry: std::filesystem::directory_iterator(libraries))
		if (entry.is_regular_file() && entry.path().extension() == ".so")
			sharedLibraries.push_back(entry.path());
	std::ranges::sort(sharedLibraries);
	for (const auto& library: sharedLibraries)
		zip.add(std::format("lib/{}/{}", g_abi, library.filename().string()), readFile(library), true);

	std::vector<std::filesystem::path> files;
	// The export report stays on the workstation: the game never reads it.
	for (const auto& entry: std::filesystem::recursive_directory_iterator(iGameDirectory))
		if (entry.is_regular_file() && entry.path().filename() != data::assets::pack::g_exportReportFile)
			files.push_back(relative(entry.path(), iGameDirectory));
	std::ranges::sort(files);
	Bytes identity;
	std::string listing;
	for (const auto& file: files) {
		const auto name = file.generic_string();
		const auto content = readFile(iGameDirectory / file);
		identity.insert(identity.end(), name.begin(), name.end());
		identity.insert(identity.end(), content.begin(), content.end());
		listing += name + "\n";
		zip.add("assets/" + name, content, true);
	}
	const auto list = std::format("{}\n{}", sha256Hex(identity), listing);
	zip.add(std::format("assets/{}", g_assetList),
			std::span{reinterpret_cast<const uint8_t*>(list.data()), list.size()}, true);

	const auto signedApk = signApk(zip.finish(), iKey);
	if (!signedApk)
		return unexpected{ApkError::Signing};
	std::error_code ec;
	std::filesystem::create_directories(iOutput.parent_path(), ec);
	std::ofstream out(iOutput, std::ios::binary);
	out.write(reinterpret_cast<const char*>(signedApk->data()), static_cast<std::streamsize>(signedApk->size()));
	if (!out.good()) {
		OWL_CORE_WARN("AndroidApk: Cannot write '{}'.", iOutput.string())
		return unexpected{ApkError::Write};
	}
	OWL_CORE_INFO("AndroidApk: {} written ({} libraries, {} game files, {:.1f} MiB).", iOutput.string(),
				  sharedLibraries.size(), files.size(), static_cast<double>(signedApk->size()) / (1024.0 * 1024.0))
	return {};
}

}// namespace owl::nest::android
