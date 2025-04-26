/**
 * @file Font.cpp
 * @author Silmaen
 * @date 10/17/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "fonts/Font.h"

#include "core/Application.h"

#undef INFINITE
#include <msdf-atlas-gen/msdf-atlas-gen.h>

namespace owl::fonts {

struct MsdfData {
	std::vector<msdf_atlas::GlyphGeometry> glyphs;
	msdf_atlas::FontGeometry fontGeometry;
};

namespace {
template<typename T, typename S, int N, msdf_atlas::GeneratorFunction<S, N> GenFunc>
auto createAndCacheAtlas([[maybe_unused]] const std::string& iFontName, [[maybe_unused]] float iFontSize,
						 const std::vector<msdf_atlas::GlyphGeometry>& iGlyphs,
						 [[maybe_unused]] const msdf_atlas::FontGeometry& iFontGeometry, math::vec2i iSize)
		-> shared<renderer::Texture2D> {
	msdf_atlas::GeneratorAttributes attributes;
	attributes.config.overlapSupport = true;
	attributes.scanlinePass = true;
	msdf_atlas::ImmediateAtlasGenerator<S, N, GenFunc, msdf_atlas::BitmapAtlasStorage<T, N>> generator(iSize.x(),
																									   iSize.y());
	generator.setAttributes(attributes);
	generator.setThreadCount(8);
	generator.generate(iGlyphs.data(), static_cast<int>(iGlyphs.size()));
	auto bitmap = static_cast<msdfgen::BitmapConstRef<T, N>>(generator.atlasStorage());
	const renderer::Texture::Specification spec{
			.size = {static_cast<uint32_t>(bitmap.width), static_cast<uint32_t>(bitmap.height)},
			.format = renderer::ImageFormat::Rgb8,
			.generateMips = false};
	shared<renderer::Texture2D> texture = renderer::Texture2D::create(spec);
	texture->setData(const_cast<uint8_t*>(bitmap.pixels), static_cast<uint32_t>(bitmap.width * bitmap.height * 3));
	return texture;
}
}// namespace

Font::Font(const std::filesystem::path& iPath, const bool iIsDefault) : m_default{iIsDefault} {
	OWL_SCOPE_UNTRACK

	if (!exists(iPath)) {
		OWL_CORE_ERROR("Font: Font file {} does not exists.", iPath.string())
		return;
	}
	msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
	if (ft == nullptr) {
		OWL_CORE_ERROR("Font: Failed to initialize Freetype library.")
		return;
	}
	msdfgen::FontHandle* font = loadFont(ft, iPath.string().c_str());
	if (font == nullptr) {
		OWL_CORE_ERROR("Font: Failed to load font: {}", iPath.string())
		return;
	}
	struct CharsetRange {
		uint32_t begin = 0, end = 0;
	};

	// from imgui_draw.cpp
	static constexpr CharsetRange charsetRanges[] = {{.begin = 0x0020, .end = 0x00FF}};
	msdf_atlas::Charset charset;
	for (auto [begin, end]: charsetRanges) {
		for (uint32_t c = begin; c <= end; c++) charset.add(c);
	}
	m_data = std::make_shared<MsdfData>();
	constexpr double fontScale = 1.0;
	m_data->fontGeometry = msdf_atlas::FontGeometry(&m_data->glyphs);
	int glyphsLoaded = m_data->fontGeometry.loadCharset(font, fontScale, charset);
	OWL_CORE_INFO("Font {}: Loaded {} glyphs from font (out of {})", iPath.filename().stem().string(), glyphsLoaded,
				  charset.size())
	double emSize = 40.0;
	msdf_atlas::TightAtlasPacker atlasPacker;
	// atlasPacker.setDimensionsConstraint()
	atlasPacker.setPixelRange(2.0);
	atlasPacker.setMiterLimit(1.0);
	atlasPacker.setScale(emSize);
	if (const int remaining = atlasPacker.pack(m_data->glyphs.data(), static_cast<int>(m_data->glyphs.size()));
		remaining != 0) {
		OWL_CORE_ERROR("Font loading: Failed Packing.")
	}
	math::vec2i size;
	atlasPacker.getDimensions(size.x(), size.y());
	emSize = atlasPacker.getScale();

#define DEFAULT_ANGLE_THRESHOLD 3.0
	constexpr uint64_t lcgMultiplier = 6364136223846793005;
	constexpr uint64_t lcgIncrement = 1442695040888963407;
#define THREAD_COUNT 8
	// if MSDF || MTSDF
	uint64_t coloringSeed = 0;
	if constexpr ((false)) {
		msdf_atlas::Workload(
				[&glyphs = m_data->glyphs, &coloringSeed](const int i, [[maybe_unused]] int iThreadNo) -> bool {
					const uint64_t glyphSeed =
							(lcgMultiplier * (coloringSeed ^ static_cast<uint64_t>(i)) + lcgIncrement) * !!coloringSeed;
					glyphs[static_cast<size_t>(i)].edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD,
																glyphSeed);
					return true;
				},
				static_cast<int>(m_data->glyphs.size()))
				.finish(THREAD_COUNT);
	} else {
		uint64_t glyphSeed = coloringSeed;
		for (msdf_atlas::GlyphGeometry& glyph: m_data->glyphs) {
			glyphSeed *= lcgMultiplier;
			glyph.edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
		}
	}
	m_atlasTexture = createAndCacheAtlas<uint8_t, float, 3, msdf_atlas::msdfGenerator>(
			"Test", static_cast<float>(emSize), m_data->glyphs, m_data->fontGeometry, size);
#if 0
	msdfgen::Shape shape;
	if (msdfgen::loadGlyph(shape, font, 'C'))
	{
		shape.normalize();
		//                      max. angle
		msdfgen::edgeColoringSimple(shape, 3.0);
		//           image width, height
		msdfgen::Bitmap<float, 3> msdf(32, 32);
		//                     range, scale, translation
		msdfgen::generateMSDF(msdf, shape, 4.0, 1.0, msdfgen::Vector2(4.0, 4.0));
		msdfgen::savePng(msdf, "output.png");
	}
#endif
	destroyFont(font);
	deinitializeFreetype(ft);
	m_name = iPath.stem().string();
}

Font::~Font() {
	if (m_atlasTexture) {
		m_atlasTexture.reset();
	}
}

auto Font::getGlyphBox(const char& iChar) const -> GlyphMetrics {
	GlyphMetrics result{.quad = {{0, 0}, {0, 0}}, .uv = {{0, 0}, {0, 0}}};
	const auto& metrics = m_data->fontGeometry.getMetrics();
	const float fsScale = 1.0f / static_cast<float>(metrics.ascenderY - metrics.descenderY);

	if (iChar == '\n') {
		return result;
	}
	const auto* glyph = m_data->fontGeometry.getGlyph(static_cast<msdfgen::unicode_t>(iChar));
	if (glyph == nullptr)
		glyph = m_data->fontGeometry.getGlyph('?');
	if (glyph == nullptr)
		return result;
	if (iChar == '\t')
		glyph = m_data->fontGeometry.getGlyph(' ');
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
	const auto& metrics = m_data->fontGeometry.getMetrics();
	const float fsScale = 1.0f / static_cast<float>(metrics.ascenderY - metrics.descenderY);
	return fsScale * static_cast<float>(metrics.lineHeight);
}

auto Font::getAdvance(const char& iChar, const char& iNextChar) const -> float {
	const auto ucode = static_cast<msdf_atlas::unicode_t>(iChar);
	const auto* glyph = m_data->fontGeometry.getGlyph(ucode);
	double advance = glyph->getAdvance();
	const auto nextCharacter = static_cast<msdfgen::unicode_t>(iNextChar);
	m_data->fontGeometry.getAdvance(advance, ucode, nextCharacter);
	const auto& metrics = m_data->fontGeometry.getMetrics();
	const float fsScale = 1.0f / static_cast<float>(metrics.ascenderY - metrics.descenderY);
	return fsScale * static_cast<float>(advance);
}

}// namespace owl::fonts
