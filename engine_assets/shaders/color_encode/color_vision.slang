// Last pass of a frame: reads the display image pixel for pixel, decodes it to linear RGB, applies the colour vision
// matrix (correction or simulation of a deficiency) and encodes it back. The entity id is copied afterwards, the id
// output only fills the slot.

struct VisionParams {
	column_major float4x4 matrix;// rgb rows: the linear RGB matrix, w unused.
	float4 frame;                // xy: size of the frame in pixels.
};

[[vk::binding(0)]]
ConstantBuffer<VisionParams> gParams;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[1];// 0 display image.

struct VertexInput {
	[[vk::location(0)]] float2 position : POSITION;
};

struct VertexOutput {
	float4 svPosition : SV_Position;
};

struct FragmentOutput {
	float4 color    : SV_Target0;
	int    entityID : SV_Target1;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;
	output.svPosition = float4(input.position, 0.0, 1.0);
	return output;
}

float3 srgbToLinear(float3 c) {
	return select(c <= 0.04045f, c / 12.92f, pow((c + 0.055f) / 1.055f, 2.4f));
}

float3 linearToSrgb(float3 c) {
	float3 low = c * 12.92;
	float3 high = 1.055 * pow(c, float3(1.0 / 2.4)) - 0.055;
	return float3(c.r <= 0.0031308 ? low.r : high.r, c.g <= 0.0031308 ? low.g : high.g,
				  c.b <= 0.0031308 ? low.b : high.b);
}

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input) {
	float2 uv = input.svPosition.xy / gParams.frame.xy;
	float4 display = gTextures[0].SampleLevel(uv, 0.0);
	float3 filtered = saturate(mul(gParams.matrix, float4(srgbToLinear(display.rgb), 0.0)).rgb);
	FragmentOutput output;
	output.color = float4(linearToSrgb(filtered), display.a);
	output.entityID = -1;
	return output;
}
