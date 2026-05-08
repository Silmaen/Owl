/**
 * @file IconBank.cpp
 * @author Silmaen
 * @date 10/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/IconBank.h"
#include "gui/utils.h"

#include "core/external/lunasvg.h"

#include <fstream>
#include <span>
#include <stb_image.h>

namespace owl::gui {

namespace {
/**
 * @brief
 *  RAII wrapper for stbi-allocated pixel data.
 */
struct StbImageDeleter {
	void operator()(uint8_t* iPtr) const {
		if (iPtr != nullptr)
			stbi_image_free(iPtr);
	}
};
using StbImagePtr = std::unique_ptr<uint8_t[], StbImageDeleter>;

/**
 * @brief
 *  Load an image file via stb_image and return owned pixel data.
 * @param[in] iPath The file path.
 * @param[out] oSize The loaded image dimensions (width, height).
 * @return Owned pixel buffer (RGBA, 4 bytes per pixel), or empty vector on failure.
 */
auto loadImageFile(const std::filesystem::path& iPath, math::vec2ui& oSize) -> std::vector<uint8_t> {
	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_set_flip_vertically_on_load(1);// Flip for OpenGL bottom-up convention
	const StbImagePtr raw{stbi_load(iPath.string().c_str(), &width, &height, &channels, 4)};
	if (raw == nullptr)
		return {};
	oSize = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
	const auto byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
	return {raw.get(), raw.get() + byteCount};
}

/**
 * @brief
 *  Read a file as a string.
 * @param[in] iPath The file path.
 * @return The file content, or empty string on failure.
 */
