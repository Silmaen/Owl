/**
 * @file PipelineState.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstdint>

namespace owl::renderer::gpu {

/**
 * @brief
 *  How the vertices of a draw are assembled into primitives.
 */
enum struct PrimitiveTopology : uint8_t {
	/// Every three indices form a triangle.
	Triangles,
	/// Every two indices form a line segment.
	Lines,
};

/**
 * @brief
 *  Which faces the rasterizer discards. Front faces are counter-clockwise on screen, on every backend.
 */
enum struct CullMode : uint8_t {
	/// Both faces are drawn.
	None,
	/// Back faces are discarded.
	Back,
	/// Front faces are discarded.
	Front,
};

/**
 * @brief
 *  How the colour output is combined with the colour already in the attachment.
 */
enum struct BlendMode : uint8_t {
	/// Straight alpha blending: `src * a + dst * (1 - a)`.
	Alpha,
	/// The output replaces the attachment colour.
	Opaque,
};

/**
 * @brief
 *  Fixed-function state of a draw, part of the Owl RHI pipeline object.
 *
 * A `DrawData` receives its state at `init` and every draw of it uses that state: nothing is inherited from the
 * previous draw. Vulkan bakes topology, culling and blending into the pipeline and records the depth flags as
 * dynamic state; OpenGL applies the differences from the previous draw. The default is the painter-ordered 2D state:
 * triangles, no culling, alpha blending, no depth.
 */
struct PipelineState {
	/// Primitive assembly.
	PrimitiveTopology topology = PrimitiveTopology::Triangles;
	/// Face culling.
	CullMode cullMode = CullMode::None;
	/// Colour blending.
	BlendMode blendMode = BlendMode::Alpha;
	/// Test fragments against the depth attachment (`less`).
	bool depthTest = false;
	/// Write the depth of the fragments that pass (only meaningful with `depthTest`).
	bool depthWrite = false;

	/**
	 * @brief
	 *  Compare two states field by field.
	 * @param[in] iOther The other state.
	 * @return True when every field matches.
	 */
	auto operator==(const PipelineState& iOther) const -> bool = default;
};

}// namespace owl::renderer::gpu
