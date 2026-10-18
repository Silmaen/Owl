/**
 * @file Font.cpp
 * @author Silmaen
 * @date 10/17/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/fonts/Font.h"

#include "app/Application.h"
#include "renderer/gpu/Texture.h"

#undef INFINITE
#include <msdf-atlas-gen/msdf-atlas-gen.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::data::fonts {

struct MsdfData {
	static constexpr size_t denseCount = 256;

	struct DenseGlyph {
		Font::GlyphMetrics box;
		float advance = 0.f;
		bool valid = false;
	};

	std::vector<msdf_atlas::GlyphGeometry> glyphs;
	msdf_atlas::FontGeometry fontGeometry;
	std::array<DenseGlyph, denseCount> dense{};
	bool denseAdvance = false;
};

namespace {
constexpr double g_emSize = 40.0;
// Distance range in atlas texels; the text shader (`text.slang`, `kPxRange`) must use the same value.
constexpr double g_pixelRange = 6.0;
constexpr double g_miterLimit = 1.0;
constexpr double g_angleThreshold = 3.0;
constexpr uint64_t g_lcgMultiplier = 6364136223846793005;
constexpr uint32_t g_channels = 3;
// Bumped whenever the generation or the cache layout changes, so old cache files stop matching.
constexpr uint32_t g_atlasCacheVersion = 2;
constexpr std::array<char, 8> g_atlasCacheMagic{'O', 'W', 'L', 'A', 'T', 'L', 'A', 'S'};

struct AtlasBitmap {
	math::vec2ui size;
	std::vector<uint8_t> pixels;
};

// FNV-1a 64-bit, enough to tell two inputs of the atlas apart.
class Hasher final {
public:
	void add(const std::span<const uint8_t> iBytes) {
		for (const uint8_t byte: iBytes) {
			m_hash ^= byte;
			m_hash *= 1099511628211ull;
		}
	}

	template<typename T>
	void addValue(const T& iValue) {
		std::array<uint8_t, sizeof(T)> bytes{};
		std::memcpy(bytes.data(), &iValue, sizeof(T));
		add(bytes);
	}

	[[nodiscard]] auto get() const -> uint64_t { return m_hash; }

private:
	uint64_t m_hash = 14695981039346656037ull;
};

auto readFileBytes(const std::filesystem::path& iPath) -> std::vector<uint8_t> {
	std::ifstream in(iPath, std::ios::binary);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

auto computeAtlasKey(const std::filesystem::path& iFontPath, const std::vector<CodePointRange>& iRanges) -> uint64_t {
	Hasher hasher;
	hasher.add(readFileBytes(iFontPath));
	hasher.addValue(g_atlasCacheVersion);
	hasher.addValue(g_emSize);
	hasher.addValue(g_pixelRange);
	hasher.addValue(g_miterLimit);
	hasher.addValue(g_angleThreshold);
	for (const auto& [first, last]: iRanges) {
		hasher.addValue(static_cast<uint32_t>(first));
		hasher.addValue(static_cast<uint32_t>(last));
	}
	return hasher.get();
}

auto cacheFilePath(const std::string& iStem, const uint64_t iKey) -> std::filesystem::path {
	return Font::getAtlasCacheDirectory() / std::format("{}-{:016x}.owlatlas", iStem, iKey);
}

template<typename T>
auto readValue(std::ifstream& ioStream, T& oValue) -> bool {
	return static_cast<bool>(ioStream.read(reinterpret_cast<char*>(&oValue), sizeof(T)));
}

template<typename T>
void writeValue(std::ofstream& ioStream, const T& iValue) {
	ioStream.write(reinterpret_cast<const char*>(&iValue), sizeof(T));
}

auto readCachedAtlas(const std::filesystem::path& iPath, const uint64_t iKey, const math::vec2ui& iSize)
		-> std::optional<AtlasBitmap> {
	if (!exists(iPath))
		return std::nullopt;
	std::ifstream in(iPath, std::ios::binary);
	std::array<char, 8> magic{};
	uint32_t version = 0;
	uint64_t key = 0;
	AtlasBitmap bitmap;
	if (!in.read(magic.data(), magic.size()) || !readValue(in, version) || !readValue(in, key) ||
		!readValue(in, bitmap.size.x()) || !readValue(in, bitmap.size.y()) || magic != g_atlasCacheMagic ||
		version != g_atlasCacheVersion || key != iKey || bitmap.size != iSize) {
		OWL_CORE_WARN("Font: Atlas cache '{}' does not match, regenerating.", iPath.string())
		return std::nullopt;
	}
	bitmap.pixels.resize(static_cast<size_t>(iSize.x()) * iSize.y() * g_channels);
	if (!in.read(reinterpret_cast<char*>(bitmap.pixels.data()), static_cast<std::streamsize>(bitmap.pixels.size()))) {
		OWL_CORE_WARN("Font: Atlas cache '{}' is truncated, regenerating.", iPath.string())
		return std::nullopt;
	}
	return bitmap;
}

void writeCachedAtlas(const std::filesystem::path& iPath, const std::string& iStem, const uint64_t iKey,
					  const AtlasBitmap& iBitmap) {
	std::error_code error;
	create_directories(iPath.parent_path(), error);
	// Atlases of older inputs of the same font are stale for good: drop them.
	for (const auto& entry: std::filesystem::directory_iterator(iPath.parent_path(), error)) {
		const auto name = entry.path().filename().string();
		if (entry.path() != iPath && entry.path().extension() == ".owlatlas" && name.starts_with(iStem + "-") &&
			name.size() == iStem.size() + 1 + 16 + 9)
			std::filesystem::remove(entry.path(), error);
	}
	std::ofstream out(iPath, std::ios::binary | std::ios::trunc);
	out.write(g_atlasCacheMagic.data(), g_atlasCacheMagic.size());
	writeValue(out, g_atlasCacheVersion);
	writeValue(out, iKey);
	writeValue(out, iBitmap.size.x());
	writeValue(out, iBitmap.size.y());
	out.write(reinterpret_cast<const char*>(iBitmap.pixels.data()),
			  static_cast<std::streamsize>(iBitmap.pixels.size()));
	if (!out)
		OWL_CORE_WARN("Font: Cannot write the atlas cache '{}'.", iPath.string())
}

auto generationThreadCount() -> int {
	if (app::Application::instanced())
		return static_cast<int>(app::Application::get().getTaskScheduler().getWorkerCount());
	return static_cast<int>(std::max(std::thread::hardware_concurrency(), 1u));
}

auto generateAtlas(std::vector<msdf_atlas::GlyphGeometry>& ioGlyphs, const math::vec2ui& iSize) -> AtlasBitmap {
	uint64_t glyphSeed = 0;
	for (msdf_atlas::GlyphGeometry& glyph: ioGlyphs) {
		glyphSeed *= g_lcgMultiplier;
		glyph.edgeColoring(msdfgen::edgeColoringInkTrap, g_angleThreshold, glyphSeed);
	}
	msdf_atlas::GeneratorAttributes attributes;
	attributes.config.overlapSupport = true;
	attributes.scanlinePass = true;
	msdf_atlas::ImmediateAtlasGenerator<float, g_channels, msdf_atlas::msdfGenerator,
										msdf_atlas::BitmapAtlasStorage<uint8_t, g_channels>>
			generator(static_cast<int>(iSize.x()), static_cast<int>(iSize.y()));
	generator.setAttributes(attributes);
	generator.setThreadCount(generationThreadCount());
	generator.generate(ioGlyphs.data(), static_cast<int>(ioGlyphs.size()));
	const auto bitmap = static_cast<msdfgen::BitmapConstRef<uint8_t, g_channels>>(generator.atlasStorage());
	AtlasBitmap result{.size = iSize, .pixels = {}};
	result.pixels.assign(bitmap.pixels, bitmap.pixels + static_cast<size_t>(iSize.x()) * iSize.y() * g_channels);
	return result;
}

auto createTexture(AtlasBitmap& ioBitmap) -> shared<renderer::gpu::Texture2D> {
	const renderer::gpu::Texture::Specification spec{.size = ioBitmap.size,
													 .format = renderer::gpu::ImageFormat::Rgb8,
													 .generateMips = false};
	shared<renderer::gpu::Texture2D> texture = renderer::gpu::Texture2D::create(spec);
	texture->setData(ioBitmap.pixels.data(), static_cast<uint32_t>(ioBitmap.pixels.size()));
	return texture;
}
}// namespace

Font::Font(std::filesystem::path iPath, const bool iIsDefault, FontSettings iSettings)
	: m_path{std::move(iPath)}, m_settings{std::move(iSettings)}, m_default{iIsDefault} {
	OWL_SCOPE_UNTRACK
	if (!exists(m_path)) {
		OWL_CORE_ERROR("Font: Font file {} does not exists.", m_path.string())
		return;
	}
	msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
	if (ft == nullptr) {
		OWL_CORE_ERROR("Font: Failed to initialize Freetype library.")
		return;
	}
	msdfgen::FontHandle* font = loadFont(ft, m_path.string().c_str());
	if (font == nullptr) {
		OWL_CORE_ERROR("Font: Failed to load font: {}.", m_path.string())
		deinitializeFreetype(ft);
		return;
	}
	const auto ranges = m_settings.getCharsetRanges();
	msdf_atlas::Charset charset;
	for (const auto& [first, last]: ranges) {
		for (char32_t c = first; c <= last; ++c) charset.add(c);
	}
	m_data = mkShared<MsdfData>();
	constexpr double fontScale = 1.0;
	m_data->fontGeometry = msdf_atlas::FontGeometry(&m_data->glyphs);
	const int glyphsLoaded = m_data->fontGeometry.loadCharset(font, fontScale, charset);
	destroyFont(font);
	deinitializeFreetype(ft);
	OWL_CORE_INFO("Font {}: Loaded {} glyphs from font (out of {}).", m_path.stem().string(), glyphsLoaded,
				  charset.size())

	msdf_atlas::TightAtlasPacker atlasPacker;
	atlasPacker.setPixelRange(g_pixelRange);
	atlasPacker.setMiterLimit(g_miterLimit);
	atlasPacker.setScale(g_emSize);
	if (const int remaining = atlasPacker.pack(m_data->glyphs.data(), static_cast<int>(m_data->glyphs.size()));
		remaining != 0) {
		OWL_CORE_ERROR("Font: Failed packing {} glyphs of {}.", remaining, m_path.string())
	}
	int width = 0;
	int height = 0;
	atlasPacker.getDimensions(width, height);
	const math::vec2ui size{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

	const auto stem = m_path.stem().string();
	const uint64_t key = computeAtlasKey(m_path, ranges);
	const auto cachePath = cacheFilePath(stem, key);
	auto bitmap = readCachedAtlas(cachePath, key, size);
	m_atlasFromCache = bitmap.has_value();
	if (!bitmap) {
		bitmap = generateAtlas(m_data->glyphs, size);
		writeCachedAtlas(cachePath, stem, key, *bitmap);
	}
	m_atlasTexture = createTexture(*bitmap);
	m_name = stem;
	m_data->denseAdvance = m_data->fontGeometry.getKerning().empty();
	for (size_t code = 0; code < MsdfData::denseCount; ++code) {
		const auto codePoint = static_cast<char32_t>(code);
		auto& entry = m_data->dense[code];
		entry.box = getGlyphBox(codePoint);
		entry.advance = getAdvance(codePoint, codePoint);
		entry.valid = true;
	}
}

Font::~Font() {
	if (m_atlasTexture) {
		m_atlasTexture.reset();
	}
}

auto Font::getAtlasCacheDirectory() -> std::filesystem::path {
	if (app::Application::instanced())
		return app::Application::get().getWorkingDirectory() / "cache" / "font";
	return std::filesystem::temp_directory_path() / "owl_cache" / "font";
}

void Font::reload(const FontSettings& iSettings) {
	const auto path = m_path;
	*this = Font(path, m_default, iSettings);
}

auto Font::hasGlyph(const char32_t iCodePoint) const -> bool {
	return m_data != nullptr && m_data->fontGeometry.getGlyph(static_cast<msdfgen::unicode_t>(iCodePoint)) != nullptr;
}

auto Font::getGlyphCount() const -> size_t { return m_data != nullptr ? m_data->glyphs.size() : 0; }

auto Font::getGlyphBox(const char32_t iCodePoint) const -> GlyphMetrics {
	GlyphMetrics result{.quad = {{0, 0}, {0, 0}}, .uv = {{0, 0}, {0, 0}}};
	if (m_data == nullptr || iCodePoint == U'\n')
		return result;
	if (iCodePoint < MsdfData::denseCount && m_data->dense[iCodePoint].valid)
		return m_data->dense[iCodePoint].box;
	const auto& metrics = m_data->fontGeometry.getMetrics();
	const float fsScale = 1.0f / static_cast<float>(metrics.ascenderY - metrics.descenderY);
	const char32_t lookup = iCodePoint == U'\t' ? U' ' : iCodePoint;
	const auto* glyph = m_data->fontGeometry.getGlyph(static_cast<msdfgen::unicode_t>(lookup));
	if (glyph == nullptr)
		glyph = m_data->fontGeometry.getGlyph('?');
	if (glyph == nullptr)
		return result;
	double pl = 0;
	double pb = 0;
	double pr = 0;
	double pt = 0;
	glyph->getQuadPlaneBounds(pl, pb, pr, pt);
	result.quad.min() = math::vec2{static_cast<float>(pl) * fsScale, static_cast<float>(pb) * fsScale};
	result.quad.max() = math::vec2{static_cast<float>(pr) * fsScale, static_cast<float>(pt) * fsScale};
	double al = 0;
	double ab = 0;
	double ar = 0;
	double at = 0;
	glyph->getQuadAtlasBounds(al, ab, ar, at);
	result.uv.min() = math::vec2{static_cast<float>(al / m_atlasTexture->getSize().x()),
								 static_cast<float>(ab / m_atlasTexture->getSize().y())};
	result.uv.max() = math::vec2{static_cast<float>(ar / m_atlasTexture->getSize().x()),
								 static_cast<float>(at / m_atlasTexture->getSize().y())};
	return result;
}

auto Font::getScaledLineHeight() const -> float {
	if (m_data == nullptr)
		return 0.f;
	const auto& metrics = m_data->fontGeometry.getMetrics();
	const float fsScale = 1.0f / static_cast<float>(metrics.ascenderY - metrics.descenderY);
	return fsScale * static_cast<float>(metrics.lineHeight);
}

auto Font::getAscender() const -> float {
	if (m_data == nullptr)
		return 0.f;
	const auto& metrics = m_data->fontGeometry.getMetrics();
	return static_cast<float>(metrics.ascenderY / (metrics.ascenderY - metrics.descenderY));
}

auto Font::getDescender() const -> float {
	if (m_data == nullptr)
		return 0.f;
	const auto& metrics = m_data->fontGeometry.getMetrics();
	return static_cast<float>(metrics.descenderY / (metrics.ascenderY - metrics.descenderY));
}

auto Font::getAdvance(const char32_t iCodePoint, const char32_t iNextCodePoint) const -> float {
	if (m_data == nullptr)
		return 0.f;
	if (m_data->denseAdvance && iCodePoint < MsdfData::denseCount && m_data->dense[iCodePoint].valid)
		return m_data->dense[iCodePoint].advance;
	const auto resolve = [this](const char32_t iCode) -> msdfgen::unicode_t {
		const auto code = static_cast<msdfgen::unicode_t>(iCode == U'\t' ? U' ' : iCode);
		return m_data->fontGeometry.getGlyph(code) != nullptr ? code : static_cast<msdfgen::unicode_t>('?');
	};
	const auto current = resolve(iCodePoint);
	const auto* glyph = m_data->fontGeometry.getGlyph(current);
	if (glyph == nullptr)
		return 0.f;
	double advance = glyph->getAdvance();
	std::ignore = m_data->fontGeometry.getAdvance(advance, current, resolve(iNextCodePoint));
	const auto& metrics = m_data->fontGeometry.getMetrics();
	const float fsScale = 1.0f / static_cast<float>(metrics.ascenderY - metrics.descenderY);
	return fsScale * static_cast<float>(advance);
}

auto Font::measureText(const std::string_view iText, const float iKerning, const float iLineSpacing) const
		-> math::box2f {
	math::box2f extents;
	forEachGlyph(iText, iKerning, iLineSpacing,
				 [&extents](const GlyphMetrics& iGlyph) -> void { extents.update(iGlyph.quad); });
	return extents;
}

}// namespace owl::data::fonts