auto readFileAsString(const std::filesystem::path& iPath) -> std::string {
	std::ifstream file(iPath, std::ios::binary);
	if (!file.is_open())
		return {};
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

/**
 * @brief
 *  Convert a math::vec4 colour (0-1 floats) to a hex string like "#rrggbb".
 * @param[in] iColor The colour.
 * @return The hex colour string.
 */
auto colorToHex(const math::vec4& iColor) -> std::string {
	const auto r = static_cast<uint8_t>(std::clamp(iColor.x(), 0.f, 1.f) * 255.f);
	const auto g = static_cast<uint8_t>(std::clamp(iColor.y(), 0.f, 1.f) * 255.f);
	const auto b = static_cast<uint8_t>(std::clamp(iColor.z(), 0.f, 1.f) * 255.f);
	return std::format("#{:02x}{:02x}{:02x}", r, g, b);
}

/**
 * @brief
 *  Case-insensitive replace all occurrences of a hex colour in a string.
 * @param[in,out] ioStr The string to modify.
 * @param[in] iFrom The colour to find (lowercase, e.g., "#ffffff").
 * @param[in] iTo The replacement string.
 */
void replaceColorInPlace(std::string& ioStr, const std::string& iFrom, const std::string& iTo) {
	if (iFrom == iTo)
		return;
	// Build uppercase variant for case-insensitive matching.
	std::string fromUpper = iFrom;
	for (auto& ch: fromUpper) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
	std::size_t pos = 0;
	while (pos < ioStr.size()) {
		// Try lowercase match.
		if (const auto found = ioStr.find(iFrom, pos); found != std::string::npos) {
			ioStr.replace(found, iFrom.size(), iTo);
			pos = found + iTo.size();
			continue;
		}
		// Try uppercase match.
		if (const auto found = ioStr.find(fromUpper, pos); found != std::string::npos) {
			ioStr.replace(found, fromUpper.size(), iTo);
			pos = found + iTo.size();
			continue;
		}
		break;
	}
}

/**
 * @brief
 *  Load an SVG file, apply theme colour substitution, rasterize to RGBA pixels.
 * @param[in] iPath The SVG file path.
 * @param[in] iTargetSize Target pixel size (square).
 * @param[in] iColors Theme colours for substitution.
 * @return Owned pixel buffer (RGBA straight alpha, 4 bytes per pixel), or empty vector on failure.
 */
auto loadSvgFile(const std::filesystem::path& iPath, const uint32_t iTargetSize,
				 const IconThemeColors& iColors) -> std::vector<uint8_t> {
	auto svgContent = readFileAsString(iPath);
	if (svgContent.empty())
		return {};
	// Apply theme colour substitution in memory (never modifies the file).
	replaceColorInPlace(svgContent, "#ffffff", colorToHex(iColors.primary));
	replaceColorInPlace(svgContent, "#ff00ff", colorToHex(iColors.secondary));
	// Rasterize via lunasvg.
	const auto doc = lunasvg::Document::loadFromData(svgContent);
	if (!doc)
		return {};
	const auto bitmap = doc->renderToBitmap(static_cast<int>(iTargetSize), static_cast<int>(iTargetSize));
	if (bitmap.isNull())
		return {};
	// Convert from ARGB premultiplied (lunasvg) to RGBA straight alpha.
	// Also flip vertically (lunasvg is top-down, OpenGL atlas expects bottom-up).
	const auto pixelCount = static_cast<size_t>(iTargetSize) * iTargetSize;
	std::vector<uint8_t> pixels(pixelCount * 4);
	const auto* src = bitmap.data();
	for (uint32_t y = 0; y < iTargetSize; ++y) {
		const uint32_t flippedY = iTargetSize - 1 - y;
		for (uint32_t x = 0; x < iTargetSize; ++x) {
			const size_t srcIdx = (static_cast<size_t>(y) * iTargetSize + x) * 4;
			const size_t dstIdx = (static_cast<size_t>(flippedY) * iTargetSize + x) * 4;
			const uint8_t a = src[srcIdx + 3];
			if (a == 0) {
				pixels[dstIdx + 0] = 0;
				pixels[dstIdx + 1] = 0;
				pixels[dstIdx + 2] = 0;
				pixels[dstIdx + 3] = 0;
			} else {
				// Unpremultiply and swizzle BGRA → RGBA.
				pixels[dstIdx + 0] = static_cast<uint8_t>(static_cast<uint16_t>(src[srcIdx + 2]) * 255 / a);
				pixels[dstIdx + 1] = static_cast<uint8_t>(static_cast<uint16_t>(src[srcIdx + 1]) * 255 / a);
				pixels[dstIdx + 2] = static_cast<uint8_t>(static_cast<uint16_t>(src[srcIdx + 0]) * 255 / a);
				pixels[dstIdx + 3] = a;
			}
		}
	}
	return pixels;
}

/**
 * @brief
 *  Simple box-filter downscale of an RGBA image.
 */
void boxFilterDownscale(std::span<const uint8_t> iSrc, const math::vec2ui iSrcSize, std::span<uint8_t> oDst,
						const math::vec2ui iDstSize) {
	for (uint32_t dy = 0; dy < iDstSize.y(); ++dy) {
		for (uint32_t dx = 0; dx < iDstSize.x(); ++dx) {
			const math::vec2ui srcMin{dx * iSrcSize.x() / iDstSize.x(), dy * iSrcSize.y() / iDstSize.y()};
			const math::vec2ui srcMax{(dx + 1) * iSrcSize.x() / iDstSize.x(),
									  (dy + 1) * iSrcSize.y() / iDstSize.y()};
			math::vec4ui accum{0, 0, 0, 0};
			uint32_t count = 0;
			for (uint32_t sy = srcMin.y(); sy < srcMax.y(); ++sy) {
				for (uint32_t sx = srcMin.x(); sx < srcMax.x(); ++sx) {
					const uint32_t idx = (sy * iSrcSize.x() + sx) * 4;
					accum += {iSrc[idx + 0], iSrc[idx + 1], iSrc[idx + 2], iSrc[idx + 3]};
					++count;
				}
			}
			if (count == 0)
				count = 1;
			const uint32_t dstIdx = (dy * iDstSize.x() + dx) * 4;
			const auto result = accum / count;
			oDst[dstIdx + 0] = static_cast<uint8_t>(result.x());
			oDst[dstIdx + 1] = static_cast<uint8_t>(result.y());
			oDst[dstIdx + 2] = static_cast<uint8_t>(result.z());
			oDst[dstIdx + 3] = static_cast<uint8_t>(result.w());
		}
	}
}

/**
 * @brief
 *  Bilinear upscale of an RGBA image.
 */
void bilinearUpscale(std::span<const uint8_t> iSrc, const math::vec2ui iSrcSize, std::span<uint8_t> oDst,
					 const math::vec2ui iDstSize) {
	const float scaleX = static_cast<float>(iSrcSize.x()) / static_cast<float>(iDstSize.x());
	const float scaleY = static_cast<float>(iSrcSize.y()) / static_cast<float>(iDstSize.y());
	for (uint32_t dy = 0; dy < iDstSize.y(); ++dy) {
		for (uint32_t dx = 0; dx < iDstSize.x(); ++dx) {
			const float srcX = (static_cast<float>(dx) + 0.5f) * scaleX - 0.5f;
			const float srcY = (static_cast<float>(dy) + 0.5f) * scaleY - 0.5f;
			const auto x0 = static_cast<uint32_t>(std::max(0.f, std::floor(srcX)));
			const auto y0 = static_cast<uint32_t>(std::max(0.f, std::floor(srcY)));
			const uint32_t x1 = std::min(x0 + 1, iSrcSize.x() - 1);
			const uint32_t y1 = std::min(y0 + 1, iSrcSize.y() - 1);
			const float fx = srcX - static_cast<float>(x0);
			const float fy = srcY - static_cast<float>(y0);
			const uint32_t dstIdx = (dy * iDstSize.x() + dx) * 4;
			for (uint32_t c = 0; c < 4; ++c) {
				const auto v00 = static_cast<float>(iSrc[(y0 * iSrcSize.x() + x0) * 4 + c]);
				const auto v10 = static_cast<float>(iSrc[(y0 * iSrcSize.x() + x1) * 4 + c]);
				const auto v01 = static_cast<float>(iSrc[(y1 * iSrcSize.x() + x0) * 4 + c]);
				const auto v11 = static_cast<float>(iSrc[(y1 * iSrcSize.x() + x1) * 4 + c]);
				const float val =
						v00 * (1 - fx) * (1 - fy) + v10 * fx * (1 - fy) + v01 * (1 - fx) * fy + v11 * fx * fy;
				oDst[dstIdx + c] = static_cast<uint8_t>(std::clamp(val, 0.f, 255.f));
			}
		}
	}
}

}// namespace

