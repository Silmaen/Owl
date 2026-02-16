/**
 * @file Renderer2D.cpp
 * @author Silmaen
 * @date 18/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Renderer2D.h"

#include "core/Application.h"
#include "renderer/BackgroundRenderer.h"
#include "renderer/DrawData.h"
#include "renderer/RenderCommand.h"
#include "renderer/UniformBuffer.h"

namespace owl::renderer {

namespace utils {

namespace {
constexpr uint32_t g_maxQuads = 20000;
constexpr size_t g_quadVertexCount = 4;
constexpr uint32_t g_maxVertices = g_maxQuads * g_quadVertexCount;
constexpr uint32_t g_maxIndices = g_maxQuads * 6;
constexpr std::array g_textureCoords{math::vec2{0.0f, 0.0f}, math::vec2{1.0f, 0.0f}, math::vec2{1.0f, 1.0f},
									 math::vec2{0.0f, 1.0f}};
constexpr std::array g_quadVertexPositions = {math::vec4{-0.5f, -0.5f, 0.0f, 1.0f}, math::vec4{0.5f, -0.5f, 0.0f, 1.0f},
											  math::vec4{0.5f, 0.5f, 0.0f, 1.0f}, math::vec4{-0.5f, 0.5f, 0.0f, 1.0f}};

uint32_t g_MaxTextureSlots = 0;
}// namespace

/**
 * @brief Structure holding quad vertex information.
 */
struct QuadVertex {
	math::vec3 position;
	math::vec4 color;
	math::vec2 texCoord;
	float texIndex;
	float tilingFactor;
	int entityId;
};

/**
 * @brief Structure holding circle vertex information.
 */
struct CircleVertex {
	math::vec3 worldPosition;
	math::vec3 localPosition;
	math::vec4 color;
	float thickness;
	float fade;
	int entityId;
};

/**
 * @brief Structure holding line vertex information.
 */
struct LineVertex {
	math::vec3 position;
	math::vec4 color;
	int entityId;
};

/**
 * @brief Structure holding text vertex information.
 */
struct TextVertex {
	math::vec3 position;
	math::vec4 color;
	math::vec2 texCoord;
	float texIndex;
	// todo bg color
	int entityId;
};

/**
 * @brief Base structure for rendering an object type
 */
template<typename VertexType>
struct VertexData {
	uint32_t indexCount = 0;
	std::vector<VertexType> vertexBuf;
};

namespace {
template<typename VertexType>
void resetDrawData(VertexData<VertexType>& iData) {
	iData.indexCount = 0;
	iData.vertexBuf.clear();
	iData.vertexBuf.reserve(g_maxVertices);
}
}// namespace

/**
 * @brief Structure holding static internal g_data
 */
struct InternalData {
	/// Camera Data
	struct CameraData {
		/// Camera projection
		math::mat4 viewProjection;
	};
	CameraData cameraBuffer{};
	/// Quad Data
	VertexData<QuadVertex> quad;
	shared<DrawData> drawQuad;
	/// Circle Data
	VertexData<CircleVertex> circle;
	shared<DrawData> drawCircle;
	/// Line Data
	VertexData<LineVertex> line;
	shared<DrawData> drawLine;
	/// text Data
	VertexData<TextVertex> text;
	shared<DrawData> drawText;
	/// Statistics
	Renderer2D::Statistics stats;
	// Textures Data
	/// One white texture for coloring
	shared<Texture2D> whiteTexture;

	shared<UniformBuffer> cameraUniformBuffer;
	/// Array of textures
	std::vector<shared<Texture2D>> textureSlots;
	/// next texture index
	uint32_t textureSlotIndex = 1;// 0 = white texture
};

}// namespace utils

namespace {
shared<utils::InternalData> g_Data;
}// namespace

