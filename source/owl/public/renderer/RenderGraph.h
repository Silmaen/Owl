/**
 * @file RenderGraph.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "gpu/Framebuffer.h"
#include "gpu/Texture.h"
#include "math/vectors.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::renderer {

/**
 * @brief
 *  Frame graph of render passes: each pass declares the targets it reads and writes, the graph derives the order,
 *  drops the passes nothing uses, allocates the transient targets and binds the right framebuffer around each pass.
 *
 * Every target has the engine attachment layout (colour, `RedInteger` entity id, depth). A transient target is
 * linear (`Rgba16F` colour, the HDR scene colour), display (`Surface` colour) or depth (`RedFloat`, a shadow map),
 * sized as the frame unless given a size; it is cleared on its first write of a frame. The imported target is the framebuffer bound when `execute` runs (or the
 * window surface), bound again when the graph returns. A pass reading a target runs after every pass declared
 * before it that writes the target; reading a colour attachment unbinds its framebuffer first, which turns it
 * shader readable.
 */
class OWL_API RenderGraph {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	RenderGraph() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~RenderGraph() = default;

	/**
	 * @brief
	 *  Copy constructor (deleted: the graph owns GPU targets).
	 */
	RenderGraph(const RenderGraph&) = delete;

	/**
	 * @brief
	 *  Move constructor.
	 */
	RenderGraph(RenderGraph&&) = default;

	/**
	 * @brief
	 *  Copy assignment (deleted).
	 * @return This graph.
	 */
	auto operator=(const RenderGraph&) -> RenderGraph& = delete;

	/**
	 * @brief
	 *  Move assignment.
	 * @return This graph.
	 */
	auto operator=(RenderGraph&&) -> RenderGraph& = default;

	/**
	 * @brief
	 *  Colour attachment of a transient target.
	 */
	enum struct TargetFormat : uint8_t {
		Linear,///< `Rgba16F` colour: linear values, beyond 1 allowed.
		Display,///< `Surface` colour: display-encoded values.
		Depth,///< `RedFloat` colour holding a depth (shadow maps), cleared to 1, the far plane.
	};

	/**
	 * @brief
	 *  Handle of a target of the graph.
	 */
	struct Resource {
		/// Index of the target, `invalid` when unset.
		uint32_t index = std::numeric_limits<uint32_t>::max();

		/**
		 * @brief
		 *  Check the handle names a target.
		 * @return True when set.
		 */
		[[nodiscard]] auto isValid() const -> bool { return index != std::numeric_limits<uint32_t>::max(); }
	};

	/**
	 * @brief
	 *  What a pass declares while it is added.
	 */
	class OWL_API PassBuilder {
	public:
		/**
		 * @brief
		 *  Declare a target the pass samples or copies from.
		 * @param[in] iResource The target.
		 */
		void read(Resource iResource);

		/**
		 * @brief
		 *  Declare the target the pass draws into (one per pass, the last call wins).
		 * @param[in] iResource The target.
		 */
		void write(Resource iResource);

	private:
		friend class RenderGraph;

		/**
		 * @brief
		 *  Builder filling a pass of a graph.
		 * @param[in,out] ioGraph The graph.
		 * @param[in] iPass Index of the pass.
		 */
		PassBuilder(RenderGraph& ioGraph, uint32_t iPass) : m_graph{ioGraph}, m_pass{iPass} {}

		/// The graph.
		RenderGraph& m_graph;
		/// Index of the pass.
		uint32_t m_pass;
	};

	/**
	 * @brief
	 *  What a pass reaches while it runs.
	 */
	class OWL_API PassContext {
	public:
		/**
		 * @brief
		 *  The framebuffer of a target in this frame.
		 * @param[in] iResource The target.
		 * @return The framebuffer, null for the window surface or an unset target.
		 */
		[[nodiscard]] auto getFramebuffer(Resource iResource) const -> gpu::Framebuffer*;

		/**
		 * @brief
		 *  A colour attachment of a target as a texture to sample.
		 * @param[in] iResource The target, declared read by the pass.
		 * @param[in] iAttachment Index of the colour attachment.
		 * @return The texture, null when the target cannot be sampled.
		 */
		[[nodiscard]] auto getTexture(Resource iResource, uint32_t iAttachment = 0) const -> shared<gpu::Texture2D>;

		/**
		 * @brief
		 *  Size of the frame.
		 * @return The size, in pixels.
		 */
		[[nodiscard]] auto getSize() const -> const math::vec2ui&;

	private:
		friend class RenderGraph;

		/**
		 * @brief
		 *  Context of a graph run.
		 * @param[in] iGraph The graph.
		 */
		explicit PassContext(const RenderGraph& iGraph) : m_graph{iGraph} {}

		/// The graph.
		const RenderGraph& m_graph;
	};

	/// Setup of a pass kept in a variable: declares its reads and its write.
	using Setup = std::function<void(PassBuilder&)>;
	/// Body of a pass, run with its target bound.
	using Execute = std::function<void(const PassContext&)>;

	/**
	 * @brief
	 *  Remove the passes and the targets of the last frame; the transient framebuffers stay pooled.
	 */
	void reset();

	/**
	 * @brief
	 *  Declare the framebuffer bound when `execute` runs as a target.
	 * @param[in] iName Name of the target.
	 * @return The target.
	 */
	auto importTarget(std::string_view iName) -> Resource;

	/**
	 * @brief
	 *  Declare a transient target, allocated when a kept pass uses it.
	 * @param[in] iName Name of the target.
	 * @param[in] iFormat Format of its colour attachment.
	 * @param[in] iSize Size of the target; zero takes the size of the frame.
	 * @return The target.
	 */
	auto createTarget(std::string_view iName, TargetFormat iFormat, const math::vec2ui& iSize = {0, 0}) -> Resource;

