/**
 * @file AssetThumbnails.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <core/Core.h>
#include <data/geometry/StaticMesh.h>
#include <renderer/CameraEditor.h>
#include <renderer/gpu/Framebuffer.h>
#include <renderer/gpu/Texture.h>
#include <scene/Scene.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace owl::nest::panel {

/**
 * @brief
 *  Previews of the asset files shown by the Content Browser: images loaded as textures, scenes and prefabs rendered
 *  off-screen, sounds read as a waveform.
 *
 * Requests are cheap and never block: images and sounds are read a few per frame, scenes and prefabs are rendered one
 * at a time by `onUpdate` over a few frames (so their textures finish loading), into a pool of small framebuffers
 * that are reused, never freed while the editor runs (ImGui may still sample them).
 */
class AssetThumbnails final {
public:
	/// What a preview is made of.
	enum struct Kind : uint8_t {
		None,///< The file has no preview.
		Image,///< A texture of the image itself.
		Scene,///< A render through the primary camera of the scene.
		Prefab,///< A render of the prefab, framed by an orthographic camera.
		Hud,///< A render of the widgets of the HUD, in pixels, over the backdrop colour.
		Sound,///< The waveform of the sound.
		Tileset,///< The atlas image of the tileset.
		Mesh,///< The triangles of the mesh, projected in an isometric view and shaded.
	};

	/// A triangle of a mesh preview, in the unit square (y down), with its light level.
	struct MeshTriangle {
		/// Corners, from 0 to 1.
		std::array<math::vec2, 3> corners;
		/// Lambert light level, from 0 to 1.
		float shade = 1.f;
	};

	/// A finished preview.
	struct Preview {
		/// Image of the file (Image kind).
		shared<renderer::gpu::Texture2D> texture;
		/// Rendered frame (Scene and Prefab kinds); attachment 0 holds the colour.
		shared<renderer::gpu::Framebuffer> framebuffer;
		/// Peaks of the sound, 0 to 1 (Sound kind).
		std::vector<float> waveform;
		/// Triangles back to front (Mesh kind).
		std::vector<MeshTriangle> mesh;
	};

	/// Triangles kept in a mesh preview; a larger mesh is sampled.
	static constexpr size_t maxMeshTriangles = 6000;

	/**
	 * @brief
	 *  Project a mesh in an isometric view into the unit square, back to front, shaded by a light from the viewer.
	 * @param[in] iMesh The mesh.
	 * @return The triangles to draw in order; empty for an empty mesh.
	 */
	[[nodiscard]] static auto projectMesh(const data::geometry::StaticMesh& iMesh) -> std::vector<MeshTriangle>;

	/// Side of a rendered preview, in pixels.
	static constexpr uint32_t renderSize = 192;
	/// Rendered previews kept; the oldest framebuffer is reused beyond.
	static constexpr size_t maxRendered = 48;
	/// Frames a scene is rendered at least before its preview is final.
	static constexpr int renderFrames = 24;
	/// Seconds a scene is rendered at least before its preview is final (its textures load asynchronously).
	static constexpr float renderSeconds = 0.75f;
	/// Waveform slices of a sound preview.
	static constexpr size_t waveformBuckets = 48;

	AssetThumbnails(const AssetThumbnails&) = delete;

	AssetThumbnails(AssetThumbnails&&) = delete;

	auto operator=(const AssetThumbnails&) -> AssetThumbnails& = delete;

	auto operator=(AssetThumbnails&&) -> AssetThumbnails& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	AssetThumbnails();

	/**
	 * @brief
	 *  Destructor.
	 */
	~AssetThumbnails();

	/**
	 * @brief
	 *  Kind of preview a file gets, from its extension.
	 * @param[in] iPath The file.
	 * @return The kind.
	 */
	[[nodiscard]] static auto getKind(const std::filesystem::path& iPath) -> Kind;

	/**
	 * @brief
	 *  Preview of a file, or a request to make it.
	 * @param[in] iPath The file.
	 * @return The preview, or null while it is not ready (or the file has none).
	 */
	auto get(const std::filesystem::path& iPath) -> const Preview*;

	/**
	 * @brief
	 *  Start a new frame of requests: resets the per-frame read budget and the progress of the shown folder.
	 */
	void beginFrame();

	/**
	 * @brief
	 *  Render the pending scene or prefab, a frame at a time; call once per frame outside any render pass.
	 */
	void onUpdate();

	/**
	 * @brief
	 *  Previews requested this frame that are ready, and requested in all.
	 * @return Ready and total counts.
	 */
	[[nodiscard]] auto getProgress() const -> std::pair<size_t, size_t> {
		return {m_readyThisFrame, m_requestedThisFrame};
	}

	/**
	 * @brief
	 *  Drop every preview and the framebuffers; call while the graphics device is alive.
	 */
	void clear();

private:
	/// A preview and when its file was last written.
	struct Entry {
		/// The preview, empty while not made.
		std::optional<Preview> preview;
		/// Write time of the file the preview was made from.
		std::filesystem::file_time_type writeTime;
		/// Order of the last render, to find the oldest one to reuse.
		uint64_t renderOrder = 0;
		/// The file could not be previewed: never retried until it changes.
		bool failed = false;
	};

	/// The scene being rendered.
	struct Job {
		/// The file.
		std::filesystem::path path;
		/// The scene loaded from it.
		shared<scene::Scene> scene;
		/// Camera framing a prefab, or a scene without a primary camera.
		std::optional<renderer::CameraEditor> camera;
		/// Where it renders.
		shared<renderer::gpu::Framebuffer> framebuffer;
		/// Frames rendered so far.
		int frames = 0;
		/// When the first frame was rendered.
		std::chrono::steady_clock::time_point start;
	};

	/**
	 * @brief
	 *  Load the scene of the next pending file and pick its framebuffer.
	 * @return True when a job starts.
	 */
	auto startJob() -> bool;

	/**
	 * @brief
	 *  A framebuffer for a new render: a new one while the pool is small, else the one of the oldest render.
	 * @return The framebuffer.
	 */
	auto takeFramebuffer() -> shared<renderer::gpu::Framebuffer>;

	/// Previews by file path.
	std::unordered_map<std::string, Entry> m_entries;
	/// Scenes and prefabs waiting for a render, oldest request first.
	std::deque<std::filesystem::path> m_pending;
	/// The render in progress.
	std::optional<Job> m_job;
	/// Every framebuffer made, reused once there are `maxRendered`.
	std::vector<shared<renderer::gpu::Framebuffer>> m_framebuffers;
	/// Order given to the next render.
	uint64_t m_nextOrder = 1;
	/// Image or sound reads still allowed this frame.
	int m_readBudget = 0;
	/// Previews requested this frame.
	size_t m_requestedThisFrame = 0;
	/// Previews requested this frame that are ready.
	size_t m_readyThisFrame = 0;
};

}// namespace owl::nest::panel