void Renderer2D::init() {
	OWL_PROFILE_FUNCTION()

	if (g_Data) {
		OWL_CORE_WARN("Renderer2D already initiated.")
		g_Data.reset();
	}
	g_Data = mkShared<utils::InternalData>();

	std::vector<uint32_t> quadIndices;
	{
		quadIndices.resize(utils::g_maxIndices);
		uint32_t offset = 0;
		for (uint32_t i = 0; i < utils::g_maxIndices; i += 6) {
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}
	}
	// quads
	g_Data->drawQuad = DrawData::create();
	g_Data->drawQuad->init(
			{
					{"i_Position", ShaderDataType::Float3},
					{"i_Color", ShaderDataType::Float4},
					{"i_TexCoord", ShaderDataType::Float2},
					{"i_TexIndex", ShaderDataType::Float},
					{"i_TilingFactor", ShaderDataType::Float},
					{"i_EntityID", ShaderDataType::Int},
			},
			"renderer2D", quadIndices, "quad");
	// circles
	g_Data->drawCircle = DrawData::create();
	g_Data->drawCircle->init(
			{
					{"i_WorldPosition", ShaderDataType::Float3},
					{"i_LocalPosition", ShaderDataType::Float3},
					{"i_Color", ShaderDataType::Float4},
					{"i_Thickness", ShaderDataType::Float},
					{"i_Fade", ShaderDataType::Float},
					{"i_EntityID", ShaderDataType::Int},
			},
			"renderer2D", quadIndices, "circle");
	// Lines
	g_Data->drawLine = DrawData::create();
	g_Data->drawLine->init(
			{
					{"i_Position", ShaderDataType::Float3},
					{"i_Color", ShaderDataType::Float4},
					{"i_EntityID", ShaderDataType::Int},
			},
			"renderer2D", quadIndices, "line");
	// Text
	g_Data->drawText = DrawData::create();
	g_Data->drawText->init(
			{
					{"i_Position", ShaderDataType::Float3},
					{"i_Color", ShaderDataType::Float4},
					{"i_TexCoord", ShaderDataType::Float2},
					{"i_TexIndex", ShaderDataType::Float},
					{"i_EntityID", ShaderDataType::Int},
			},
			"renderer2D", quadIndices, "text");

	g_Data->whiteTexture = Texture2D::create(Texture2D::Specification{.size = {1, 1}, .format = ImageFormat::Rgba8});
	uint32_t whiteTextureData = 0xffffffff;
	g_Data->whiteTexture->setData(&whiteTextureData, sizeof(uint32_t));


	// Set all texture slots to 0
	utils::g_MaxTextureSlots = RenderCommand::getMaxTextureSlots();
	g_Data->textureSlots.resize(utils::g_MaxTextureSlots);
	g_Data->textureSlots[0] = g_Data->whiteTexture;
	g_Data->cameraUniformBuffer = UniformBuffer::create(sizeof(utils::InternalData::CameraData), 0, "Renderer2D");
	g_Data->cameraUniformBuffer->bind();
}

void Renderer2D::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (!g_Data) {
		OWL_CORE_WARN("Renderer2D already shutdown.")
		return;
	}

	// clearing the internal g_data
	g_Data->cameraUniformBuffer.reset();
	g_Data->whiteTexture.reset();
	for (auto& text: g_Data->textureSlots) {
		if (text == nullptr)
			continue;
		text.reset();
	}
	g_Data->drawQuad.reset();
	g_Data->drawCircle.reset();
	g_Data->drawLine.reset();
	g_Data.reset();
}

void Renderer2D::beginScene(const Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	g_Data->cameraBuffer.viewProjection = iCamera.getViewProjection();
	g_Data->cameraUniformBuffer->setData(&g_Data->cameraBuffer, sizeof(utils::InternalData::CameraData), 0);
	startBatch();
}

void Renderer2D::endScene() {
	OWL_PROFILE_FUNCTION()

	flush();
}