	/**
	 * @brief
	 *  Add a pass; `iSetup` runs right away (it is not stored).
	 * @tparam SetupFn Callable taking a `PassBuilder&`.
	 * @param[in] iName Name of the pass (profiling, inspection).
	 * @param[in] iSetup Declares the reads and the write.
	 * @param[in] iExecute Draws, with the written target bound.
	 */
	template<typename SetupFn>
	void addPass(const std::string_view iName, SetupFn&& iSetup, Execute iExecute) {
		PassBuilder builder{*this, beginPass(iName, std::move(iExecute))};
		std::forward<SetupFn>(iSetup)(builder);
	}

	/**
	 * @brief
	 *  Order the passes and drop those whose target nothing uses.
	 * @return False when a pass reads a target no earlier pass writes, or writes none.
	 */
	auto compile() -> bool;

	/**
	 * @brief
	 *  Run the compiled passes.
	 * @param[in] iSize Size of the frame, the size of the transient targets.
	 */
	void execute(const math::vec2ui& iSize);

	/**
	 * @brief
	 *  Names of the passes `compile` kept, in their order.
	 * @return The names.
	 */
	[[nodiscard]] auto getCompiledPasses() const -> std::vector<std::string>;

	/**
	 * @brief
	 *  Number of transient framebuffers held by the pool.
	 * @return The count.
	 */
	[[nodiscard]] auto getPooledTargetCount() const -> size_t { return m_pool.size(); }

	/**
	 * @brief
	 *  Release the pooled framebuffers (before the device goes away).
	 */
	void releasePool() { m_pool.clear(); }

private:
	/// A target of the frame.
	struct Target {
		/// Name of the target.
		std::string name;
		/// Whether it is the framebuffer bound when `execute` runs.
		bool imported = false;
		/// Format of a transient target.
		TargetFormat format = TargetFormat::Linear;
		/// Size of a transient target, zero for the frame size.
		math::vec2ui size{0, 0};
		/// Framebuffer of the frame (null for the window surface).
		gpu::Framebuffer* framebuffer = nullptr;
		/// Whether the target was written in this frame.
		bool written = false;
		/// Last kept pass using the target.
		uint32_t lastUse = 0;
	};

	/// A pass of the frame.
	struct Pass {
		/// Name of the pass.
		std::string name;
		/// Targets read.
		std::vector<uint32_t> reads;
		/// Target written.
		Resource write;
		/// Body.
		Execute execute;
	};

	/// A transient framebuffer kept from frame to frame.
	struct PooledTarget {
		/// The framebuffer.
		shared<gpu::Framebuffer> framebuffer;
		/// Its format.
		TargetFormat format = TargetFormat::Linear;
		/// Whether a target of the running frame holds it.
		bool used = false;
		/// Runs of `execute` since it was last used.
		uint32_t idleRuns = 0;
	};

	/**
	 * @brief
	 *  Open the next pass slot (the slots of the last frame are reused, with their capacity).
	 * @param[in] iName Name of the pass.
	 * @param[in] iExecute Body of the pass.
	 * @return Index of the pass.
	 */
	auto beginPass(std::string_view iName, Execute iExecute) -> uint32_t;

	/**
	 * @brief
	 *  Declare a target in the next free slot (the slots of the last frame are reused, with their capacity).
	 * @param[in] iName Name of the target.
	 * @param[in] iImported Whether it is the bound framebuffer.
	 * @param[in] iFormat Format of a transient target.
	 * @param[in] iSize Size of a transient target, zero for the frame size.
	 * @return The target.
	 */
	auto addTarget(std::string_view iName, bool iImported, TargetFormat iFormat, const math::vec2ui& iSize) -> Resource;

	/**
	 * @brief
	 *  Take a pooled framebuffer of a format and size, or create one.
	 * @param[in] iFormat The format.
	 * @param[in] iSize The size.
	 * @return The framebuffer.
	 */
	auto acquire(TargetFormat iFormat, const math::vec2ui& iSize) -> gpu::Framebuffer*;

	/**
	 * @brief
	 *  Hand a framebuffer back to the pool for the next targets of the frame.
	 * @param[in] iFramebuffer The framebuffer.
	 */
	void release(const gpu::Framebuffer* iFramebuffer);

	/// Targets, the first `m_targetCount` belong to the frame.
	std::vector<Target> m_targets;
	/// Number of targets of the frame.
	uint32_t m_targetCount = 0;
	/// Passes in declaration order, the first `m_passCount` belong to the frame.
	std::vector<Pass> m_passes;
	/// Number of passes of the frame.
	uint32_t m_passCount = 0;
	/// Indices of the kept passes, in run order.
	std::vector<uint32_t> m_order;
	/// Transient framebuffers.
	std::vector<PooledTarget> m_pool;
	/// Size of the running frame.
	math::vec2ui m_size{0, 0};

	/// Work arrays of `compile`, kept between frames for their capacity.
	struct CompileScratch {
		/// Per target: written by an earlier pass.
		std::vector<uint8_t> written;
		/// Per target: needed by a kept pass or the frame.
		std::vector<uint8_t> needed;
		/// Per pass: kept.
		std::vector<uint8_t> kept;
		/// Per pass: passes that must run after it.
		std::vector<std::vector<uint32_t>> successors;
		/// Per pass: passes still to run before it.
		std::vector<uint32_t> predecessorCount;
		/// Per target: last pass writing it.
		std::vector<int64_t> lastWriter;
		/// Per target: passes reading it since its last write.
		std::vector<std::vector<uint32_t>> readersSinceWrite;
		/// Passes ready to run.
		std::vector<uint32_t> ready;
	};
	/// Work arrays of `compile`.
	CompileScratch m_scratch;
};

}// namespace owl::renderer
