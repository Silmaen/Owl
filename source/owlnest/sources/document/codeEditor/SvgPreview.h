/**
 * @file SvgPreview.h
 * @author Silmaen
 * @date 28/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <renderer/gpu/Texture.h>
#include <core/Core.h>
#include <core/Timestep.h>
#include <math/vectors.h>

#include <string>

struct ImVec2;

namespace owl::nest::codeEditor {

/**
 * @brief Live SVG renderer for the code editor preview pane.
 *
 * Parses the input SVG via lunasvg, rasterizes to a `Texture2D` and exposes
 * the texture for `ImGui::Image`. The rasterization is debounced (~250 ms)
 * and re-runs when either the source text or the requested target size
 * changes.
 *
 * Failures (parse errors, lunasvg returning a null document) are surfaced
 * via `isParseOk()` so the host can render a friendly error message instead
 * of a stale image.
 */
class SvgPreview final {
public:
	/// @brief Default constructor.
	SvgPreview();
	/// @brief Destructor.
	~SvgPreview();
	SvgPreview(const SvgPreview&) = delete;
	SvgPreview(SvgPreview&&) = delete;
	auto operator=(const SvgPreview&) -> SvgPreview& = delete;
	auto operator=(SvgPreview&&) -> SvgPreview& = delete;

	/**
	 * @brief Feed a new text frame. Re-rasterization is debounced.
	 * @param[in] iTimeStep The current frame timestep.
	 * @param[in] iText The SVG/XML source.
	 * @param[in] iTargetSize Desired raster size in pixels (square is enforced via the smaller side).
	 */
	void update(const core::Timestep& iTimeStep, const std::string& iText, math::vec2ui iTargetSize);

	/**
	 * @brief Render the latest rasterized SVG inside the current ImGui window/child.
	 * @param[in] iSize Target display size.
	 */
	void render(const ImVec2& iSize) const;

	/// @brief True if the latest non-empty parse succeeded.
	[[nodiscard]] auto isParseOk() const -> bool { return m_lastParseOk; }

private:
	/// Maximum side length for the raster (capped to avoid OOM on huge canvases).
	static constexpr uint32_t kMaxSide = 2048;
	/// Debounce window (seconds).
	static constexpr float kDebounceSeconds = 0.25f;

	/// Re-rasterize from `m_pendingText` into `m_texture`. Updates `m_lastParseOk`.
	void rasterize();

	/// Last input text seen by `update`.
	std::string m_pendingText;
	/// Text effectively rasterized into the GPU texture.
	std::string m_renderedText;
	/// Last requested target size.
	math::vec2ui m_pendingSize{0, 0};
	/// Last rasterized texture size.
	math::vec2ui m_textureSize{0, 0};
	/// GPU texture holding the rasterized image.
	shared<renderer::gpu::Texture2D> m_texture;
	/// Seconds remaining before the next rasterization.
	float m_debounceLeft = 0.0f;
	/// True while waiting for the debounce window to expire.
	bool m_dirty = false;
	/// True if the last parse succeeded.
	bool m_lastParseOk = true;
};

}// namespace owl::nest::codeEditor
