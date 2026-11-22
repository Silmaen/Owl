/**
 * @file shaderFileUtils.cpp
 * @author Silmaen
 * @date 06/02/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "app/Application.h"
#include "data/assets/AssetSearchPaths.h"
#include "platform/UserDirectory.h"
#include "shaderFileUtils.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wswitch-enum")
OWL_DIAG_DISABLE_CLANG("-Wdouble-promotion")
OWL_DIAG_DISABLE_CLANG("-Wsign-conversion")
#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>
OWL_DIAG_POP

#include "renderer/Renderer.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <exception>
#include <expected>
#include <format>
#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <string_view>
#include <system_error>
#include <vector>

namespace owl::renderer::utils {

auto getCacheDirectory(const std::string& iRenderer, const std::string& iRendererApi) -> std::filesystem::path {
	auto output = platform::getCacheDirectory() / "shader";
	if (!iRenderer.empty())
		output /= iRenderer;
	if (!iRendererApi.empty())
		output /= iRendererApi;
	return output;
}

void createCacheDirectoryIfNeeded(const std::string& iRenderer, const std::string& iRendererApi) {
	if (const std::filesystem::path cacheDirectory = getCacheDirectory(iRenderer, iRendererApi);
		!exists(cacheDirectory)) {
		create_directories(cacheDirectory);
		if (!exists(cacheDirectory))
			OWL_CORE_WARN("Shader: Cannot create the cache directory '{}', the shaders are not cached.",
						  cacheDirectory.string())
	}
}

auto getShaderCachedPath(const std::string& iShaderName, const std::string& iRenderer, const std::string& iRendererApi,
						 const gpu::ShaderType& iType) -> std::filesystem::path {
	return getCacheDirectory(iRenderer, iRendererApi) / (iShaderName + getCacheExtension(iType));
}

auto getShaderPath(const std::string& iShaderName, const std::string& iRenderer, const std::string& iRendererApi,
				   const gpu::ShaderType& iType) -> std::filesystem::path {
	return Renderer::getTextureLibrary()
			.find(std::format("shaders/{}/{}/{}{}", iRenderer, iRendererApi, iShaderName, getExtension(iType)))
			.value_or(std::filesystem::path{});
}

auto getRelativeShaderPath(const std::string& iShaderName, const std::string& iRenderer,
						   const std::string& iRendererApi, const gpu::ShaderType& iType) -> std::filesystem::path {
	return std::filesystem::path("shaders") / iRenderer / iRendererApi / (iShaderName + getExtension(iType));
}

auto getExtension(const gpu::ShaderType& iStage) -> std::string {
	auto ext = std::format(".{}", magic_enum::enum_name(iStage).substr(0, 4));
	std::ranges::transform(ext.begin(), ext.end(), ext.begin(),
						   [](const unsigned char iChar) -> int { return std::tolower(iChar); });
	return ext;
}

auto getCacheExtension(const gpu::ShaderType& iStage) -> std::string {
	auto ext = std::format(".{}.spv", magic_enum::enum_name(iStage).substr(0, 4));
	std::ranges::transform(ext.begin(), ext.end(), ext.begin(),
						   [](const unsigned char iChar) -> int { return std::tolower(iChar); });
	return ext;
}

namespace {
std::mutex g_inMemoryShaderCacheMutex;
std::unordered_map<std::string, std::vector<uint32_t>> g_inMemoryShaderCache;
}// namespace

auto readCachedShader(const std::filesystem::path& iFile) -> std::vector<uint32_t> {
	OWL_PROFILE_FUNCTION()

	const std::string key = iFile.generic_string();
	{
		const std::lock_guard<std::mutex> lock{g_inMemoryShaderCacheMutex};
		if (const auto it = g_inMemoryShaderCache.find(key); it != g_inMemoryShaderCache.end())
			return it->second;
	}
	std::vector<uint32_t> result;
	std::ifstream in(iFile, std::ios::in | std::ios::binary);
	in.seekg(0, std::ios::end);
	const auto size = in.tellg();
	in.seekg(0, std::ios::beg);
	result.resize(static_cast<size_t>(size) / sizeof(uint32_t));
	in.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size() * sizeof(uint32_t)));
	in.close();
	{
		const std::lock_guard<std::mutex> lock{g_inMemoryShaderCacheMutex};
		g_inMemoryShaderCache.insert_or_assign(key, result);
	}
	return result;
}

auto writeCachedShader(const std::filesystem::path& iFile, const std::vector<uint32_t>& iData) -> bool {
	OWL_PROFILE_FUNCTION()

	std::ofstream out(iFile, std::ios::out | std::ios::binary);

	if (out.is_open()) {
		out.write(reinterpret_cast<const char*>(iData.data()),
				  static_cast<std::streamsize>(iData.size() * sizeof(uint32_t)));
		out.flush();
		out.close();
		const std::lock_guard<std::mutex> lock{g_inMemoryShaderCacheMutex};
		g_inMemoryShaderCache.insert_or_assign(iFile.generic_string(), iData);
		return true;
	}
	OWL_CORE_WARN("Shader: Cannot open '{}' for writing.", iFile.string())
	return false;
}

auto shaderReflect(const std::string& iShaderName, const std::string& iRenderer, const std::string& iRendererApi,
				   const gpu::ShaderType iStage, const std::vector<uint32_t>& iShaderData) -> ShaderReflectionData {
	ShaderReflectionData result;
	OWL_CORE_TRACE("Shader: Reflecting the {0} stage of '<assets>/{1}'.", magic_enum::enum_name(iStage),
				   renderer::utils::getRelativeShaderPath(iShaderName, iRenderer, iRendererApi, iStage).string())
	if (iShaderData.empty())
		return result;
	const spirv_cross::Compiler compiler(iShaderData);
	const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
	OWL_CORE_TRACE("Shader: {} sampled images.", resources.sampled_images.size())
	for (const auto& resource: resources.uniform_buffers) {
		const auto& bufferType = compiler.get_type(resource.base_type_id);
		const auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
		const auto size = compiler.get_declared_struct_size(bufferType);
		OWL_CORE_TRACE("Shader: Uniform buffer '{}' size={} binding={} members={}.", resource.name, size, binding,
					   bufferType.member_types.size())
		result.uniformBuffers.push_back({.name = resource.name,
										 .binding = binding,
										 .size = size,
										 .memberCount = bufferType.member_types.size()});
	}
	for (const auto& resource: resources.sampled_images) {
		const auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
		const auto& type = compiler.get_type(resource.type_id);
		const uint32_t descriptorCount = type.array.empty() ? 1 : type.array[0];
		OWL_CORE_TRACE("Shader: Sampled image '{}' binding={} count={}.", resource.name, binding, descriptorCount)
		result.sampledImages.push_back({.name = resource.name, .binding = binding, .descriptorCount = descriptorCount});
	}
	return result;
}

auto computeShaderHash(const std::string& iSource) -> std::string {
	const std::hash<std::string> hasher;
	return std::to_string(hasher(iSource));
}

auto getShaderTargetFolder(const ShaderTarget iTarget) -> std::string {
	switch (iTarget) {
		case ShaderTarget::Vulkan:
			return "vulkan";
		case ShaderTarget::OpenGl:
			return "opengl";
		case ShaderTarget::VulkanAndroid:
			return "vulkan_android";
		case ShaderTarget::WebGpu:
			return "webgpu";
	}
	return "unknown";
}

auto parseShaderTarget(const std::string_view iFolder) -> std::optional<ShaderTarget> {
	for (const auto target:
		 {ShaderTarget::Vulkan, ShaderTarget::OpenGl, ShaderTarget::VulkanAndroid, ShaderTarget::WebGpu}) {
		if (getShaderTargetFolder(target) == iFolder)
			return target;
	}
	return std::nullopt;
}

auto getShaderTargetProfile(const ShaderTarget iTarget) -> std::string_view {
	switch (iTarget) {
		case ShaderTarget::Vulkan:
			return "spirv_1_6";
		case ShaderTarget::OpenGl:
			return "glsl_450";
		case ShaderTarget::VulkanAndroid:
			return "spirv_1_3";
		case ShaderTarget::WebGpu:
			return "wgsl";
	}
	return "";
}

auto getShaderTargetMacro(const ShaderTarget iTarget) -> std::string_view {
	switch (iTarget) {
		case ShaderTarget::Vulkan:
		case ShaderTarget::VulkanAndroid:
			return "BACKEND_VULKAN";
		case ShaderTarget::OpenGl:
			return "BACKEND_OPENGL";
		case ShaderTarget::WebGpu:
			return "BACKEND_WEBGPU";
	}
	return "";
}

auto getShaderCacheKey(const std::string& iSource, const std::string& iModuleName, const ShaderTarget iTarget)
		-> std::string {
	// The WGSL is edited after Slang (`OwlShaderCompiler`): its revision keys the WebGPU stages, the others keep theirs.
	constexpr std::string_view wgslRevision = ";wgsl=1";
	return std::format("backend={};module={};slang={};macros={}=1;profile={}{};source={}",
					   getShaderTargetFolder(iTarget), iModuleName, OWL_SLANG_VERSION, getShaderTargetMacro(iTarget),
					   getShaderTargetProfile(iTarget),
					   iTarget == ShaderTarget::WebGpu ? wgslRevision : std::string_view{}, iSource);
}

auto remapBuiltinsForOpenGl(std::vector<uint32_t>& ioSpirv) -> uint32_t {
	constexpr size_t headerWords = 5;
	constexpr uint32_t opExecutionMode = 16;
	constexpr uint32_t opDecorate = 71;
	constexpr uint32_t decorationBuiltIn = 11;
	uint32_t patched = 0;
	for (size_t i = headerWords; i < ioSpirv.size();) {
		const uint32_t wordCount = ioSpirv[i] >> 16u;
		if (wordCount == 0)
			break;
		if ((ioSpirv[i] & 0xffffu) == opExecutionMode && wordCount >= 3 && i + 2 < ioSpirv.size() &&
			ioSpirv[i + 2] == spv::ExecutionModeOriginUpperLeft) {
			ioSpirv[i + 2] = spv::ExecutionModeOriginLowerLeft;
			++patched;
		} else if ((ioSpirv[i] & 0xffffu) == opDecorate && wordCount >= 4 && i + 3 < ioSpirv.size() &&
				   ioSpirv[i + 2] == decorationBuiltIn) {
			auto& builtin = ioSpirv[i + 3];
			if (builtin == spv::BuiltInInstanceIndex) {
				builtin = spv::BuiltInInstanceId;
				++patched;
			} else if (builtin == spv::BuiltInVertexIndex) {
				builtin = spv::BuiltInVertexId;
				++patched;
			}
		}
		i += wordCount;
	}
	return patched;
}

auto crossCompileToGlsl(const std::vector<uint32_t>& iSpirv, const std::string& iName) -> std::optional<std::string> {
	OWL_PROFILE_FUNCTION()

	if (iSpirv.empty()) {
		OWL_CORE_ERROR("Shader: '{}' has an empty SPIR-V, no GLSL to generate.", iName)
		return std::nullopt;
	}
	std::string glsl;
	try {
		spirv_cross::CompilerGLSL compiler(iSpirv);
		spirv_cross::CompilerGLSL::Options options;
		options.version = 450;
		options.es = false;
		options.vulkan_semantics = false;
		options.vertex.support_nonzero_base_instance = false;
		compiler.set_common_options(options);
		// Slang names every storage block `StructuredBuffer`: the GL linker would match a vertex and a fragment one.
		for (const auto& buffer: compiler.get_shader_resources().storage_buffers)
			compiler.set_name(buffer.base_type_id, std::format("{}_block", compiler.get_name(buffer.id)));
		glsl = compiler.compile();
	} catch (const std::exception& iEx) {
		OWL_CORE_ERROR("Shader: SPIR-V to GLSL translation of '{}' failed ({}).", iName, iEx.what())
		return std::nullopt;
	}
	// NonUniformResourceIndex maps to GL_NV_gpu_shader5; GL drivers without it index sampler arrays natively.
	constexpr std::string_view nvRequire = "#extension GL_NV_gpu_shader5 : require\n";
	if (const auto pos = glsl.find(nvRequire); pos != std::string::npos)
		glsl.replace(pos, nvRequire.size(),
					 "#ifdef GL_NV_gpu_shader5\n#extension GL_NV_gpu_shader5 : enable\n#endif\n");
	return glsl;
}

auto isShaderCacheValid(const std::filesystem::path& iCachedPath, const std::string& iSource) -> bool {
	if (!exists(iCachedPath))
		return false;
	const auto hashPath = std::filesystem::path(iCachedPath.string() + ".hash");
	if (!exists(hashPath))
		return false;
	std::ifstream in(hashPath, std::ios::in);
	if (!in.is_open())
		return false;
	std::string storedHash;
	std::getline(in, storedHash);
	in.close();
	return storedHash == computeShaderHash(iSource);
}

void writeShaderHash(const std::filesystem::path& iCachedPath, const std::string& iSource) {
	const auto hashPath = std::filesystem::path(iCachedPath.string() + ".hash");
	std::ofstream out(hashPath, std::ios::out);
	if (out.is_open()) {
		out << computeShaderHash(iSource);
		out.close();
	}
}


namespace {
// An include as the loaders find it: on disk, in the open pack, or relative to an asset folder.
auto isReadableAsset(const std::filesystem::path& iPath) -> bool {
	std::error_code error;
	if (std::filesystem::is_regular_file(iPath, error) || data::assets::isPackedAsset(iPath))
		return true;
	if (iPath.is_absolute())
		return false;
	return std::ranges::any_of(data::assets::getAssetSearchPaths(), [&iPath](const auto& iFolder) -> bool {
		std::error_code folderError;
		return std::filesystem::is_regular_file(iFolder / iPath, folderError);
	});
}

// NOLINTNEXTLINE(misc-no-recursion): each file is expanded at most once.
void expandIncludes(const std::filesystem::path& iFile, std::vector<std::filesystem::path>& ioSeen, std::string& oOut) {
	const auto text = data::assets::readAssetText(iFile);
	if (!text) {
		OWL_CORE_WARN("Shader: Cannot read '{}'.", iFile.string())
		return;
	}
	ioSeen.push_back(iFile.lexically_normal());
	std::istringstream in{*text};
	std::string line;
	while (std::getline(in, line)) {
		const auto start = line.find_first_not_of(" \t");
		if (start == std::string::npos || line.compare(start, 8, "#include") != 0) {
			oOut.append(line).push_back('\n');
			continue;
		}
		const auto open = line.find('"', start);
		const auto close = open == std::string::npos ? open : line.find('"', open + 1);
		if (close == std::string::npos) {
			oOut.append(line).push_back('\n');
			continue;
		}
		const std::string name = line.substr(open + 1, close - open - 1);
		std::filesystem::path found;
		for (const auto& candidate: {iFile.parent_path() / name, iFile.parent_path().parent_path() / "common" / name}) {
			if (isReadableAsset(candidate)) {
				found = candidate.lexically_normal();
				break;
			}
		}
		if (found.empty()) {
			OWL_CORE_WARN("Shader: Include '{}' of '{}' not found.", name, iFile.string())
			continue;
		}
		if (std::ranges::find(ioSeen, found) == ioSeen.end())
			expandIncludes(found, ioSeen, oOut);
	}
}
}// namespace

auto readShaderSource(const std::filesystem::path& iFile) -> std::string {
	std::string out;
	std::vector<std::filesystem::path> seen;
	expandIncludes(iFile, seen, out);
	return out;
}

namespace {
auto installedCompiler() -> ShaderCompiler& {
	static ShaderCompiler compiler;
	return compiler;
}
}// namespace

auto setShaderCompiler(ShaderCompiler iCompiler) -> ShaderCompiler {
	return std::exchange(installedCompiler(), std::move(iCompiler));
}

auto hasShaderCompiler() -> bool { return static_cast<bool>(installedCompiler()); }

auto compileShader(const std::string& iSource, const std::string& iModuleName, const ShaderTarget iTarget)
		-> SlangCompilationResult {
	if (!hasShaderCompiler()) {
		OWL_CORE_ERROR("Shader: Cannot compile '{}' for {}, this program has no shader compiler.", iModuleName,
					   getShaderTargetFolder(iTarget))
		return {};
	}
	return installedCompiler()(iSource, iModuleName, iTarget);
}

auto getPrecompiledWgslPath(const std::string& iShaderName, const std::string& iRenderer, const gpu::ShaderType& iType)
		-> std::filesystem::path {
	auto file = std::filesystem::path("shaders") / iRenderer / "wgsl" / (iShaderName + getCacheExtension(iType));
	return file.replace_extension(".wgsl");
}

auto getPrecompiledShaderPath(const std::string& iShaderName, const std::string& iRenderer,
							  const std::string& iRendererApi, const gpu::ShaderType& iType) -> std::filesystem::path {
	return std::filesystem::path("shaders") / iRenderer / "spirv" / iRendererApi /
		   (iShaderName + getCacheExtension(iType));
}

auto checkSpirv(const std::vector<uint32_t>& iSpirv, const gpu::ShaderType iStage) -> std::optional<std::string> {
	constexpr size_t headerWords = 5;
	constexpr uint32_t opMemoryModel = 14;
	constexpr uint32_t opEntryPoint = 15;
	constexpr uint32_t opFunctionEnd = 56;
	if (iSpirv.empty())
		return "empty";
	if (iSpirv.size() < headerWords)
		return std::format("{} words, shorter than the SPIR-V header", iSpirv.size());
	if (iSpirv[0] != spv::MagicNumber)
		return std::format("magic number {:#010x} instead of {:#010x}", iSpirv[0], spv::MagicNumber);
	if (iSpirv[3] == 0)
		return "zero id bound";
	const auto model = [iStage]() -> std::optional<uint32_t> {
		switch (iStage) {
			case gpu::ShaderType::Vertex:
				return spv::ExecutionModelVertex;
			case gpu::ShaderType::Geometry:
				return spv::ExecutionModelGeometry;
			case gpu::ShaderType::Fragment:
				return spv::ExecutionModelFragment;
			case gpu::ShaderType::Compute:
				return spv::ExecutionModelGLCompute;
			case gpu::ShaderType::None:
				break;
		}
		return std::nullopt;
	}();
	bool memoryModel = false;
	bool entryPoint = false;
	uint32_t lastOpCode = 0;
	for (size_t i = headerWords; i < iSpirv.size();) {
		const uint32_t wordCount = iSpirv[i] >> 16u;
		if (wordCount == 0)
			return std::format("null instruction at word {}", i);
		if (i + wordCount > iSpirv.size())
			return std::format("truncated: the instruction at word {} needs {} words, {} left", i, wordCount,
							   iSpirv.size() - i);
		const uint32_t opCode = iSpirv[i] & 0xffffu;
		lastOpCode = opCode;
		memoryModel = memoryModel || opCode == opMemoryModel;
		entryPoint = entryPoint ||
					 (opCode == opEntryPoint && wordCount >= 2 && (!model.has_value() || iSpirv[i + 1] == *model));
		i += wordCount;
	}
	// Function definitions come last, and an entry point needs one: a module cut between two instructions ends early.
	if (lastOpCode != opFunctionEnd)
		return "truncated: the module does not end with OpFunctionEnd";
	if (!memoryModel || !entryPoint)
		return model.has_value() ? std::format("no OpMemoryModel or no {} entry point", magic_enum::enum_name(iStage))
								 : std::string{"no OpMemoryModel or no OpEntryPoint"};
	return std::nullopt;
}

namespace {
// Why stored SPIR-V is not used.
struct StoredProblem {
	// Kind of refusal, from the least to the most severe.
	enum struct Kind : uint8_t {
		// No file: a normal miss, not logged.
		Absent,
		// Key of another source or Slang version: expected after an edit, logged as information.
		Outdated,
		// Unreadable, truncated or garbled file: logged as a warning.
		Damaged
	};
	// Kind of refusal.
	Kind kind = Kind::Absent;
	// Files and reasons, for the log.
	std::string reason;
};

// Read one stored stage when its key matches and its content is a well-formed module of that stage.
auto readStoredStage(const std::filesystem::path& iFile, const std::string& iCacheKey, const gpu::ShaderType iStage)
		-> std::expected<std::vector<uint32_t>, StoredProblem> {
	using enum StoredProblem::Kind;
	// Read from disk or the pack, not the in-memory cache: a file replaced since must be checked again.
	const auto bytes = data::assets::readAssetBytes(iFile);
	if (!bytes)
		return std::unexpected{StoredProblem{.kind = Absent, .reason = "absent"}};
	const auto storedKey = data::assets::readAssetText(iFile.string() + ".hash");
	if (!storedKey || storedKey->substr(0, storedKey->find('\n')) != computeShaderHash(iCacheKey))
		return std::unexpected{
				StoredProblem{.kind = Outdated, .reason = "key of another source or Slang version, or no .hash"}};
	if (bytes->empty())
		return std::unexpected{StoredProblem{.kind = Damaged, .reason = "empty (0 bytes)"}};
	if (bytes->size() % sizeof(uint32_t) != 0)
		return std::unexpected{
				StoredProblem{.kind = Damaged,
							  .reason = std::format("{} bytes, not a whole number of SPIR-V words", bytes->size())}};
	std::vector<uint32_t> words(bytes->size() / sizeof(uint32_t));
	std::memcpy(words.data(), bytes->data(), bytes->size());
	if (auto problem = checkSpirv(words, iStage); problem.has_value())
		return std::unexpected{StoredProblem{.kind = Damaged, .reason = std::move(*problem)}};
	return words;
}

// Read every stage of a shader from one folder; absent only when no stage file exists, else the worst refusal.
auto readStoredStages(const std::vector<gpu::ShaderType>& iStages, const std::string& iCacheKey,
					  const std::function<std::filesystem::path(gpu::ShaderType)>& iPathOf)
		-> std::expected<SpirvStages, StoredProblem> {
	SpirvStages stages;
	StoredProblem worst;
	size_t absent = 0;
	for (const auto stage: iStages) {
		const auto file = iPathOf(stage);
		auto words = readStoredStage(file, iCacheKey, stage);
		if (words.has_value()) {
			stages[stage] = std::move(*words);
			continue;
		}
		if (words.error().kind == StoredProblem::Kind::Absent)
			++absent;
		worst.kind = std::max(worst.kind, words.error().kind);
		worst.reason += std::format("{}{}: {}", worst.reason.empty() ? "" : "; ", file.string(), words.error().reason);
	}
	if (absent == iStages.size())
		return std::unexpected{StoredProblem{}};
	if (!worst.reason.empty()) {
		// A stage missing beside present ones is a damaged set.
		if (absent > 0)
			worst.kind = StoredProblem::Kind::Damaged;
		return std::unexpected{worst};
	}
	return stages;
}

// Compile a shader from its Slang source and store the result in the working directory cache.
auto compileAndCache(const std::string& iSource, const std::string& iShaderName, const std::string& iRenderer,
					 const ShaderTarget iTarget) -> std::optional<LoadedSpirv> {
	const std::string api = getShaderTargetFolder(iTarget);
	auto compiled = compileShader(iSource, iShaderName, iTarget);
	if (!compiled.success) {
		OWL_CORE_ERROR("Slang: Compilation of shader '{}/{}' ({}) failed.", iRenderer, iShaderName, api)
		return std::nullopt;
	}
	if (app::Application::instanced()) {
		const auto cacheKey = getShaderCacheKey(iSource, iRenderer + "/" + iShaderName, iTarget);
		createCacheDirectoryIfNeeded(iRenderer, api);
		for (const auto& [stage, data]: compiled.spirvData) {
			const auto cachedPath = getShaderCachedPath(iShaderName, iRenderer, api, stage);
			if (!writeCachedShader(cachedPath, data))
				OWL_CORE_WARN("Shader: Cannot write the compiled shader '{}'.", cachedPath.string())
			writeShaderHash(cachedPath, cacheKey);
		}
	}
	return LoadedSpirv{.stages = std::move(compiled.spirvData), .origin = {}};
}
}// namespace

auto loadOrCompileSpirv(const std::string& iSource, const std::string& iShaderName, const std::string& iRenderer,
						const ShaderTarget iTarget, const std::vector<gpu::ShaderType>& iStages)
		-> std::optional<LoadedSpirv> {
	OWL_PROFILE_FUNCTION()

	const std::string api = getShaderTargetFolder(iTarget);
	const auto cacheKey = getShaderCacheKey(iSource, iRenderer + "/" + iShaderName, iTarget);
	bool damaged = false;
	// The cache is ours: a refused entry is removed, so that it is not refused again at every start.
	const auto tryFolder = [&](const std::function<std::filesystem::path(gpu::ShaderType)>& iPathOf,
							   const bool iRemoveRefused) -> std::optional<LoadedSpirv> {
		auto stages = readStoredStages(iStages, cacheKey, iPathOf);
		if (stages.has_value())
			return LoadedSpirv{.stages = std::move(*stages), .origin = iPathOf(iStages.front()).parent_path()};
		if (iRemoveRefused && stages.error().kind != StoredProblem::Kind::Absent) {
			std::error_code error;
			for (const auto stage: iStages) {
				std::filesystem::remove(iPathOf(stage), error);
				std::filesystem::remove(iPathOf(stage).string() + ".hash", error);
			}
		}
		if (stages.error().kind == StoredProblem::Kind::Damaged) {
			OWL_CORE_WARN("Shader: Stored SPIR-V of '{}/{}' ({}) rejected, {}.", iRenderer, iShaderName, api,
						  stages.error().reason)
			damaged = true;
		} else if (stages.error().kind == StoredProblem::Kind::Outdated) {
			OWL_CORE_DEBUG("Shader: Stored SPIR-V of '{}/{}' ({}) outdated, {}.", iRenderer, iShaderName, api,
						   stages.error().reason)
		}
		return std::nullopt;
	};
	if (iStages.empty())
		return compileAndCache(iSource, iShaderName, iRenderer, iTarget);
	if (app::Application::instanced()) {
		if (auto loaded = tryFolder(
					[&](const gpu::ShaderType iStage) -> std::filesystem::path {
						return getShaderCachedPath(iShaderName, iRenderer, api, iStage);
					},
					true);
			loaded.has_value()) {
			OWL_CORE_TRACE("Shader: Using the cached {} shader '{}/{}'.", api, iRenderer, iShaderName)
			return loaded;
		}
	}
	if (data::assets::isPackedAsset(getPrecompiledShaderPath(iShaderName, iRenderer, api, iStages.front()))) {
		if (auto loaded = tryFolder(
					[&](const gpu::ShaderType iStage) -> std::filesystem::path {
						return getPrecompiledShaderPath(iShaderName, iRenderer, api, iStage);
					},
					false);
			loaded.has_value()) {
			OWL_CORE_TRACE("Shader: Using the precompiled {} shader '{}/{}' from the pack.", api, iRenderer,
						   iShaderName)
			return loaded;
		}
	}
	// A direct lookup per asset folder: AssetLibrary::find walks every sub-folder on a miss.
	for (const auto& folder: data::assets::getAssetSearchPaths()) {
		if (auto loaded = tryFolder(
					[&](const gpu::ShaderType iStage) -> std::filesystem::path {
						return folder / getPrecompiledShaderPath(iShaderName, iRenderer, api, iStage);
					},
					false);
			loaded.has_value()) {
			OWL_CORE_TRACE("Shader: Using the precompiled {} shader '{}/{}' from '{}'.", api, iRenderer, iShaderName,
						   folder.string())
			return loaded;
		}
	}
	if (!hasShaderCompiler()) {
		OWL_CORE_ERROR("Shader: No baked {} shader '{}/{}' matches its source; export the game again.", api, iRenderer,
					   iShaderName)
		return std::nullopt;
	}
	if (!damaged) {
		OWL_CORE_DEBUG("Slang: Compiling shader '{}/{}' for {}.", iRenderer, iShaderName, api)
		return compileAndCache(iSource, iShaderName, iRenderer, iTarget);
	}
	OWL_CORE_WARN("Shader: No usable stored SPIR-V for '{}/{}' ({}), recompiling from the Slang source.", iRenderer,
				  iShaderName, api)
	auto loaded = compileAndCache(iSource, iShaderName, iRenderer, iTarget);
	if (loaded.has_value())
		OWL_CORE_DEBUG("Shader: '{}/{}' ({}) recompiled, the cache holds the new SPIR-V.", iRenderer, iShaderName, api)
	return loaded;
}

auto recompileSpirv(const std::string& iSource, const std::string& iShaderName, const std::string& iRenderer,
					const ShaderTarget iTarget, const std::filesystem::path& iOrigin, const std::string_view iReason)
		-> std::optional<LoadedSpirv> {
	OWL_PROFILE_FUNCTION()

	const std::string api = getShaderTargetFolder(iTarget);
	if (iOrigin.empty()) {
		OWL_CORE_ERROR("Shader: SPIR-V of '{}/{}' ({}) just compiled from the Slang source rejected, {}.", iRenderer,
					   iShaderName, api, iReason)
		return std::nullopt;
	}
	if (!hasShaderCompiler()) {
		OWL_CORE_ERROR("Shader: Baked {} shader '{}/{}' from '{}' rejected, {}; no compiler here.", api, iRenderer,
					   iShaderName, iOrigin.string(), iReason)
		return std::nullopt;
	}
	OWL_CORE_WARN("Shader: Stored SPIR-V of '{}/{}' ({}) from '{}' rejected, {}; recompiling from the Slang source.",
				  iRenderer, iShaderName, api, iOrigin.string(), iReason)
	auto loaded = compileAndCache(iSource, iShaderName, iRenderer, iTarget);
	if (loaded.has_value())
		OWL_CORE_DEBUG("Shader: '{}/{}' ({}) recompiled, the cache holds the new SPIR-V.", iRenderer, iShaderName, api)
	return loaded;
}

}// namespace owl::renderer::utils