void IconBank::build(const std::vector<std::pair<std::string, std::filesystem::path>>& iIcons,
					 const uint32_t iCellSize, const IconThemeColors& iColors) {
	clear();

	// Store for rebuild.
	m_iconList = iIcons;
	m_cellSize = iCellSize;
	if (iIcons.empty())
		return;
	// Determine grid dimensions
	const auto iconCount = static_cast<uint32_t>(iIcons.size());
	const auto cols = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(iconCount))));
	const uint32_t rows = (iconCount + cols - 1) / cols;
	const math::vec2ui cellSize{iCellSize, iCellSize};
	const math::vec2ui atlasSize{cols * iCellSize, rows * iCellSize};
	// Allocate atlas buffer (RGBA, zero-initialized for transparency)
	std::vector<uint8_t> atlasData(static_cast<size_t>(atlasSize.x()) * atlasSize.y() * 4, 0);
	// Load and pack each icon
	uint32_t index = 0;
	for (const auto& [name, filePath]: iIcons) {
		if (!std::filesystem::exists(filePath)) {
			OWL_CORE_WARN("IconBank: icon file not found: {}", filePath.string())
			++index;
			continue;
		}

		std::vector<uint8_t> pixels;
		const bool isSvg = filePath.extension() == ".svg";
		if (isSvg) {
			// Load SVG: rasterize at exact cell size with theme colours.
			pixels = loadSvgFile(filePath, iCellSize, iColors);
		} else {
			// Load raster image (PNG/JPG) via stb_image.
			math::vec2ui srcSize{};
			pixels = loadImageFile(filePath, srcSize);
			// Resize to cell size if needed.
			if (!pixels.empty() && srcSize != cellSize) {
				std::vector<uint8_t> resized(static_cast<size_t>(cellSize.x()) * cellSize.y() * 4);
				if (srcSize.x() > cellSize.x() || srcSize.y() > cellSize.y()) {
					boxFilterDownscale(pixels, srcSize, resized, cellSize);
				} else {
					bilinearUpscale(pixels, srcSize, resized, cellSize);
				}
				pixels = std::move(resized);
			}
		}

		if (pixels.empty()) {
			OWL_CORE_WARN("IconBank: failed to load icon: {}", filePath.string())
			++index;
			continue;
		}

		// Blit into atlas
		const math::vec2ui cell{index % cols, index / cols};
		const math::vec2ui cellOrigin{cell.x() * cellSize.x(), cell.y() * cellSize.y()};
		for (uint32_t y = 0; y < cellSize.y(); ++y) {
			const auto atlasY = cellOrigin.y() + y;
			const auto dstOffset = (static_cast<size_t>(atlasY) * atlasSize.x() + cellOrigin.x()) * 4;
			const auto srcOffset = static_cast<size_t>(y) * cellSize.x() * 4;
			const auto rowBytes = static_cast<size_t>(cellSize.x()) * 4;

			std::copy_n(pixels.cbegin() + static_cast<ptrdiff_t>(srcOffset), rowBytes,
						atlasData.begin() + static_cast<ptrdiff_t>(dstOffset));
		}

		// Compute UV coordinates
		const math::vec2 atlasF{static_cast<float>(atlasSize.x()), static_cast<float>(atlasSize.y())};
		const math::vec2 uvMin{static_cast<float>(cellOrigin.x()) / atlasF.x(),
							   static_cast<float>(cellOrigin.y()) / atlasF.y()};
		const math::vec2 uvMax{static_cast<float>(cellOrigin.x() + cellSize.x()) / atlasF.x(),
							   static_cast<float>(cellOrigin.y() + cellSize.y()) / atlasF.y()};
		// Store UVs — Y-flip for engine convention (top-down image data, bottom-up OpenGL)
		m_uvMap[name] = {{uvMin.x(), uvMax.y()}, {uvMax.x(), uvMin.y()}};
		++index;
	}
	// Create atlas texture
	renderer::gpu::Texture::Specification spec;
	spec.size = atlasSize;
	spec.format = renderer::gpu::ImageFormat::Rgba8;
	spec.generateMips = true;
	m_atlas = renderer::gpu::Texture2D::create(spec);
	if (m_atlas != nullptr) {
		m_atlas->setData(atlasData.data(), static_cast<uint32_t>(atlasData.size()));
	}
}