void Renderer2D::flush() {
	// Register background texture in our texture slots (if pending)
	float bgTexIndex = 0.0f;
	if (BackgroundRenderer::hasPending()) {
		if (auto bgTex = BackgroundRenderer::getPendingTexture()) {
			bgTexIndex = static_cast<float>(g_Data->textureSlotIndex);
			g_Data->textureSlots[g_Data->textureSlotIndex] = bgTex;
			g_Data->textureSlotIndex++;
		}
	}

	// bind textures
	RenderCommand::beginBatch();
	RenderCommand::beginTextureLoad();
	for (uint32_t i = 0; i < g_Data->textureSlotIndex; i++) g_Data->textureSlots[i]->bind(i);
	RenderCommand::endTextureLoad();

	// Draw background first (within the same render pass)
	BackgroundRenderer::flushPending(bgTexIndex);

	if (g_Data->quad.indexCount > 0) {
		g_Data->drawQuad->setVertexData(
				g_Data->quad.vertexBuf.data(),
				static_cast<uint32_t>(g_Data->quad.vertexBuf.size() * sizeof(utils::QuadVertex)));

		// draw call
		RenderCommand::drawData(g_Data->drawQuad, g_Data->quad.indexCount);
		g_Data->stats.drawCalls++;
	}
	if (g_Data->circle.indexCount > 0) {
		g_Data->drawCircle->setVertexData(
				g_Data->circle.vertexBuf.data(),
				static_cast<uint32_t>(g_Data->circle.vertexBuf.size() * sizeof(utils::CircleVertex)));
		// draw call
		RenderCommand::drawData(g_Data->drawCircle, g_Data->circle.indexCount);
		g_Data->stats.drawCalls++;
	}
	if (g_Data->line.indexCount > 0) {
		g_Data->drawLine->setVertexData(
				g_Data->line.vertexBuf.data(),
				static_cast<uint32_t>(g_Data->line.vertexBuf.size() * sizeof(utils::LineVertex)));
		// draw call
		RenderCommand::drawLine(g_Data->drawLine, g_Data->line.indexCount);
		g_Data->stats.drawCalls++;
	}
	if (g_Data->text.indexCount > 0) {
		g_Data->drawText->setVertexData(
				g_Data->text.vertexBuf.data(),
				static_cast<uint32_t>(g_Data->text.vertexBuf.size() * sizeof(utils::TextVertex)));
		// draw call
		RenderCommand::drawData(g_Data->drawText, g_Data->text.indexCount);
		g_Data->stats.drawCalls++;
	}
	RenderCommand::endBatch();
}

void Renderer2D::startBatch() {
	utils::resetDrawData(g_Data->quad);
	utils::resetDrawData(g_Data->circle);
	utils::resetDrawData(g_Data->line);
	utils::resetDrawData(g_Data->text);
	g_Data->textureSlotIndex = 1;
}

void Renderer2D::nextBatch() {
	flush();
	startBatch();
}

void Renderer2D::drawLine(const LineData& iLineData) {
	g_Data->line.vertexBuf.emplace_back(
			utils::LineVertex{.position = iLineData.point1, .color = iLineData.color, .entityId = iLineData.entityId});
	g_Data->line.vertexBuf.emplace_back(
			utils::LineVertex{.position = iLineData.point2, .color = iLineData.color, .entityId = iLineData.entityId});

	g_Data->line.indexCount += 2;
	g_Data->stats.drawCalls++;
}

void Renderer2D::drawRect(const RectData& iRectData) {
	const math::mat4 trans = iRectData.transform();
	std::vector<math::vec3> points;
	static const std::vector<std::pair<uint8_t, uint8_t>> idx = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
	points.reserve(utils::g_quadVertexPositions.size());
	for (const auto& vtx: utils::g_quadVertexPositions) points.emplace_back(trans * vtx);
	for (const auto& [p1, p2]: idx)
		drawLine(
				{.point1 = points[p1], .point2 = points[p2], .color = iRectData.color, .entityId = iRectData.entityId});
}

