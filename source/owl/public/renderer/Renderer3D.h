/**
 * @file Renderer3D.h
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/vectors.h"
#include "renderer/Camera.h"
#include "renderer/EnvironmentMap.h"
#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/StorageBuffer.h"
#include "renderer/gpu/Texture.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace owl::renderer {

/**
 * @brief
 *  One vertex of a generic 3D mesh uploaded to `Renderer3D`.
 *
 * The layout mirrors the `mesh3d.slang` vertex input: object-space position,
 * normal (for the directional-light term), texture coordinate, the index of the
 * texture slot to sample from the bound array, an atlas sub-rect, and a
 * per-vertex ambient-occlusion term. The struct is
 * tightly packed (56 bytes) so it can be uploaded straight into a vertex buffer.
 */
struct Mesh3DVertex {
	/// Object-space position.
	math::vec3 position;
	/// Object-space normal (need not be normalized; the shader renormalizes).
	math::vec3 normal;
	/// Texture coordinate sampled from the selected texture slot.
	math::vec2 uv;
	/// Index into the bound texture array (`0` is the default white texture); `voxel` reads the packed block surface.
	uint32_t textureIndex = 0;
	/// Atlas sub-rect `(uMin, vMin, uSize, vSize)` the UV maps into (`mesh3d`), or tiled into with `frac` (`voxel`); `{0,0,1,1}` = full texture.
	math::vec4 tileRect{0.f, 0.f, 1.f, 1.f};
	/// Per-vertex ambient-occlusion multiplier of the lighting in `[0, 1]` (`1` = unoccluded).
	float ao = 1.f;
};

/**
 * @brief
 *  A light as the 3D shaders read it (one element of the light storage buffer, std430).
 */
struct GpuLight {
	/// xyz: world position (point, spot); w: range.
	math::vec4 positionRange{0.f, 0.f, 0.f, 10.f};
	/// xyz: direction the light travels, normalized (directional, spot); w: type (0 directional, 1 point, 2 spot).
	math::vec4 directionType{0.f, -1.f, 0.f, 0.f};
	/// rgb: linear colour times intensity; w unused.
	math::vec4 color{1.f, 1.f, 1.f, 0.f};
	/// x: cosine of the inner angle, y: cosine of the outer angle (spot); zw unused.
	math::vec4 spotCosines{1.f, 0.f, 0.f, 0.f};
};

/**
 * @brief
 *  The directional shadow the 3D shaders sample.
 */
struct ShadowSettings {
	/**
	 * @brief
	 *  Camera-relative view-projection of each cascade's light camera: its projection times its view without the
	 *  translation (`cameraRelativeView`).
	 */
	std::array<math::mat4, 4> relativeViewProjections{};
	/// World position of each cascade's light camera, the origin of its relative view-projection.
	std::array<math::vec3, 4> origins{};
	/// View depth where each cascade ends.
	math::vec4 splitDepths{0.f, 0.f, 0.f, 0.f};
	/// World size of a shadow-map texel, per cascade.
	math::vec4 texelWorldSizes{0.f, 0.f, 0.f, 0.f};
	/// Third row of the view matrix of the camera: view depth = -(row . position).
	math::vec4 viewDepthRow{0.f, 0.f, 0.f, 0.f};
	/// Shadow map of each cascade (`RedFloat`, normalized device depth).
	std::array<shared<gpu::Texture2D>, 4> maps{};
	/// Number of cascades, 0 for no shadow.
	uint32_t cascadeCount = 0;
	/// Index in the lights of the directional light casting the shadow.
	int32_t lightIndex = -1;
	/// Normal-offset bias, in texels.
	float bias = 1.5f;
	/// Size of a cascade's shadow map, in texels.
	uint32_t resolution = 1024;
};

/**
 * @brief
 *  Per-draw parameters of a mesh, written to the model uniform buffer next to the model matrix.
 */
class Material;

/**
 * @brief
 *  Image-based lighting the 3D shaders read: an equirectangular environment prefiltered by roughness and its diffuse
 *  harmonics.
 */
