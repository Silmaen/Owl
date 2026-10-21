// Renderer2D text — instanced, SSBO-driven. Same instance layout as
// `quad.slang` minus the tiling factor (text never tiles); the fragment
// shader runs the MSDF screenPxRange path instead of the plain sample.
// One drawcall per batch via `glDrawElementsInstanced(GL_TRIANGLES, 6,
// instanceCount)`. World matrix resolution follows the same scene /
// transient split as `quad.slang`.

struct Camera {
	column_major float4x4 viewProjection;
	// x: 1 when the target stores linear colour (the fragment colour is decoded from sRGB); yzw unused.
	int4 flags;
};

[[vk::binding(0)]]
ConstantBuffer<Camera> gCamera;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[32];

struct TextInstance {
	int worldIndex;
	float4 color;
	float2 uv[4];
	uint texIndex;
	int entityId;
	// Outline: reach in texture space per axis (0 = none, the glyph quad and its coordinates are already grown by it).
	float2 outlineUv;
	float4 outlineColor;
};

[[vk::binding(2)]]
StructuredBuffer<TextInstance> gInstances;

// One element type per buffer: the OpenGL SPIR-V path (NVIDIA) aliases storage buffers sharing a block type.
struct SceneWorld {
	column_major float4x4 world;
};

struct TransientWorld {
	column_major float4x4 world;
};

[[vk::binding(3)]]
StructuredBuffer<SceneWorld> gSceneWorlds;

[[vk::binding(4)]]
StructuredBuffer<TransientWorld> gTransientWorlds;

struct VertexInput {
	[[vk::location(0)]] int cornerIndex : POSITION;
};

struct FragmentInput {
	[[vk::location(0)]] float4 color : COLOR;
	[[vk::location(1)]] float2 texCoord : TEXCOORD0;
	[[vk::location(2)]] nointerpolation int texIndex : TEXCOORD1;
	[[vk::location(3)]] nointerpolation int entityID : TEXCOORD2;
	[[vk::location(4)]] nointerpolation float2 outlineUv : TEXCOORD3;
	[[vk::location(5)]] nointerpolation float4 outlineColor : COLOR1;
	// Texture-space bounds of the glyph cell in the atlas: min.xy, max.xy.
	[[vk::location(6)]] nointerpolation float4 cell : TEXCOORD4;
};

struct FragmentOutput {
	float4 color : SV_Target0;
	int entityID : SV_Target1;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

static const float2 kCorners[4] = {
	float2(-0.5f, -0.5f),
	float2( 0.5f, -0.5f),
	float2( 0.5f,  0.5f),
	float2(-0.5f,  0.5f),
};

// Distance range of the atlas in texels: must match `Font.cpp` (`g_pixelRange`).
static const float kPxRange = 6.0;

// Above this screen pixel range (about 40 px text for the 40 px atlas em) one sample per pixel is enough.
static const float kSupersampleLimit = 6.0;

// Screen pixels kept free at the saturated end of the field: an outline reads the distance only up to there.
static const float kFieldMargin = 1.0;

// Rotated-grid sample offsets inside a pixel, in pixels.
static const float2 kTapOffsets[4] = {
	float2(-0.375f, -0.125f),
	float2( 0.125f, -0.375f),
	float2( 0.375f,  0.125f),
	float2(-0.125f,  0.375f),
};

float median(float r, float g, float b) {
	return max(min(r, g), min(max(r, g), b));
}

// Signed distance to the glyph edge in screen pixels (positive inside); far outside where the atlas holds another
// glyph, i.e. outside the glyph cell (which holds half a distance range of field around the shape).
float signedDistance(Sampler2D iAtlas, float2 iUv, float2 iDx, float2 iDy, float iScreenPxRange, float4 iCell) {
	if (any(iUv < iCell.xy) || any(iUv > iCell.zw))
		return -iScreenPxRange - 1.0;
	float3 msd = iAtlas.SampleGrad(iUv, iDx, iDy).rgb;
	return iScreenPxRange * (median(msd.r, msd.g, msd.b) - 0.5);
}

// Coverage of the glyph grown by an outline reach (screen pixels): the signed distance gives a smooth antialiased band
// as long as the reach stays inside the field; the rest of a longer reach is a ring dilation of that band.
float outlineCoverage(Sampler2D iAtlas, float2 iUv, float2 iDx, float2 iDy, float iScreenPxRange, float4 iCell,
					  float iReachPx) {
	const float fieldReach = max(0.5 * iScreenPxRange - kFieldMargin, 0.0);
	const float inner = min(iReachPx, fieldReach);
	const float rest = iReachPx - inner;
	float widest = clamp(signedDistance(iAtlas, iUv, iDx, iDy, iScreenPxRange, iCell) + inner + 0.5, 0.0, 1.0);
	if (rest <= 0.0)
		return widest;
	for (int ring = 0; ring < 2; ++ring) {
		const float radius = ring == 0 ? 0.5 * rest : rest;
		for (int tap = 0; tap < 8; ++tap) {
			const float angle = (float(tap) + 0.5 * float(ring)) * (6.2831853 / 8.0);
			const float2 uv = iUv + (iDx * cos(angle) + iDy * sin(angle)) * radius;
			const float dist = signedDistance(iAtlas, uv, iDx, iDy, iScreenPxRange, iCell);
			widest = max(widest, clamp(dist + inner + 0.5, 0.0, 1.0));
		}
	}
	return widest;
}

float4x4 resolveWorldMatrix(int worldIndex) {
	if (worldIndex < 0) {
		return gTransientWorlds[-(worldIndex + 1)].world;
	}
	return gSceneWorlds[worldIndex].world;
}

// Exact piecewise sRGB decode, used when the target stores linear colour.
float3 srgbToLinear(float3 c) {
	return select(c <= 0.04045f, c / 12.92f, pow((c + 0.055f) / 1.055f, 2.4f));
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint iid : SV_InstanceID) {
	VertexOutput output;
	TextInstance inst = gInstances[iid];
	float4x4 world = resolveWorldMatrix(inst.worldIndex);
	float4 worldPos = mul(world, float4(kCorners[input.cornerIndex], 0.0f, 1.0f));
	output.svPosition = mul(gCamera.viewProjection, worldPos);
	output.frag.color = inst.color;
	output.frag.texCoord = inst.uv[input.cornerIndex];
	output.frag.texIndex = int(inst.texIndex);
	output.frag.entityID = inst.entityId;
	output.frag.outlineUv = inst.outlineUv;
	output.frag.outlineColor = inst.outlineColor;
	float2 uvLow = min(inst.uv[0], inst.uv[2]);
	float2 uvHigh = max(inst.uv[0], inst.uv[2]);
	output.frag.cell = float4(uvLow + inst.outlineUv, uvHigh - inst.outlineUv);
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 texColor = input.color;
	const int texIndex = NonUniformResourceIndex(input.texIndex);
	uint texW, texH;
	gTextures[texIndex].GetDimensions(texW, texH);
	float2 texDimensions = float2(texW, texH);
	float2 unitRange = float2(kPxRange) / texDimensions;
	float2 screenTexSize = float2(1.0) / fwidth(input.texCoord);
	float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);

