
#include "testHelper.h"

#include <math/vectors.h>
#include <ranges>
#include <renderer/null/Buffer.h>
#include <renderer/null/DrawData.h>

using namespace owl::renderer;

TEST(Renderer, ShaderDataTypeSize) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	EXPECT_EQ(shaderDataTypeSize(ShaderDataType::Bool), 1);
	EXPECT_EQ(shaderDataTypeSize(ShaderDataType::Float), 4);
	EXPECT_EQ(shaderDataTypeSize(ShaderDataType::Float2), 8);
	EXPECT_EQ(shaderDataTypeSize(ShaderDataType::Float3), 12);
	EXPECT_EQ(shaderDataTypeSize(ShaderDataType::Float4), 16);
	EXPECT_EQ(shaderDataTypeSize(ShaderDataType::Mat3), 36);
	EXPECT_EQ(shaderDataTypeSize(ShaderDataType::Mat4), 64);
	EXPECT_EQ(shaderDataTypeSize(ShaderDataType::Int), 4);
	EXPECT_EQ(shaderDataTypeSize(ShaderDataType::Int2), 8);
	EXPECT_EQ(shaderDataTypeSize(ShaderDataType::Int3), 12);
	EXPECT_EQ(shaderDataTypeSize(ShaderDataType::Int4), 16);
	EXPECT_EQ(shaderDataTypeSize(ShaderDataType::None), 0);
	EXPECT_EQ(shaderDataTypeSize(static_cast<ShaderDataType>(-1)), 0);
	owl::core::Log::invalidate();
}

TEST(Renderer, bufferLayout) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	static const std::unordered_map<ShaderDataType, uint32_t> ref{
			{ShaderDataType::Bool, 1},           {ShaderDataType::Float, 1},  {ShaderDataType::Float2, 2},
			{ShaderDataType::Float3, 3},         {ShaderDataType::Float4, 4}, {ShaderDataType::Mat3, 9},
			{ShaderDataType::Mat4, 16},          {ShaderDataType::Int, 1},    {ShaderDataType::Int2, 2},
			{ShaderDataType::Int3, 3},           {ShaderDataType::Int4, 4},   {ShaderDataType::None, 0},
			{static_cast<ShaderDataType>(-1), 0}};
	owl::core::Log::init(owl::core::Log::Level::Off);
	BufferLayout allTypes{{"bool", ShaderDataType::Bool},
						  {"float", ShaderDataType::Float},
						  {"float2", ShaderDataType::Float2},
						  {"float3", ShaderDataType::Float3},
						  {"float4", ShaderDataType::Float4},
						  {"mat3", ShaderDataType::Mat3},
						  {"mat4", ShaderDataType::Mat4},
						  {"int", ShaderDataType::Int},
						  {"int2", ShaderDataType::Int2},
						  {"int3", ShaderDataType::Int3},
						  {"int4", ShaderDataType::Int4},
						  {"none", ShaderDataType::None},
						  {"invalid", static_cast<ShaderDataType>(-1)}};
	EXPECT_EQ(allTypes.getStride(), 181);
	EXPECT_EQ(allTypes.getElements().begin(), allTypes.begin());
	EXPECT_EQ(allTypes.getElements().end(), allTypes.end());
	for (auto& elem: allTypes) {
		EXPECT_EQ(elem.getComponentCount(), ref.at(elem.type));
		elem.normalized = true;
	}
	owl::core::Log::invalidate();
}

TEST(Renderer, DrawDataNull) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	null::DrawData data;
	{
		const BufferLayout layout{
				{"position", ShaderDataType::Float3},
				{"transformation", ShaderDataType::Mat4},
				{"Ident", ShaderDataType::Int},
		};
		std::vector<uint32_t> ind{0, 1, 2, 2, 1, 3};
		data.init(layout, "bob", ind, "noshader");
	}
	data.bind();
	EXPECT_EQ(data.getIndexCount(), 0);
	data.setShader("super", "bob");
	data.unbind();
	const std::vector<owl::math::vec3> vtx{{0, 0, 0}, {0, 1, 0}, {1, 0, 0}, {1, 1, 0}};
	data.setVertexData(vtx.data(), static_cast<uint32_t>(vtx.size()));
	owl::core::Log::invalidate();
}

TEST(Renderer, VertexBufferNull) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	{
		null::VertexBuffer nullbuf(0);
		EXPECT_EQ(nullbuf.getLayout().getStride(), 0);
		std::vector<owl::math::vec3> vtx{{0, 0, 0}, {0, 1, 0}, {1, 0, 0}, {1, 1, 0}};
		nullbuf.setData(reinterpret_cast<float*>(vtx.data()), static_cast<uint32_t>(vtx.size()));
	}
	{
		std::vector<owl::math::vec3> vtx{{0, 0, 0}, {0, 1, 0}, {1, 0, 0}, {1, 1, 0}};
		null::VertexBuffer fullbuf(reinterpret_cast<float*>(vtx.data()), static_cast<uint32_t>(vtx.size()));
		const BufferLayout layout{
				{"position", ShaderDataType::Float3},
				{"transformation", ShaderDataType::Mat4},
				{"Ident", ShaderDataType::Int},
		};
		fullbuf.setLayout(layout);
		fullbuf.bind();
		fullbuf.unbind();
		EXPECT_EQ(fullbuf.getLayout().getStride(), 80);
		for (const auto& elem: fullbuf.getLayout()) { EXPECT_FALSE(elem.normalized); }
	}
	owl::core::Log::invalidate();
}

TEST(Renderer, IndexBufferNull) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	std::vector<uint32_t> ind{0, 1, 2, 2, 1, 3};
	const null::IndexBuffer nullbuf(ind.data(), static_cast<uint32_t>(ind.size()));
	nullbuf.bind();
	nullbuf.unbind();
	EXPECT_EQ(nullbuf.getCount(), 6);
	owl::core::Log::invalidate();
}
