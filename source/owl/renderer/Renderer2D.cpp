/**
 * @file Renderer2D.cpp
 * @author Silmaen
 * @date 18/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Renderer2D.h"

#include "DrawData.h"
#include "RenderCommand.h"
#include "UniformBuffer.h"

namespace owl::renderer {

namespace utils {

namespace {
constexpr uint32_t g_maxQuads = 20000;
constexpr size_t g_quadVertexCount = 4;
constexpr uint32_t g_maxVertices = g_maxQuads * g_quadVertexCount;
constexpr uint32_t g_maxIndices = g_maxQuads * 6;
constexpr glm::vec2 g_textureCoords[] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
constexpr glm::vec4 g_quadVertexPositions[] = {{-0.5f, -0.5f, 0.0f, 1.0f},
											   {0.5f, -0.5f, 0.0f, 1.0f},
											   {0.5f, 0.5f, 0.0f, 1.0f},
											   {-0.5f, 0.5f, 0.0f, 1.0f}};

uint32_t g_maxTextureSlots = 0;
}// namespace

/**
 * @brief Structure holding quad vertex information.
 */
struct QuadVertex {
	glm::vec3 position;
	glm::vec4 color;
	glm::vec2 texCoord;
	float texIndex;
	float tilingFactor;
	int entityID;
};

/**
 * @brief Structure holding circle vertex information.
 */
struct CircleVertex {
	glm::vec3 worldPosition;
	glm::vec3 localPosition;
	glm::vec4 color;
	float thickness;
	float fade;
	int entityID;
};

/**
 * @brief Structure holding line vertex information.
 */
struct LineVertex {
	glm::vec3 position;
	glm::vec4 color;
	int entityID;
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
void resetDrawData(VertexData<VertexType> &iData) {
	iData.indexCount = 0;
	iData.vertexBuf.clear();
	iData.vertexBuf.reserve(utils::g_maxVertices);
}
}// namespace

/**
 * @brief Structure holding static internal g_data
 */
struct internalData {
	/// Camera Data
	struct CameraData {
		/// Camera projection
		glm::mat4 viewProjection;
	};
	CameraData cameraBuffer{};
	/// Quad Data
	VertexData<QuadVertex> quad;
	/// Circle Data
	VertexData<CircleVertex> circle;
	/// Line Data
	VertexData<LineVertex> line;
	/// Statistics
	Renderer2D::Statistics stats;
	shared<DrawData> drawQuad;
	shared<DrawData> drawCircle;
	shared<DrawData> drawLine;
	/// Debug Triangle
	shared<DrawData> drawDataTriangle;
	// Textures Data
	/// One white texture for coloring
	shared<Texture2D> whiteTexture;

	shared<UniformBuffer> cameraUniformBuffer;
	/// Array of textures
	std::vector<shared<Texture2D>> textureSlots;
	float lineWidth = 2.0f;
	/// next texture index
	uint32_t textureSlotIndex = 1;// 0 = white texture
	bool doTriangleDraw = false;
};

glm::mat4 toTransform(const PRS &iTransform) {
	return glm::translate(glm::mat4(1.0f), iTransform.position) *
		   glm::rotate(glm::mat4(1.0f), glm::radians(iTransform.rotation), {0.0f, 0.0f, 1.0f}) *
		   glm::scale(glm::mat4(1.0f), {iTransform.size.x, iTransform.size.y, 1.0f});
}

}// namespace utils

namespace {
shared<utils::internalData> g_data;
}// namespace

void Renderer2D::init() {
	OWL_PROFILE_FUNCTION()

	if (g_data) {
		OWL_CORE_WARN("Renderer2D already initiated.")
		g_data.reset();
	}
	g_data = mkShared<utils::internalData>();

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
	g_data->drawQuad = DrawData::create();
	g_data->drawQuad->init(
			{
					{"a_Position", ShaderDataType::Float3},
					{"a_Color", ShaderDataType::Float4},
					{"a_TexCoord", ShaderDataType::Float2},
					{"a_TexIndex", ShaderDataType::Float},
					{"a_TilingFactor", ShaderDataType::Float},
					{"a_EntityID", ShaderDataType::Int},
			},
			"renderer2D", quadIndices, "quad");
	// circles
	g_data->drawCircle = DrawData::create();
	g_data->drawCircle->init(
			{
					{"a_WorldPosition", ShaderDataType::Float3},
					{"a_LocalPosition", ShaderDataType::Float3},
					{"a_Color", ShaderDataType::Float4},
					{"a_Thickness", ShaderDataType::Float},
					{"a_Fade", ShaderDataType::Float},
					{"a_EntityID", ShaderDataType::Int},
			},
			"renderer2D", quadIndices, "circle");
	// Lines
	g_data->drawLine = DrawData::create();
	g_data->drawLine->init(
			{
					{"a_Position", ShaderDataType::Float3},
					{"a_Color", ShaderDataType::Float4},
					{"a_EntityID", ShaderDataType::Int},
			},
			"renderer2D", quadIndices, "line");

	g_data->whiteTexture = Texture2D::create(1, 1);
	uint32_t whiteTextureData = 0xffffffff;
	g_data->whiteTexture->setData(&whiteTextureData, sizeof(uint32_t));


	// Set all texture slots to 0
	utils::g_maxTextureSlots = RenderCommand::getMaxTextureSlots();
	g_data->textureSlots.resize(utils::g_maxTextureSlots);
	g_data->textureSlots[0] = g_data->whiteTexture;
	g_data->cameraUniformBuffer = UniformBuffer::create(sizeof(utils::internalData::CameraData), 0, "Renderer2D");
	g_data->cameraUniformBuffer->bind();

	// debug
	g_data->drawDataTriangle = DrawData::create();
	g_data->drawDataTriangle->init({}, "renderer2D", quadIndices, "triangle");
}

