/**
 * @file IconBank.cpp
 * @author Silmaen
 * @date 10/03/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/IconBank.h"
#include "gui/utils.h"

#include <span>
#include <stb_image.h>

namespace owl::gui {

namespace {

/// @brief RAII wrapper for stbi-allocated pixel data.
struct StbImageDeleter {
	void operator()(uint8_t* iPtr) const {
		if (iPtr != nullptr)
			stbi_image_free(iPtr);
	}
};
using StbImagePtr = std::unique_ptr<uint8_t[], StbImageDeleter>;

/**
 * @brief Load an image file via stb_image and return owned pixel data.
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
 * @brief Simple box-filter downscale of an RGBA image.
 * @param[in] iSrc Source pixel data (RGBA, 4 bytes per pixel).
 * @param[in] iSrcSize Source dimensions (width, height).
 * @param[out] oDst Destination pixel buffer.
 * @param[in] iDstSize Destination dimensions (width, height).
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
 * @brief Simple nearest-neighbor upscale of an RGBA image.
 * @param[in] iSrc Source pixel data (RGBA, 4 bytes per pixel).
 * @param[in] iSrcSize Source dimensions (width, height).
 * @param[out] oDst Destination pixel buffer.
 * @param[in] iDstSize Destination dimensions (width, height).
 */
void nearestUpscale(std::span<const uint8_t> iSrc, const math::vec2ui iSrcSize, std::span<uint8_t> oDst,
					const math::vec2ui iDstSize) {
	for (uint32_t dy = 0; dy < iDstSize.y(); ++dy) {
		for (uint32_t dx = 0; dx < iDstSize.x(); ++dx) {
			const math::vec2ui src{dx * iSrcSize.x() / iDstSize.x(), dy * iSrcSize.y() / iDstSize.y()};
			const uint32_t srcIdx = (src.y() * iSrcSize.x() + src.x()) * 4;
			const uint32_t dstIdx = (dy * iDstSize.x() + dx) * 4;
			oDst[dstIdx + 0] = iSrc[srcIdx + 0];
			oDst[dstIdx + 1] = iSrc[srcIdx + 1];
			oDst[dstIdx + 2] = iSrc[srcIdx + 2];
			oDst[dstIdx + 3] = iSrc[srcIdx + 3];
		}
	}
}

}// namespace

void IconBank::build(const std::vector<std::pair<std::string, std::filesystem::path>>& iIcons,
					 const uint32_t iCellSize) {
	clear();

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

		math::vec2ui srcSize{};
		auto pixels = loadImageFile(filePath, srcSize);
		if (pixels.empty()) {
			OWL_CORE_WARN("IconBank: failed to load icon: {}", filePath.string())
			++index;
			continue;
		}

		// Resize to cell size if needed
		if (srcSize != cellSize) {
			std::vector<uint8_t> resized(static_cast<size_t>(cellSize.x()) * cellSize.y() * 4);
			if (srcSize.x() > cellSize.x() || srcSize.y() > cellSize.y()) {
				boxFilterDownscale(pixels, srcSize, resized, cellSize);
			} else {
				nearestUpscale(pixels, srcSize, resized, cellSize);
			}
			pixels = std::move(resized);
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

		// Store UVs — Y-flip for engine convention (stb loads top-down, OpenGL is bottom-up)
		m_uvMap[name] = {{uvMin.x(), uvMax.y()}, {uvMax.x(), uvMin.y()}};

		++index;
	}

	// Create atlas texture
	renderer::Texture::Specification spec;
	spec.size = atlasSize;
	spec.format = renderer::ImageFormat::Rgba8;
	spec.generateMips = false;
	m_atlas = renderer::Texture2D::create(spec);
	if (m_atlas != nullptr) {
		m_atlas->setData(atlasData.data(), static_cast<uint32_t>(atlasData.size()));
	}
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

}// namespace owl::gui
