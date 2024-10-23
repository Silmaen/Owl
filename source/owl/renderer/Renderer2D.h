/**
 * @file Renderer2D.h
 * @author Silmaen
 * @date 18/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Camera.h"
#include "CameraEditor.h"
#include "CameraOrtho.h"
#include "Texture.h"
#include "fonts/Font.h"
#include "scene/component/SpriteRenderer.h"


namespace owl::renderer {
/**
 * @brief Utility functions.
 */
namespace utils {
/**
 * @brief Structure holding Position Rotation Scale.
 */
struct PRS {
	/// The position.
	math::vec3 position = math::vec3{0.f, 0.f, 0.f};
	/// The rotation.
	float rotation = 0.f;
	/// The size.
	math::vec2 size = math::vec2{1.f, 1.f};
};

/**
 * @brief Convert PRS structure int transformation matrix.
 * @param[in] iTransform The input PRS Structure.
 * @return The corresponding Transformation matrix.
 */
OWL_API auto toTransform(const PRS& iTransform) -> math::mat4;

/**
 * @brief 2D transformation structure.
 */
struct Transform2D {
	Transform2D() = delete;

	// NOLINTBEGIN(google-explicit-constructor)
	// NOLINTBEGIN(hicpp-explicit-conversions)
	/**
	 * @brief Constructor by transformation matrix.
	 * @param[in] iMatrix Input transformation matrix.
	 */
	Transform2D(const math::mat4& iMatrix) : transform{iMatrix} {}

	/**
	 * @brief Constructor by PRS.
	 * @param[in] iTransform Input PRS.
	 */
	Transform2D(const PRS& iTransform) : transform{toTransform(iTransform)} {}
	// NOLINTEND(hicpp-explicit-conversions)
	// NOLINTEND(google-explicit-constructor)

	/// The transformation matrix.
	math::mat4 transform;
};
}// namespace utils

/**
 * @brief Data for drawing a quad.
 */
struct OWL_API Quad2DData {
	/// Transformation of the square.
	utils::Transform2D transform;
	/// Color to apply to the quad.
	math::vec4 color = math::vec4{1.f, 1.f, 1.f, 1.f};
	/// Eventually the texture of the quad (plain color if nullptr).
	shared<Texture> texture = nullptr;
	/// Tilling factor of the texture.
	float tilingFactor = 1.f;
	/// unique ID for the entity.
	int entityId = -1;
};

/**
 * @brief Data for drawing a circle.
 */
struct OWL_API CircleData {
	/// Transformation of the circle.
	utils::Transform2D transform;
	/// Color to apply to the circle.
	math::vec4 color = math::vec4{1.f, 1.f, 1.f, 1.f};
	/// Thickness of the line.
	float thickness = 1.0f;
	/// Fading of the circle.
	float fade = 0.005f;
	/// unique ID for the entity.
	int entityId = -1;
};

/**
 * @brief Data for drawing a line.
 */
struct OWL_API LineData {
	/// Starting point.
	math::vec3 point1;
	/// Ending point.
	math::vec3 point2;
	/// Color to apply to the line.
	math::vec4 color = math::vec4{1.f, 1.f, 1.f, 1.f};
	/// unique ID for the entity.
	int entityId = -1;
};

/**
 * @brief Data for drawing a rectangle.
 */
struct OWL_API RectData {
	/// Transformation of the rectangle.
	utils::Transform2D transform;
	/// Color to apply to the rect.
	math::vec4 color = math::vec4{1.f, 1.f, 1.f, 1.f};
	/// unique ID for the entity.
	int entityId = -1;
};

/**
 * @brief Data for drawing a polyline.
 */
struct OWL_API PolyLineData {
	/// Transformation of the rectangle.
	utils::Transform2D transform;
	/// Set of points.
	std::vector<math::vec3> points;
	/// If the ending point connected to starting point.
	bool closed = false;
	/// Color to apply to the rect.
	math::vec4 color = math::vec4{1.f, 1.f, 1.f, 1.f};
	/// unique ID for the entity.
	int entityId = -1;
};

/**
 * @brief Data for drawing a string.
 */
struct OWL_API StringData {
	/// Transformation of the render.
	utils::Transform2D transform;
	/// Test to render
	std::string text;
	/// font to use (or default one)
	shared<fonts::Font> font = nullptr;
	/// Color to render.
	math::vec4 color = math::vec4{1.f, 1.f, 1.f, 1.f};
	/// The kerning.
	float kerning = 0.f;
	/// The line spacing.
	float lineSpacing = 0.f;
	/// unique ID for the entity.
	int entityId = -1;
};

/**
 * @brief Class Renderer2D.
 */
class OWL_API Renderer2D {
public:
	Renderer2D() = default;
	Renderer2D(const Renderer2D&) = delete;
	Renderer2D(Renderer2D&&) = delete;
	auto operator=(const Renderer2D&) -> Renderer2D& = delete;
	auto operator=(Renderer2D&&) -> Renderer2D& = delete;

	/**
	 * @brief Destructor.
	 */
	~Renderer2D() = default;

	/**
	 * @brief Initialize the renderer.
	 */
	static void init();

	/**
	 * @brief Terminate the renderer.
	 */
	static void shutdown();

	/**
	 * @brief Begins a scene.
	 * @param[in] iCamera The camera.
	 */
	static void beginScene(const Camera& iCamera);

	/**
	 * @brief Ends a scene.
	 */
	static void endScene();

	/**
	 * @brief Flush the vertex buffer.
	 */
	static void flush();

	// Primitives
	/**
	 * @brief Draw a line on the screen.
	 * @param[in] iLineData The data to draw the line.
	 */
	static void drawLine(const LineData& iLineData);

	/**
	 * @brief Draw a rectangle.
	 * @param[in] iRectData The data to draw the rectangle.
	 */
	static void drawRect(const RectData& iRectData);

	/**
	 * @brief Draw a polyline on the screen.
	 * @param[in] iLineData The data to draw the polyline
	 */
	static void drawPolyLine(const PolyLineData& iLineData);

	/**
	 * @brief Draws a circle on the screen.
	 * @param[in] iCircleData Circle's properties.
	 */
	static void drawCircle(const CircleData& iCircleData);

	/**
	 * @brief Draws a Quad on the screen.
	 * @param[in] iQuadData Quad's properties.
	 */
	static void drawQuad(const Quad2DData& iQuadData);

	/**
	 * @brief Draws a Quad on the screen.
	 * @param[in] iStringData String's properties.
	 */
	static void drawString(const StringData& iStringData);

	/**
	 * @brief Statistics.
	 */
	struct OWL_API Statistics {
		/// Amount of draw calls.
		uint32_t drawCalls = 0;
		/// Amount of quad drawn.
		uint32_t quadCount = 0;
		/// Amount of lines drawn.
		uint32_t lineCount = 0;
		/// Compute the amount of vertices.
		[[nodiscard]] auto getTotalVertexCount() const -> uint32_t { return quadCount * 4 + lineCount * 2; }
		/// Compute the amount of indices.
		[[nodiscard]] auto getTotalIndexCount() const -> uint32_t { return quadCount * 6 + lineCount * 2; }
	};

	/**
	 * @brief Reset the statistics data.
	 */
	static void resetStats();

	/**
	 * @brief Access to stats.
	 * @return The Stats.
	 */
	static auto getStats() -> Statistics;

	/**
	 * @brief Start the next batch.
	 */
	static void nextBatch();

private:
	/**
	 * @brief Combine flush and reset.
	 */
	static void startBatch();
};

}// namespace owl::renderer