void Renderer2D::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (!g_data) {
		OWL_CORE_WARN("Renderer2D already shutdown.")
		return;
	}

	// clearing the internal g_data
	g_data->cameraUniformBuffer.reset();
	g_data->whiteTexture.reset();
	for (auto &text: g_data->textureSlots) {
		if (text == nullptr)
			continue;
		text.reset();
	}
	g_data->drawQuad.reset();
	g_data->drawCircle.reset();
	g_data->drawLine.reset();
	g_data.reset();
}

void Renderer2D::beginScene(const CameraOrtho &iCamera) {
	OWL_PROFILE_FUNCTION()

	g_data->cameraBuffer.viewProjection = iCamera.getViewProjectionMatrix();
	g_data->cameraUniformBuffer->setData(&g_data->cameraBuffer, sizeof(utils::internalData::CameraData), 0);
	startBatch();
}

void Renderer2D::beginScene(const CameraEditor &iCamera) {
	OWL_PROFILE_FUNCTION()

	g_data->cameraBuffer.viewProjection = iCamera.getViewProjection();
	g_data->cameraUniformBuffer->setData(&g_data->cameraBuffer, sizeof(utils::internalData::CameraData), 0);
	startBatch();
}

void Renderer2D::beginScene(const Camera &iCamera, const glm::mat4 &iTransform) {
	OWL_PROFILE_FUNCTION()

	g_data->cameraBuffer.viewProjection = iCamera.getProjection() * glm::inverse(iTransform);
	g_data->cameraUniformBuffer->setData(&g_data->cameraBuffer, sizeof(utils::internalData::CameraData), 0);
	startBatch();
}

void Renderer2D::endScene() {
	OWL_PROFILE_FUNCTION()

	flush();
}

void Renderer2D::flush() {
	// bind textures
	RenderCommand::beginBatch();
	RenderCommand::beginTextureLoad();
	for (uint32_t i = 0; i < g_data->textureSlotIndex; i++) g_data->textureSlots[i]->bind(i);
	RenderCommand::endTextureLoad();

	if (g_data->quad.indexCount > 0) {
		g_data->drawQuad->setVertexData(
				g_data->quad.vertexBuf.data(),
				static_cast<uint32_t>(g_data->quad.vertexBuf.size() * sizeof(utils::QuadVertex)));

		// draw call
		RenderCommand::drawData(g_data->drawQuad, g_data->quad.indexCount);
		g_data->stats.drawCalls++;
	}
	if (g_data->circle.indexCount > 0) {
		g_data->drawCircle->setVertexData(
				g_data->circle.vertexBuf.data(),
				static_cast<uint32_t>(g_data->circle.vertexBuf.size() * sizeof(utils::CircleVertex)));
		// draw call
		RenderCommand::drawData(g_data->drawCircle, g_data->circle.indexCount);
		g_data->stats.drawCalls++;
	}
	if (g_data->line.indexCount > 0) {
		g_data->drawLine->setVertexData(
				g_data->line.vertexBuf.data(),
				static_cast<uint32_t>(g_data->line.vertexBuf.size() * sizeof(utils::LineVertex)));
		// draw call
		RenderCommand::drawData(g_data->drawLine, g_data->line.indexCount);
		g_data->stats.drawCalls++;
	}
	if (g_data->doTriangleDraw) {
		RenderCommand::drawData(g_data->drawDataTriangle, 3);
	}
	RenderCommand::endBatch();
}

void Renderer2D::startBatch() {
	utils::resetDrawData(g_data->quad);
	utils::resetDrawData(g_data->circle);
	utils::resetDrawData(g_data->line);
	g_data->doTriangleDraw = false;
	g_data->textureSlotIndex = 1;
}

void Renderer2D::nextBatch() {
	flush();
	startBatch();
}

float Renderer2D::getLineWidth() { return g_data->lineWidth; }