	// Texture-space size of one screen pixel: the footprint that small text is supersampled over.
	float2 dx = ddx(input.texCoord);
	float2 dy = ddy(input.texCoord);

	// The glyph's atlas cell already holds half a distance range of field around the shape.
	const float4 cell = input.cell;

	float opacity;
	if (screenPxRange > kSupersampleLimit) {
		// Large text: the distance ramp spans several pixels, one sample is smooth.
		float dist = signedDistance(gTextures[texIndex], input.texCoord, dx, dy, screenPxRange, cell);
		opacity = clamp(dist + 0.5, 0.0, 1.0);
	} else {
		// Small text: the atlas is minified, so a single linear sample aliases. Four taps on a rotated grid over the
		// pixel footprint give the glyph a smooth coverage instead of ragged edges.
		opacity = 0.0;
		for (int tap = 0; tap < 4; ++tap) {
			float2 offset = kTapOffsets[tap];
			float2 uv = input.texCoord + dx * offset.x + dy * offset.y;
			float dist = signedDistance(gTextures[texIndex], uv, dx, dy, screenPxRange, cell);
			opacity += clamp(dist + 0.5, 0.0, 1.0);
		}
		opacity *= 0.25;
	}
	const bool outlined = input.outlineUv.x > 0.0 || input.outlineUv.y > 0.0;
	if (outlined) {
		// The outline is the glyph grown by its reach (screen pixels), drawn under the fill in the same pass.
		float reachPx = 0.5 * dot(input.outlineUv, screenTexSize);
		float fillAlpha = texColor.a * opacity;
		float reach = outlineCoverage(gTextures[texIndex], input.texCoord, dx, dy, screenPxRange, cell, reachPx);
		float outlineAlpha = input.outlineColor.a * max(reach, opacity);
		float alpha = fillAlpha + outlineAlpha * (1.0 - fillAlpha);
		if (alpha < 0.001)
			discard;
		float3 rgb = (texColor.rgb * fillAlpha + input.outlineColor.rgb * outlineAlpha * (1.0 - fillAlpha)) / alpha;
		if (gCamera.flags.x != 0)
			rgb = srgbToLinear(rgb);
		output.color = float4(rgb, alpha);
		output.entityID = input.entityID;
		return output;
	}
	if (opacity < 0.001)
		discard;

	output.color = float4(texColor.rgb, texColor.a * opacity);
	if (output.color.a < 0.001)
		discard;
	if (gCamera.flags.x != 0)
		output.color.rgb = srgbToLinear(output.color.rgb);

	output.entityID = input.entityID;
	return output;
}
