// Outline of a silhouette over the display target: texture 0 holds the depth of the outlined meshes alone (reversed-Z,
// 0 where empty), texture 1 the depth of the whole 3D scene. A pixel outside the silhouette with a covered pixel within
// the line width is on the outline: in full when the outlined surface there is in front of the scene, dimmed when other
// geometry hides it. Both textures are the size of the frame and read at the pixel centres (nearest).

struct OutlineParams {
	float4 color; // rgb colour of the line, a its opacity.
	float4 frame; // xy: frame size in pixels, z: line width in pixels, w: opacity of the hidden edges.
	int4 flags;   // x: 1 when texture 1 holds the scene depth.
};

[[vk::binding(0)]]
ConstantBuffer<OutlineParams> gParams;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[2];// 0 depth of the outlined meshes, 1 depth of the scene.

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

static const float MASK_EMPTY = 1e-7f;

float maskAt(float2 pixel) {
	return gTextures[0].SampleLevel((floor(pixel) + 0.5f) / gParams.frame.xy, 0.0f).r;
}

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input) {
	FragmentOutput output;
	output.entityID = -1;
	float2 pixel = input.svPosition.xy;
	if (maskAt(pixel) > MASK_EMPTY)
		discard;
	float width = gParams.frame.z;
	float strength = 0.0f;
	const int taps = 16;
	// A gap narrower than the line, covered on two opposite sides (the planks of a trapdoor), is inside the silhouette.
	for (int tap = 0; tap < taps / 2; ++tap) {
		float angle = 6.2831853f * float(tap) / float(taps);
		float2 offset = float2(cos(angle), sin(angle)) * width * 2.5f;
		if (maskAt(pixel + offset) > MASK_EMPTY && maskAt(pixel - offset) > MASK_EMPTY)
			discard;
	}
	for (int ring = 1; ring <= 2; ++ring) {
		float radius = width * float(ring) * 0.5f;
		for (int tap = 0; tap < taps; ++tap) {
			float angle = 6.2831853f * (float(tap) + 0.5f * float(ring)) / float(taps);
			float2 neighbour = pixel + float2(cos(angle), sin(angle)) * radius;
			float depth = maskAt(neighbour);
			if (depth <= MASK_EMPTY)
				continue;
			float visible = 1.0f;
			if (gParams.flags.x != 0) {
				float scene = gTextures[1].SampleLevel((floor(neighbour) + 0.5f) / gParams.frame.xy, 0.0f).r;
				visible = depth >= scene * 0.9995f ? 1.0f : gParams.frame.w;
			}
			strength = max(strength, visible);
		}
	}
	if (strength <= 0.0f)
		discard;
	output.color = float4(gParams.color.rgb, gParams.color.a * strength);
	return output;
}