struct EnvironmentLighting {
	/// Prefiltered levels (level i for roughness i / (levelCount - 1)), RGBA float textures.
	std::array<shared<gpu::Texture2D>, 6> levels{};
	/// Number of valid levels, 0 without an environment.
	uint32_t levelCount = 0;
	/// Diffuse irradiance.
	SphericalHarmonics irradiance;
	/// Luminance, in cd/m², of a pixel value of 1 in the levels, or of a white `ambient`.
	float intensity = 1.f;
	/// Rotation of the environment about Y, radians.
	float rotation = 0.f;
	/// Linear colour of the constant sky lighting without levels, times `intensity`; empty keeps the layer ambient.
	std::optional<math::vec3> ambient;
};

/**
 * @brief
 *  Per-draw parameters of a mesh, written to the model uniform buffer next to the model matrix.
 */
struct MeshDrawParams {
	/// Colour multiplied with the base colour of the material (an entity tint).
	math::vec4 tint{1.f, 1.f, 1.f, 1.f};
	/// Value written to the entity-id attachment for picking; `-1` for none.
	int32_t entityId = -1;
	/// Material of the draw; null draws the default material with the textures of the draw.
	const Material* material = nullptr;
	/// Without a material: blend with the base-colour alpha instead of drawing opaque.
	bool blended = false;
};

/**
 * @brief
 *  One instance of an instanced mesh draw, as the `mesh3d` shader reads it (std430, 96 bytes).
 */
struct MeshInstance {
	/// Model (object-to-world) matrix.
	math::mat4 model = math::identity<float, 4>();
	/// Colour multiplied with the base colour of the material.
	math::vec4 tint{1.f, 1.f, 1.f, 1.f};
	/// x: entity id written for picking (`-1` for none); yzw unused.
	std::array<int32_t, 4> ids{-1, 0, 0, 0};
};

/**
 * @brief
 *  Where an instanced draw reads its instances: draw instance i is `instances[visible[visibleBase + i]]`.
 */
struct MeshInstanceBuffers {
	/// The `MeshInstance` records.
	shared<gpu::StorageBuffer> instances;
	/// Indices (one `uint32_t` each) of the instances to draw.
	shared<gpu::StorageBuffer> visible;
	/// First entry of the draw in `visible`.
	uint32_t visibleBase = 0;
};

/**
 * @brief
 *  Forward 3D renderer: depth-tested, PBR-shaded meshes lit by the scene lights, shadows and environment.
 *
 * A mesh is uploaded once via `createMesh` (returning an opaque GPU handle). `drawMesh` draws it with a model matrix;
 * `drawMeshInstanced` and `drawMeshIndirect` draw many instances of it with one material, reading the model, tint and
 * entity id of each instance from a storage buffer (`MeshInstance`). `RendererMesh` and `RendererVoxel` build on it.
 * Like `Renderer2D` it is a static facade with no instances; it owns its descriptor block and scene uniform buffer.
 */
class OWL_API Renderer3D final {
public:
	/// Opaque handle to a GPU-resident mesh created by `createMesh`.
	using MeshHandle = shared<gpu::DrawData>;

	/// Pipeline state of an opaque mesh: depth tested and written.
	static constexpr gpu::PipelineState opaqueMeshState{.depthTest = true, .depthWrite = true};

	/// Pipeline state of a blended mesh drawn back-to-front: depth tested, not written.
	static constexpr gpu::PipelineState transparentMeshState{.depthTest = true, .depthWrite = false};

	/**
	 * @brief
	 *  Initialize the renderer (descriptor block, scene UBO, default texture, shader).
	 */
	static void init();

	/**
	 * @brief
	 *  Release all GPU resources held by the renderer.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Begin a frame: bind the camera and enable depth testing.
	 * @param[in] iCamera The camera whose view-projection drives the frame.
	 */
	static void beginScene(const Camera& iCamera);

	/**
	 * @brief
	 *  End a frame: restore depth state for subsequent 2D layers.
	 */
	static void endScene();

	/**
	 * @brief
	 *  Set the lighting of the layer: its sun, lighting a scene that has no light, and its ambient, the constant sky
	 *  when the scene has no environment.
	 * @param[in] iSunDirection Direction the light travels (will be normalized).
	 * @param[in] iSunIlluminance Illuminance of the sun, in lux.
	 * @param[in] iAmbient Radiance of the constant sky, in cd/m² per channel.
	 */
	static void setLighting(const math::vec3& iSunDirection, float iSunIlluminance, const math::vec3& iAmbient);