void Renderer2D::drawPolyLine(const PolyLineData& iLineData) {
	if (iLineData.points.size() < 2) {
		OWL_CORE_WARN("Too few points in the multiline with ID {}", iLineData.entityId)
		return;
	}
	const math::mat4 trans = iLineData.transform();
	std::vector<math::vec3> points;
	std::vector<std::pair<uint32_t, uint32_t>> link;
	uint32_t i = 0;
	for (const auto& vtx: iLineData.points) {
		points.emplace_back(trans * math::vec4{vtx.x(), vtx.y(), vtx.z(), 1.f});
		if (i < iLineData.points.size() - 1)
			link.emplace_back(i, i + 1);
		++i;
	}
	if (iLineData.closed)
		link.emplace_back(iLineData.points.size() - 1, 0);
	for (const auto& [p1, p2]: link)
		drawLine(
				{.point1 = points[p1], .point2 = points[p2], .color = iLineData.color, .entityId = iLineData.entityId});
}

void Renderer2D::drawCircle(const CircleData& iCircleData) {
	OWL_PROFILE_FUNCTION()

	// TODO(Silmaen): implement for circles
	// if (g_data->circleIndexCount >= utils::maxIndices)
	// 	nextBatch();

	for (const auto& vtx: utils::g_quadVertexPositions) {
		g_Data->circle.vertexBuf.emplace_back(utils::CircleVertex{.worldPosition = iCircleData.transform() * vtx,
																  .localPosition = vtx * 2.0f,
																  .color = iCircleData.color,
																  .thickness = iCircleData.thickness,
																  .fade = iCircleData.fade,
																  .entityId = iCircleData.entityId});
	}

	g_Data->circle.indexCount += 6;
	g_Data->stats.quadCount++;
}

void Renderer2D::drawQuad(const Quad2DData& iQuadData) {
	OWL_PROFILE_FUNCTION()
	if (g_Data->quad.indexCount >= utils::g_maxIndices)
		nextBatch();
	float textureIndex = 0.0f;
	if (iQuadData.texture != nullptr) {
		for (uint32_t i = 1; i < g_Data->textureSlotIndex; i++) {
			if (*g_Data->textureSlots[i] == *iQuadData.texture) {
				textureIndex = static_cast<float>(i);
				break;
			}
		}
		if (textureIndex == 0.0f) {
			if (g_Data->textureSlotIndex >= utils::g_MaxTextureSlots)
				nextBatch();
			textureIndex = static_cast<float>(g_Data->textureSlotIndex);
			g_Data->textureSlots[g_Data->textureSlotIndex] = std::static_pointer_cast<Texture2D>(iQuadData.texture);
			g_Data->textureSlotIndex++;
		}
	}
	for (size_t i = 0; i < utils::g_quadVertexCount; i++) {
		g_Data->quad.vertexBuf.emplace_back(
				utils::QuadVertex{.position = iQuadData.transform() * utils::g_quadVertexPositions[i],
								  .color = iQuadData.color,
								  .texCoord = utils::g_textureCoords[i],
								  .texIndex = textureIndex,
								  .tilingFactor = iQuadData.tilingFactor,
								  .entityId = iQuadData.entityId});
	}
	g_Data->quad.indexCount += 6;
	g_Data->stats.quadCount++;
}