void Renderer2D::setLineWidth(const float iWidth) { g_data->lineWidth = iWidth; }

void Renderer2D::drawDebugTriangle() { g_data->doTriangleDraw = true; }

void Renderer2D::drawLine(const LineData &iLineData) {
	g_data->line.vertexBuf.emplace_back(utils::LineVertex{iLineData.point1, iLineData.color, iLineData.entityID});
	g_data->line.vertexBuf.emplace_back(utils::LineVertex{iLineData.point2, iLineData.color, iLineData.entityID});
	g_data->line.indexCount += 2;
	g_data->stats.drawCalls++;
}

void Renderer2D::drawRect(const RectData &iRectData) {
	const glm::mat4 trans = iRectData.transform.transform;
	std::vector<glm::vec3> points;
	static const std::vector<std::pair<uint8_t, uint8_t>> idx = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
	for (const auto &vtx: utils::g_quadVertexPositions) points.emplace_back(trans * vtx);
	for (const auto &[p1, p2]: idx) drawLine({points[p1], points[p2], iRectData.color, iRectData.entityID});
}

void Renderer2D::drawPolyLine(const PolyLineData &iLineData) {
	if (iLineData.points.size() < 2) {
		OWL_CORE_WARN("Too few points in the multiline with ID {}", iLineData.entityID)
		return;
	}
	const glm::mat4 trans = iLineData.transform.transform;
	std::vector<glm::vec3> points;
	std::vector<std::pair<uint32_t, uint32_t>> link;
	uint32_t i = 0;
	for (const auto &vtx: iLineData.points) {
		points.emplace_back(trans * glm::vec4{vtx.x, vtx.y, vtx.z, 1.f});
		if (i < iLineData.points.size() - 1)
			link.emplace_back(i, i + 1);
		++i;
	}
	if (iLineData.closed)
		link.emplace_back(iLineData.points.size() - 1, 0);
	for (const auto &[p1, p2]: link) drawLine({points[p1], points[p2], iLineData.color, iLineData.entityID});
}

void Renderer2D::drawCircle(const CircleData &iCircleData) {
	OWL_PROFILE_FUNCTION()

	// TODO(Silmaen): implement for circles
	// if (g_data->circleIndexCount >= utils::maxIndices)
	// 	nextBatch();

	for (const auto &vtx: utils::g_quadVertexPositions) {
		g_data->circle.vertexBuf.emplace_back(
				utils::CircleVertex{.worldPosition = iCircleData.transform.transform * vtx,
									.localPosition = vtx * 2.0f,
									.color = iCircleData.color,
									.thickness = iCircleData.thickness,
									.fade = iCircleData.fade,
									.entityID = iCircleData.entityID});
	}

	g_data->circle.indexCount += 6;

	g_data->stats.quadCount++;
}


OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void Renderer2D::drawQuad(const Quad2DData &iQuadData) {
	OWL_PROFILE_FUNCTION()
	if (g_data->quad.indexCount >= utils::g_maxIndices)
		nextBatch();
	float textureIndex = 0.0f;
	if (iQuadData.texture != nullptr) {
		for (uint32_t i = 1; i < g_data->textureSlotIndex; i++) {
			if (*g_data->textureSlots[i] == *iQuadData.texture) {
				textureIndex = static_cast<float>(i);
				break;
			}
		}
		if (textureIndex == 0.0f) {
			if (g_data->textureSlotIndex >= utils::g_maxTextureSlots)
				nextBatch();
			textureIndex = static_cast<float>(g_data->textureSlotIndex);
			g_data->textureSlots[g_data->textureSlotIndex] = std::static_pointer_cast<Texture2D>(iQuadData.texture);
			g_data->textureSlotIndex++;
		}
	}
	for (size_t i = 0; i < utils::g_quadVertexCount; i++) {
		g_data->quad.vertexBuf.emplace_back(
				utils::QuadVertex{.position = iQuadData.transform.transform * utils::g_quadVertexPositions[i],
								  .color = iQuadData.color,
								  .texCoord = utils::g_textureCoords[i],
								  .texIndex = textureIndex,
								  .tilingFactor = iQuadData.tilingFactor,
								  .entityID = iQuadData.entityID});
	}
	g_data->quad.indexCount += 6;

	g_data->stats.quadCount++;
}
OWL_DIAG_POP

void Renderer2D::drawSprite(const glm::mat4 &iTransform, const scene::component::SpriteRenderer &iSrc,
							const int iEntityID) {
	drawQuad({.transform = iTransform,
			  .color = iSrc.color,
			  .texture = iSrc.texture,
			  .tilingFactor = iSrc.tilingFactor,
			  .entityID = iEntityID});
}

void Renderer2D::resetStats() {
	g_data->stats.drawCalls = 0;
	g_data->stats.quadCount = 0;
}

Renderer2D::Statistics Renderer2D::getStats() { return g_data->stats; }

}// namespace owl::renderer