	/**
	 * @brief
	 *  Set the exposure every physical light value is multiplied by before shading (lights, sun, sky, emissive).
	 * @param[in] iExposure The exposure, `exposureOf(ev100)` for a camera set to an EV100.
	 */
	static void setExposure(float iExposure);

	/**
	 * @brief
	 *  Get the exposure every physical light value is multiplied by.
	 * @return The exposure.
	 */
	[[nodiscard]] static auto getExposure() -> float;

	/**
	 * @brief
	 *  Exposure of a camera set to an exposure value at ISO 100: 1 / (1.2 · 2^EV100).
	 * @param[in] iEv100 The exposure value (15 sunny day, 4 candle-lit room).
	 * @return The exposure.
	 */
	[[nodiscard]] static auto exposureOf(float iEv100) -> float;

	/**
	 * @brief
	 *  Set the lights of the scene; with none, the meshes are lit by the sun of `setLighting`.
	 * @param[in] iLights The lights, at most `g_maxLights` are used.
	 */
	static void setSceneLights(std::span<const GpuLight> iLights);

	/**
	 * @brief
	 *  Choose whether the sun of `setLighting` lights the meshes when the scene gives no light (on by default).
	 * @param[in] iEnabled False keeps the meshes unlit by any sun, as when every light of a scene is off.
	 */
	static void setLayerSun(bool iEnabled);

	/**
	 * @brief
	 *  Whether the sun of `setLighting` lights the meshes when the scene gives no light.
	 * @return True when it does.
	 */
	[[nodiscard]] static auto isLayerSunEnabled() -> bool;

	/**
	 * @brief
	 *  Set a constant light added to every lit surface, unshadowed and independent of the environment (the editor
	 *  uses it to see dark places; black by default).
	 * @param[in] iRadiance Linear radiance added.
	 */
	static void setEditorAmbient(const math::vec3& iRadiance);

	/**
	 * @brief
	 *  Set the directional shadow the meshes sample (`cascadeCount` 0 for none).
	 * @param[in] iShadow The shadow.
	 */
	static void setShadow(const ShadowSettings& iShadow);

	/**
	 * @brief
	 *  Draw the next meshes into a shadow map: the shaders write the depth, the transparent meshes are skipped.
	 * @param[in] iShadowPass True while a shadow cascade renders.
	 */
	static void setShadowPass(bool iShadowPass);

	/**
	 * @brief
	 *  Set the image-based lighting; a `levelCount` of 0 lights the meshes with the layer ambient as a constant sky.
	 * @param[in] iEnvironment The environment.
	 */
	static void setEnvironment(const EnvironmentLighting& iEnvironment);

	/**
	 * @brief
	 *  Draw the environment as the sky over the whole target (no depth), for the camera of the last `beginScene`.
	 *  Nothing without an environment.
	 */
	static void drawSky();

	/**
	 * @brief
	 *  Check whether a shadow cascade renders.
	 * @return True during a shadow pass.
	 */
	[[nodiscard]] static auto isShadowPass() -> bool;

	/**
	 * @brief
	 *  Get the view-projection of the current pass (`beginScene`), a cascade's light camera in a shadow pass.
	 * @return The view-projection.
	 */
	[[nodiscard]] static auto getViewProjection() -> math::mat4;

	/// Most lights the meshes read.
	static constexpr uint32_t g_maxLights = 64;
	/// EV100 used without a camera (previews): a sunny day.
	static constexpr float g_defaultEv100 = 15.f;
	/// First texture slot of the shadow maps (one per cascade, up to the last slot).
	static constexpr uint32_t g_shadowMapSlot = 28;

	/**
	 * @brief
	 *  Get the default white texture bound to slot `0`, to bind where a mesh samples slot `1` without a texture.
	 * @return The texture, null before `init`.
	 */
	[[nodiscard]] static auto getWhiteTexture() -> shared<gpu::Texture2D>;