void Renderer2D::drawString(const StringData& iStringData) {
	if (iStringData.font == nullptr) {
		OWL_CORE_ERROR("Renderer2D::drawString: Font not set")
		return;
	}

	// Manage texture
	const shared<Texture2D> fontAtlas = iStringData.font->getAtlasTexture();
	float textureIndex = 0.0f;
	for (uint32_t i = 1; i < g_Data->textureSlotIndex; i++) {
		if (*g_Data->textureSlots[i] == *fontAtlas) {
			textureIndex = static_cast<float>(i);
			break;
		}
	}
	if (textureIndex == 0.0f) {
		if (g_Data->textureSlotIndex >= utils::g_MaxTextureSlots)
			nextBatch();
		textureIndex = static_cast<float>(g_Data->textureSlotIndex);
		g_Data->textureSlots[g_Data->textureSlotIndex] = fontAtlas;
		g_Data->textureSlotIndex++;
	}
	// compute extent.
	math::box2f extents;
	{
		math::vec2 cursor{0.f, 0.f};
		for (size_t i = 0; i < iStringData.text.size(); i++) {
			char character = iStringData.text[i];
			if (isascii(character) == 0)
				character = '?';
			if (character == '\r')
				continue;
			if (character == '\n') {
				cursor.x() = 0;
				cursor.y() -= iStringData.font->getScaledLineHeight() + iStringData.lineSpacing;
				continue;
			}
			auto [quad, uv] = iStringData.font->getGlyphBox(character);
			quad.translate(cursor);
			extents.update(quad);
			if (i < iStringData.text.size() - 1) {
				char next = iStringData.text[i + 1];
				if (isascii(next) == 0)
					next = '?';
				cursor.x() += iStringData.font->getAdvance(character, next) + iStringData.kerning;
			}
		}
	}
	// compute offset and scale to fit the 2D quad -1,1
	math::vec2 scale = extents.diagonal();
	scale.x() = 1.f / scale.x();
	scale.y() = 1.f / scale.y();
	const math::vec2 offset = -extents.min() - 0.5f * extents.diagonal();
	math::vec2 cursor{0.f, 0.f};
	for (size_t i = 0; i < iStringData.text.size(); i++) {
		char character = iStringData.text[i];
		if (isascii(character) == 0)
			character = '?';
		if (character == '\r')
			continue;
		if (character == '\n') {
			cursor.x() = 0;
			cursor.y() -= iStringData.font->getScaledLineHeight() + iStringData.lineSpacing;
			continue;
		}
		auto [quad, uv] = iStringData.font->getGlyphBox(character);
		quad.translate(cursor + offset);
		quad.scale(scale);
		// render here
		const math::vec3 p1 = iStringData.transform() * math::vec4(quad.min().x(), quad.min().y(), 0, 1.f);
		const math::vec3 p2 = iStringData.transform() * math::vec4(quad.min().x(), quad.max().y(), 0, 1.f);
		const math::vec3 p3 = iStringData.transform() * math::vec4(quad.max().x(), quad.max().y(), 0, 1.f);
		const math::vec3 p4 = iStringData.transform() * math::vec4(quad.max().x(), quad.min().y(), 0, 1.f);
		g_Data->text.vertexBuf.emplace_back(utils::TextVertex{.position = p1,
															  .color = iStringData.color,
															  .texCoord = uv.min(),
															  .texIndex = textureIndex,
															  .entityId = iStringData.entityId});
		g_Data->text.vertexBuf.emplace_back(utils::TextVertex{.position = p2,
															  .color = iStringData.color,
															  .texCoord = {uv.min().x(), uv.max().y()},
															  .texIndex = textureIndex,
															  .entityId = iStringData.entityId});
		g_Data->text.vertexBuf.emplace_back(utils::TextVertex{.position = p3,
															  .color = iStringData.color,
															  .texCoord = uv.max(),
															  .texIndex = textureIndex,
															  .entityId = iStringData.entityId});
		g_Data->text.vertexBuf.emplace_back(utils::TextVertex{.position = p4,
															  .color = iStringData.color,
															  .texCoord = {uv.max().x(), uv.min().y()},
															  .texIndex = textureIndex,
															  .entityId = iStringData.entityId});
		g_Data->stats.quadCount++;
		g_Data->text.indexCount += 6;
		if (i < iStringData.text.size() - 1) {
			char next = iStringData.text[i + 1];
			if (isascii(next) == 0)
				next = '?';
			cursor.x() += iStringData.font->getAdvance(character, next) + iStringData.kerning;
		}
	}
}

void Renderer2D::resetStats() {
	g_Data->stats.drawCalls = 0;
	g_Data->stats.quadCount = 0;
	g_Data->stats.lineCount = 0;
}

auto Renderer2D::getStats() -> Statistics { return g_Data->stats; }

}// namespace owl::renderer
