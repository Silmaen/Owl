// Renderer2D rounded box — instanced, SSBO-driven like `quad.slang`. One quad per box, grown past its edges by
// `margin` for the shadow and the antialiasing; the fragment shader evaluates the signed distance to the rounded
// rectangle in frame units (pixels for the HUD): fill with a vertical gradient, inner border, soft shadow outside,
// and an optional nine-slice texture whose borders keep their size while the middle stretches.

struct Camera {
	column_major float4x4 viewProjection;
	// x: 1 when the target stores linear colour (the fragment colour is decoded from sRGB); yzw unused here.
	int4 flags;
	float4 ambient;
};

[[vk::binding(0)]]
ConstantBuffer<Camera> gCamera;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[32];

struct BoxInstance {
	int worldIndex;
	uint texIndex;
	int entityId;
	float cornerRadius;
	float4 topColor;
	float4 bottomColor;
	float4 borderColor;
	float4 shadowColor;
	float2 size;
	float2 shadowOffset;
	float shadowBlur;
	float borderWidth;
	float margin;
	float pad0;
	float4 sliceInsets;// left, top, right, bottom, frame units.
	float4 sliceUv;    // the same borders as fractions of the texture.
	float4 uvRect;     // texture coordinates of the bottom-left (xy) and top-right (zw) corners.
};

[[vk::binding(2)]]
StructuredBuffer<BoxInstance> gInstances;

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
	[[vk::location(0)]] float2 local : POSITION1;
	[[vk::location(1)]] nointerpolation float4 topColor : COLOR0;
	[[vk::location(2)]] nointerpolation float4 bottomColor : COLOR1;
	[[vk::location(3)]] nointerpolation float4 borderColor : COLOR2;
	[[vk::location(4)]] nointerpolation float4 shadowColor : COLOR3;
	// xy: size, zw: shadow offset.
	[[vk::location(5)]] nointerpolation float4 sizeOffset : TEXCOORD0;
	// x: corner radius, y: shadow blur, z: border width.
	[[vk::location(6)]] nointerpolation float4 shape : TEXCOORD1;
	[[vk::location(7)]] nointerpolation float4 sliceInsets : TEXCOORD2;
	[[vk::location(8)]] nointerpolation float4 sliceUv : TEXCOORD3;
	[[vk::location(9)]] nointerpolation int texIndex : TEXCOORD4;
	[[vk::location(10)]] nointerpolation int entityID : TEXCOORD5;
	[[vk::location(11)]] nointerpolation float4 uvRect : TEXCOORD6;
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

// Signed distance to a rectangle of half size `halfSize` with rounded corners of radius `radius`.
float roundedBoxDistance(float2 p, float2 halfSize, float radius) {
	float2 q = abs(p) - halfSize + radius;
	return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - radius;
}

// Texture coordinate along one axis of a nine-slice: the borders keep their size, the middle stretches.
float sliceAxis(float x, float len, float lo, float hi, float uvLo, float uvHi) {
	float fit = min(1.0f, len / max(lo + hi, 1e-4f));
	lo *= fit;
	hi *= fit;
	if (x < lo)
		return x / max(lo, 1e-4f) * uvLo;
	if (x > len - hi)
		return 1.0f - (len - x) / max(hi, 1e-4f) * uvHi;
	return uvLo + (x - lo) / max(len - lo - hi, 1e-4f) * (1.0f - uvLo - uvHi);
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint iid : SV_InstanceID) {
	VertexOutput output;
	BoxInstance inst = gInstances[iid];
	float4x4 world = resolveWorldMatrix(inst.worldIndex);
	float2 grown = kCorners[input.cornerIndex] * (inst.size + 2.0f * inst.margin);
	float4 worldPos = mul(world, float4(grown / inst.size, 0.0f, 1.0f));
	output.svPosition = mul(gCamera.viewProjection, worldPos);
	output.frag.local = grown;
	output.frag.topColor = inst.topColor;
	output.frag.bottomColor = inst.bottomColor;
	output.frag.borderColor = inst.borderColor;
	output.frag.shadowColor = inst.shadowColor;
	output.frag.sizeOffset = float4(inst.size, inst.shadowOffset);
	output.frag.shape = float4(inst.cornerRadius, inst.shadowBlur, inst.borderWidth, 0.0f);
	output.frag.sliceInsets = inst.sliceInsets;
	output.frag.sliceUv = inst.sliceUv;
	output.frag.uvRect = inst.uvRect;
	output.frag.texIndex = int(inst.texIndex);
	output.frag.entityID = inst.entityId;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float2 p = input.local;
	float2 size = input.sizeOffset.xy;
	float radius = input.shape.x;
	float blur = input.shape.y;
	float borderWidth = input.shape.z;
	float2 halfSize = size * 0.5f;
	float distance = roundedBoxDistance(p, halfSize, radius);
	float pixel = max(fwidth(distance), 1e-4f);
	float coverage = saturate(0.5f - distance / pixel);

	float t = saturate(0.5f - p.y / size.y);
	float4 fill = lerp(input.topColor, input.bottomColor, t);
	float u = sliceAxis(p.x + halfSize.x, size.x, input.sliceInsets.x, input.sliceInsets.z, input.sliceUv.x,
						input.sliceUv.z);
	float v = sliceAxis(halfSize.y - p.y, size.y, input.sliceInsets.y, input.sliceInsets.w, input.sliceUv.y,
						input.sliceUv.w);
	fill *= gTextures[NonUniformResourceIndex(input.texIndex)].SampleLevel(
			lerp(input.uvRect.xy, input.uvRect.zw, saturate(float2(u, 1.0f - v))), 0.0f);
	if (borderWidth > 0.0f) {
		float border = saturate(0.5f + (distance + borderWidth) / pixel);
		fill = lerp(fill, input.borderColor, border * input.borderColor.a);
		fill.a = max(fill.a, border * input.borderColor.a);
	}
	float boxAlpha = fill.a * coverage;

	float shadowAlpha = 0.0f;
	if (input.shadowColor.a > 0.0f) {
		float shadowDistance = roundedBoxDistance(p - input.sizeOffset.zw, halfSize, radius);
		float soft = max(blur, pixel);
		shadowAlpha = input.shadowColor.a * (1.0f - smoothstep(-soft, soft, shadowDistance)) * (1.0f - coverage);
	}

	float alpha = boxAlpha + shadowAlpha * (1.0f - boxAlpha);
	if (alpha < 0.001f)
		discard;
	float3 rgb = (fill.rgb * boxAlpha + input.shadowColor.rgb * shadowAlpha * (1.0f - boxAlpha)) / alpha;
	if (gCamera.flags.x != 0)
		rgb = srgbToLinear(rgb);
	output.color = float4(rgb, alpha);
	output.entityID = input.entityID;
	return output;
}