void IconBank::rebuild(const IconThemeColors& iColors) {
	if (m_iconList.empty())
		return;
	build(m_iconList, m_cellSize, iColors);
}

auto IconBank::getIcon(const std::string& iName) const -> std::optional<IconInfo> {
	const auto it = m_uvMap.find(iName);
	if (it == m_uvMap.end())
		return std::nullopt;
	if (m_atlas == nullptr)
		return std::nullopt;

	const auto texId = m_atlas->getRendererId();
	if (texId == 0)
		return std::nullopt;

	return IconInfo{.textureId = texId, .uv0 = it->second.first, .uv1 = it->second.second};
}

auto IconBank::hasIcon(const std::string& iName) const -> bool { return m_uvMap.contains(iName); }

void IconBank::clear() {
	m_atlas.reset();
	m_uvMap.clear();
}

auto IconBank::instance() -> IconBank& {
	static IconBank s_instance;
	return s_instance;
}

auto IconBank::menuItem(const std::string& iIconName, const char* iLabel, const char* iShortcut,
						const bool iEnabled) const -> bool {
	if (const auto info = getIcon(iIconName); info.has_value()) {
		constexpr float iconSize = 16.0f;
		if (!iEnabled) {
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().DisabledAlpha);
		}
		ImGui::Image(static_cast<ImTextureID>(info->textureId), {iconSize, iconSize}, vec(info->uv0), vec(info->uv1));
		if (!iEnabled) {
			ImGui::PopStyleVar();
		}
		ImGui::SameLine();
	}
	return ImGui::MenuItem(iLabel, iShortcut, false, iEnabled);
}

auto IconBank::iconButton(const std::string& iIconName, const char* iLabel, const math::vec2& iSize) const -> bool {
	const auto info = getIcon(iIconName);
	if (!info.has_value())
		return ImGui::Button(iLabel, vec(iSize));

	const float iconSize = ImGui::GetFontSize();
	const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
	const ImVec2 labelSize = ImGui::CalcTextSize(iLabel, nullptr, true);
	const ImVec2 framePad = ImGui::GetStyle().FramePadding;

	ImVec2 size{iSize.x(), iSize.y()};
	if (size.x <= 0.f)
		size.x = iconSize + spacing + labelSize.x + framePad.x * 2.f;
	if (size.y <= 0.f)
		size.y = std::max(iconSize, labelSize.y) + framePad.y * 2.f;

	ImGui::PushID(iLabel);
	const bool clicked = ImGui::Button("##iconbtn", size);
	const ImVec2 btnMin = ImGui::GetItemRectMin();
	const ImVec2 btnMax = ImGui::GetItemRectMax();
	auto* drawList = ImGui::GetWindowDrawList();

	const float contentW = iconSize + spacing + labelSize.x;
	const float startX = btnMin.x + ((btnMax.x - btnMin.x) - contentW) * 0.5f;
	const float centerY = (btnMin.y + btnMax.y) * 0.5f;

	const ImVec2 iconMin{startX, centerY - iconSize * 0.5f};
	const ImVec2 iconMax{startX + iconSize, centerY + iconSize * 0.5f};
	drawList->AddImage(static_cast<ImTextureID>(info->textureId), iconMin, iconMax, vec(info->uv0), vec(info->uv1));

	const ImVec2 textPos{startX + iconSize + spacing, centerY - labelSize.y * 0.5f};
	drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), iLabel);
	ImGui::PopID();
	return clicked;
}

}// namespace owl::gui
