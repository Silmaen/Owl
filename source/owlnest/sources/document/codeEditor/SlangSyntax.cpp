/**
 * @file SlangSyntax.cpp
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SlangSyntax.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::codeEditor {

namespace {
using namespace std::string_view_literals;

constexpr std::array g_keywords = {
		"struct"sv,
		"class"sv,
		"interface"sv,
		"extension"sv,
		"enum"sv,
		"typedef"sv,
		"typealias"sv,
		"namespace"sv,
		"using"sv,
		"import"sv,
		"module"sv,
		"implementing"sv,
		"__include"sv,
		"associatedtype"sv,
		"property"sv,
		"generic"sv,
		"__generic"sv,
		"where"sv,
		"This"sv,
		"this"sv,
		"let"sv,
		"var"sv,
		"func"sv,
		"if"sv,
		"else"sv,
		"for"sv,
		"while"sv,
		"do"sv,
		"switch"sv,
		"case"sv,
		"default"sv,
		"break"sv,
		"continue"sv,
		"return"sv,
		"discard"sv,
		"true"sv,
		"false"sv,
		"const"sv,
		"static"sv,
		"uniform"sv,
		"in"sv,
		"out"sv,
		"inout"sv,
		"groupshared"sv,
		"cbuffer"sv,
		"tbuffer"sv,
		"row_major"sv,
		"column_major"sv,
		"precise"sv,
		"nointerpolation"sv,
		"centroid"sv,
		"noperspective"sv,
		"public"sv,
		"private"sv,
		"internal"sv,
		"no_diff"sv,
		"differentiable"sv,
		"register"sv,
		"packoffset"sv,
		"inline"sv,
		"volatile"sv,
		"extern"sv,
		"export"sv,
		"sizeof"sv,
		"new"sv,
		"as"sv,
		"is"sv,
		"typeof"sv,
};

constexpr std::array g_types = {
		"void"sv,
		"bool"sv,
		"int"sv,
		"uint"sv,
		"float"sv,
		"half"sv,
		"double"sv,
		"int8_t"sv,
		"int16_t"sv,
		"int64_t"sv,
		"uint8_t"sv,
		"uint16_t"sv,
		"uint64_t"sv,
		"float16_t"sv,
		"vector"sv,
		"matrix"sv,
		"string"sv,
		"Texture1D"sv,
		"Texture2D"sv,
		"Texture3D"sv,
		"TextureCube"sv,
		"Texture2DArray"sv,
		"Texture2DMS"sv,
		"RWTexture1D"sv,
		"RWTexture2D"sv,
		"RWTexture3D"sv,
		"SamplerState"sv,
		"SamplerComparisonState"sv,
		"Sampler2D"sv,
		"Buffer"sv,
		"RWBuffer"sv,
		"StructuredBuffer"sv,
		"RWStructuredBuffer"sv,
		"ByteAddressBuffer"sv,
		"RWByteAddressBuffer"sv,
		"ConstantBuffer"sv,
		"ParameterBlock"sv,
		"Atomic"sv,
		"Optional"sv,
		"Array"sv,
		"Ptr"sv,
		"AppendStructuredBuffer"sv,
		"ConsumeStructuredBuffer"sv,
};

constexpr std::array g_builtins = {
		"mul"sv,
		"saturate"sv,
		"lerp"sv,
		"normalize"sv,
		"dot"sv,
		"cross"sv,
		"length"sv,
		"distance"sv,
		"clamp"sv,
		"pow"sv,
		"sqrt"sv,
		"rsqrt"sv,
		"abs"sv,
		"min"sv,
		"max"sv,
		"floor"sv,
		"ceil"sv,
		"round"sv,
		"frac"sv,
		"fmod"sv,
		"sign"sv,
		"step"sv,
		"smoothstep"sv,
		"reflect"sv,
		"refract"sv,
		"sin"sv,
		"cos"sv,
		"tan"sv,
		"asin"sv,
		"acos"sv,
		"atan"sv,
		"atan2"sv,
		"exp"sv,
		"exp2"sv,
		"log"sv,
		"log2"sv,
		"rcp"sv,
		"mad"sv,
		"fma"sv,
		"transpose"sv,
		"determinant"sv,
		"any"sv,
		"all"sv,
		"select"sv,
		"ddx"sv,
		"ddy"sv,
		"fwidth"sv,
		"asfloat"sv,
		"asint"sv,
		"asuint"sv,
		"trunc"sv,
		"isnan"sv,
		"isinf"sv,
		"Sample"sv,
		"SampleLevel"sv,
		"SampleGrad"sv,
		"SampleBias"sv,
		"SampleCmp"sv,
		"Load"sv,
		"Store"sv,
		"GetDimensions"sv,
		"InterlockedAdd"sv,
		"InterlockedMin"sv,
		"InterlockedMax"sv,
		"InterlockedOr"sv,
		"InterlockedAnd"sv,
		"GroupMemoryBarrierWithGroupSync"sv,
		"AllMemoryBarrier"sv,
		"numthreads"sv,
		"shader"sv,
};

constexpr std::array g_semanticNames = {
		"SV_Position"sv,     "SV_Target"sv,        "SV_Depth"sv,       "SV_VertexID"sv,
		"SV_InstanceID"sv,   "SV_PrimitiveID"sv,   "SV_IsFrontFace"sv, "SV_DispatchThreadID"sv,
		"SV_GroupID"sv,      "SV_GroupThreadID"sv, "SV_GroupIndex"sv,  "SV_ClipDistance"sv,
		"SV_CullDistance"sv, "SV_Coverage"sv,      "SV_SampleIndex"sv, "SV_ViewID"sv,
		"POSITION"sv,        "NORMAL"sv,           "TANGENT"sv,        "BINORMAL"sv,
		"COLOR"sv,           "TEXCOORD"sv,         "BLENDWEIGHT"sv,    "BLENDINDICES"sv,
};

// Count of numbered variants written out for the semantics that take an index.
constexpr size_t g_semanticIndices = 8;

template<size_t N>
auto contains(const std::array<std::string_view, N>& iWords, const std::string_view iWord) -> bool {
	return std::ranges::find(iWords, iWord) != iWords.end();
}

auto isSemantic(const std::string_view iWord) -> bool {
	auto stem = iWord;
	while (!stem.empty() && std::isdigit(static_cast<unsigned char>(stem.back())) != 0) stem.remove_suffix(1);
	return contains(g_semanticNames, stem);
}
}// namespace

auto classifySlangWord(const std::string_view iWord) -> SlangWordKind {
	if (contains(g_keywords, iWord))
		return SlangWordKind::Keyword;
	if (isSemantic(iWord))
		return SlangWordKind::Semantic;
	if (contains(g_builtins, iWord))
		return SlangWordKind::Builtin;
	if (contains(g_types, iWord))
		return SlangWordKind::Type;
	// Scalar, vector and matrix types: float, float3, float4x4, uint2, int3x3...
	for (const auto base: {"bool"sv, "int"sv, "uint"sv, "float"sv, "half"sv, "double"sv}) {
		if (!iWord.starts_with(base) || iWord.size() == base.size())
			continue;
		const auto suffix = iWord.substr(base.size());
		const auto isDimension = [](const char iCh) -> bool { return iCh >= '1' && iCh <= '4'; };
		if (suffix.size() == 1 && isDimension(suffix[0]))
			return SlangWordKind::Type;
		if (suffix.size() == 3 && isDimension(suffix[0]) && suffix[1] == 'x' && isDimension(suffix[2]))
			return SlangWordKind::Type;
	}
	return SlangWordKind::None;
}

auto slangWords(const SlangWordKind iKind) -> std::vector<std::string> {
	std::vector<std::string> words;
	const auto append = [&words](const auto& iList) -> void {
		for (const auto word: iList) words.emplace_back(word);
	};
	switch (iKind) {
		case SlangWordKind::Keyword:
			append(g_keywords);
			break;
		case SlangWordKind::Builtin:
			append(g_builtins);
			break;
		case SlangWordKind::Type:
			{
				append(g_types);
				for (const auto* base: {"bool", "int", "uint", "float", "half", "double"}) {
					for (char rows = '1'; rows <= '4'; ++rows) {
						words.push_back(std::string{base} + rows);
						for (char columns = '1'; columns <= '4'; ++columns)
							words.push_back(std::string{base} + rows + 'x' + columns);
					}
				}
				break;
			}
		case SlangWordKind::Semantic:
			for (const auto name: g_semanticNames) {
				words.emplace_back(name);
				for (size_t index = 0; index < g_semanticIndices; ++index)
					words.push_back(std::string{name} + std::to_string(index));
			}
			break;
		case SlangWordKind::None:
			break;
	}
	return words;
}

}// namespace owl::nest::codeEditor