	/**
	 * @brief
	 *  Upload a mesh to the GPU and return a handle for later drawing.
	 *
	 * The mesh is static once created; rebuild it (create a new handle) when the
	 * geometry changes. An empty index span yields a handle that draws nothing.
	 * @param[in] iVertices The mesh vertices.
	 * @param[in] iIndices Triangle indices into `iVertices`.
	 * @param[in] iShaderName Shader used to draw the mesh (defaults to `mesh3d`; e.g. `voxel` for frac-tiled faces).
	 * @param[in] iState Pipeline state of every draw of the mesh (`transparentMeshState` for blended geometry).
	 * @return A handle usable with `drawMesh`.
	 */
	[[nodiscard]] static auto createMesh(std::span<const Mesh3DVertex> iVertices, std::span<const uint32_t> iIndices,
										 const std::string& iShaderName = "mesh3d",
										 const gpu::PipelineState& iState = opaqueMeshState) -> MeshHandle;

	/**
	 * @brief
	 *  Draw a previously created mesh with a model transform.
	 *
	 * Textures are bound to slots `1..N` (slot `0` is the default white texture);
	 * a vertex's `textureIndex` selects which slot it samples. A null handle or an
	 * empty texture span is a no-op / white-only draw.
	 * @param[in] iMesh The mesh handle from `createMesh`.
	 * @param[in] iModel The model (object-to-world) matrix.
	 * @param[in] iTextures Textures bound to slots `1..N` in order.
	 * @param[in] iParams Tint and entity id of the draw.
	 */
	static void drawMesh(const MeshHandle& iMesh, const math::mat4& iModel,
						 std::span<const shared<gpu::Texture2D>> iTextures = {}, const MeshDrawParams& iParams = {});

	/**
	 * @brief
	 *  Draw many meshes that share one model matrix and texture set.
	 *
	 * Sets the model UBO and binds the textures once, then issues a geometry-only
	 * draw per mesh — far cheaper than calling `drawMesh` per mesh when they share
	 * state (e.g. all chunks of one voxel world). Null / empty handles are skipped.
	 * @param[in] iMeshes The mesh handles to draw.
	 * @param[in] iModel The shared model (object-to-world) matrix.
	 * @param[in] iTextures Textures bound to slots `1..N` in order.
	 * @param[in] iParams Tint and entity id shared by the draws.
	 */
	static void drawMeshes(std::span<const MeshHandle> iMeshes, const math::mat4& iModel,
						   std::span<const shared<gpu::Texture2D>> iTextures = {}, const MeshDrawParams& iParams = {});

	/**
	 * @brief
	 *  Draw instances of a mesh with one material (`mesh3d` only), the instance count known on the CPU.
	 * @param[in] iMesh The mesh to draw.
	 * @param[in] iBuffers Where the instances are read.
	 * @param[in] iInstanceCount Number of instances drawn.
	 * @param[in] iTextures Textures without a material (slot 1 onward).
	 * @param[in] iParams Material and blending of the draw; tint and entity id come from the instances.
	 */
	static void drawMeshInstanced(const MeshHandle& iMesh, const MeshInstanceBuffers& iBuffers, uint32_t iInstanceCount,
								  std::span<const shared<gpu::Texture2D>> iTextures, const MeshDrawParams& iParams);

	/**
	 * @brief
	 *  Draw instances of a mesh with one material (`mesh3d` only), the instance count read on the GPU from an indirect
	 *  command a culling pass wrote.
	 * @param[in] iMesh The mesh to draw.
	 * @param[in] iBuffers Where the instances are read.
	 * @param[in] iCommands SSBO of `DrawIndexedIndirectCommand` records.
	 * @param[in] iCommandIndex Command of this draw.
	 * @param[in] iTextures Textures without a material (slot 1 onward).
	 * @param[in] iParams Material and blending of the draw; tint and entity id come from the instances.
	 */
	static void drawMeshIndirect(const MeshHandle& iMesh, const MeshInstanceBuffers& iBuffers,
								 const shared<gpu::StorageBuffer>& iCommands, uint32_t iCommandIndex,
								 std::span<const shared<gpu::Texture2D>> iTextures, const MeshDrawParams& iParams);
};

}// namespace owl::renderer
