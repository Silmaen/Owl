/**
 * @file FontPreviewCache.h
 * @author Silmaen
 * @date 26/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/fonts/Font.h"
#include "renderer/gpu/Framebuffer.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace owl::gui {
/**
 * @brief
 *  Per-font cache of rendered "sample-string" preview thumbnails.
 *
 * Each entry is a small offscreen `Framebuffer` containing the sample text rendered
 * once with the target font (via `renderer::Renderer2D::drawString`). Entries persist
 * for the lifetime of the cache; they are not invalidated by font reloads.
 *
 * Threading: main thread only. Renders are performed during `pumpPending()`, which must
 * be called from a place where the renderer is between scene passes (e.g. the start of
 * a layer's `onUpdate`). Reading entries via `request()` is non-blocking.
 */
class OWL_API FontPreviewCache final {
public:
	FontPreviewCache(const FontPreviewCache&) = delete;

	FontPreviewCache(FontPreviewCache&&) = delete;

	auto operator=(const FontPreviewCache&) -> FontPreviewCache& = delete;

	auto operator=(FontPreviewCache&&) -> FontPreviewCache& = delete;

	/**
	 * @brief
	 *  Get the process-wide cache instance.
	 * @return The singleton cache.
	 */
	[[nodiscard]] static auto get() -> FontPreviewCache&;

	/**
	 * @brief
	 *  Look up (or queue) the preview framebuffer for the given font.
	 * @param[in] iFont Source font.
	 * @return Framebuffer ready to be sampled when the entry has been rendered, or
	 *         `nullptr` on first request (the entry is queued and rendered by the next
	 *         `pumpPending()`). A `nullptr` font also returns `nullptr`.
	 */
	auto request(const shared<data::fonts::Font>& iFont) -> shared<renderer::gpu::Framebuffer>;

	/**
	 * @brief
	 *  Render every queued entry now. Must run between scene passes (no active
	 *        Renderer2D scene, no foreign framebuffer bound).
	 */
	void pumpPending();

	/**
	 * @brief
	 *  Drop every cached entry, releasing the underlying framebuffers.
	 */
	void clear();

private:
	/**
	 * @brief
	 *  Private constructor — go through `get()` for the singleton.
	 */
	FontPreviewCache() = default;

	/**
	 * @brief
	 *  Private destructor — singleton lifetime is process-wide.
	 */
	~FontPreviewCache() = default;

	/**
	 * @brief
	 *  One cached preview entry: framebuffer + readiness flag.
	 */
	struct Entry {
		/// Lazily allocated framebuffer.
		shared<renderer::gpu::Framebuffer> fb;
		/// False until `pumpPending` rendered the entry once.
		bool ready{false};
	};

	/// Map keyed by font name → cached preview entry.
	std::unordered_map<std::string, Entry> m_cache;
	/// Fonts queued by `request()` that still need to be drawn by `pumpPending()`.
	std::vector<shared<data::fonts::Font>> m_pending;
};

}// namespace owl::gui
