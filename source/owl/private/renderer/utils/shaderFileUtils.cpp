/**
 * @file shaderFileUtils.cpp
 * @author Silmaen
 * @date 06/02/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "app/Application.h"
#include "core/external/slang.h"
#include "data/assets/AssetSearchPaths.h"
#include "shaderFileUtils.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wswitch-enum")
OWL_DIAG_DISABLE_CLANG("-Wdouble-promotion")
OWL_DIAG_DISABLE_CLANG("-Wsign-conversion")
#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>
OWL_DIAG_POP

#include "renderer/Renderer.h"

#include <cctype>
#include <exception>
#include <expected>
#include <format>
#include <fstream>
#include <functional>
#include <mutex>
#include <slang-tag-version.h>

namespace owl::renderer::utils {

auto getCacheDirectory(const std::string& iRenderer, const std::string& iRendererApi) -> std::filesystem::path {
	auto output = app::Application::get().getWorkingDirectory() / "cache" / "shader";
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
			OWL_CORE_ERROR("Cannot Create directory {}.", cacheDirectory.string())
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
	in.read(reinterpret_cast<char*>(result.data()), size);
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
	if (!exists(iFile.parent_path()))
		OWL_CORE_WARN("Cache directory {} does not exists, creating.", iFile.parent_path().string())

	if (out.is_open()) {
		out.write(reinterpret_cast<const char*>(iData.data()), static_cast<int64_t>(iData.size() * sizeof(uint32_t)));
		out.flush();
		out.close();
		const std::lock_guard<std::mutex> lock{g_inMemoryShaderCacheMutex};
		g_inMemoryShaderCache.insert_or_assign(iFile.generic_string(), iData);
		return true;
	}
	OWL_CORE_WARN("Cannot open file {} for writing.", iFile.string())
	return false;
}

auto shaderReflect(const std::string& iShaderName, const std::string& iRenderer, const std::string& iRendererApi,
				   const gpu::ShaderType iStage, const std::vector<uint32_t>& iShaderData) -> ShaderReflectionData {
	ShaderReflectionData result;
	OWL_CORE_TRACE("gpu::Shader reflect - {0} : <assets>/{1}.", magic_enum::enum_name(iStage),
				   renderer::utils::getRelativeShaderPath(iShaderName, iRenderer, iRendererApi, iStage).string())
	if (iShaderData.empty())
		return result;
	const spirv_cross::Compiler compiler(iShaderData);
	const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
	OWL_CORE_TRACE("    {} sampled images.", resources.sampled_images.size())
	for (const auto& resource: resources.uniform_buffers) {
		const auto& bufferType = compiler.get_type(resource.base_type_id);
		const auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
		const auto size = compiler.get_declared_struct_size(bufferType);
		OWL_CORE_TRACE("  Uniform buffer: {} Size={} Binding={} Members={}.", resource.name, size, binding,
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
		OWL_CORE_TRACE("  Sampled image: {} Binding={} Count={}.", resource.name, binding, descriptorCount)
		result.sampledImages.push_back({.name = resource.name, .binding = binding, .descriptorCount = descriptorCount});
	}
	return result;
}

auto computeShaderHash(const std::string& iSource) -> std::string {
	const std::hash<std::string> hasher;
	return std::to_string(hasher(iSource));
}

auto getShaderCacheKey(const std::string& iSource, const std::string& iModuleName, const bool iForVulkan)
		-> std::string {
	return std::format("backend={};module={};slang={};macros=BACKEND_{}=1;profile={};source={}",
					   iForVulkan ? "vulkan" : "opengl", iModuleName, SLANG_TAG_VERSION,
					   iForVulkan ? "VULKAN" : "OPENGL", iForVulkan ? "spirv_1_6" : "glsl_450", iSource);
}

auto remapBuiltinsForOpenGl(std::vector<uint32_t>& ioSpirv) -> uint32_t {
	constexpr size_t headerWords = 5;
	constexpr uint32_t opDecorate = 71;
	constexpr uint32_t decorationBuiltIn = 11;
	uint32_t patched = 0;
	for (size_t i = headerWords; i < ioSpirv.size();) {
		const uint32_t wordCount = ioSpirv[i] >> 16u;
		if (wordCount == 0)
			break;
		if ((ioSpirv[i] & 0xffffu) == opDecorate && wordCount >= 4 && i + 3 < ioSpirv.size() &&
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
		OWL_CORE_ERROR("Shader {}: Empty SPIR-V, no GLSL to generate.", iName)
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
		glsl = compiler.compile();
	} catch (const std::exception& iEx) {
		OWL_CORE_ERROR("Shader {}: SPIR-V to GLSL translation failed ({}).", iName, iEx.what())
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
auto getOrCreateGlobalSession() -> Slang::ComPtr<slang::IGlobalSession> {
	static Slang::ComPtr<slang::IGlobalSession> session;
	if (session == nullptr) {
		if (SLANG_FAILED(slang::createGlobalSession(session.writeRef()))) {
			OWL_CORE_ERROR("Slang: Failed to create global session.")
			return nullptr;
		}
	}
	return session;
}

auto extractSpirv(slang::IBlob* iBlob) -> std::vector<uint32_t> {
	const auto* data = static_cast<const uint32_t*>(iBlob->getBufferPointer());
	const auto size = iBlob->getBufferSize() / sizeof(uint32_t);
	return {data, data + size};
}
}// namespace

auto compileSlangToSpirv(const std::string& iSource, const std::string& iModuleName, const bool iForVulkan)
		-> SlangCompilationResult {
	OWL_PROFILE_FUNCTION()

	SlangCompilationResult result;
	auto globalSession = getOrCreateGlobalSession();
	if (globalSession == nullptr)
		return result;

	slang::TargetDesc targetDesc{};
	targetDesc.format = SLANG_SPIRV;
	targetDesc.profile = globalSession->findProfile(iForVulkan ? "spirv_1_6" : "glsl_450");
	targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
	const char* backendDefine = iForVulkan ? "BACKEND_VULKAN" : "BACKEND_OPENGL";
	slang::PreprocessorMacroDesc macroDesc{};
	macroDesc.name = backendDefine;
	macroDesc.value = "1";
	slang::SessionDesc sessionDesc{};
	sessionDesc.targets = &targetDesc;
	sessionDesc.targetCount = 1;
	sessionDesc.preprocessorMacros = &macroDesc;
	sessionDesc.preprocessorMacroCount = 1;
	Slang::ComPtr<slang::ISession> session;
	if (SLANG_FAILED(globalSession->createSession(sessionDesc, session.writeRef()))) {
		OWL_CORE_ERROR("Slang: Failed to create session for module '{}'.", iModuleName)
		return result;
	}

	Slang::ComPtr<slang::IBlob> diagnosticBlob;
	auto* module = session->loadModuleFromSourceString(iModuleName.c_str(), iModuleName.c_str(), iSource.c_str(),
													   diagnosticBlob.writeRef());
	if (module == nullptr) {
		if (diagnosticBlob != nullptr)

			OWL_CORE_ERROR("Slang: Module load failed for '{}': {}.", iModuleName,
						   static_cast<const char*>(diagnosticBlob->getBufferPointer()))
		return result;
	}
	if (diagnosticBlob != nullptr && diagnosticBlob->getBufferSize() > 0) {
		const std::string_view diag(static_cast<const char*>(diagnosticBlob->getBufferPointer()),
									diagnosticBlob->getBufferSize());
		// Filter out harmless warning 41012 (capabilities auto-upgrade), "warning 41012" or "warning[E41012]"
		if (diag.find("41012") == std::string_view::npos)

			OWL_CORE_WARN("Slang: Warnings for '{}': {}.", iModuleName, diag)
	}

	struct EntryPointInfo {
		const char* name;
		gpu::ShaderType type;
	};
	constexpr EntryPointInfo entryPoints[] = {{"vertexMain", gpu::ShaderType::Vertex},
											  {"fragmentMain", gpu::ShaderType::Fragment},
											  {"computeMain", gpu::ShaderType::Compute}};

	for (const auto& [name, type]: entryPoints) {
		Slang::ComPtr<slang::IEntryPoint> entryPoint;
		if (SLANG_FAILED(module->findEntryPointByName(name, entryPoint.writeRef()))) {
			continue;
		}

		slang::IComponentType* const components[] = {module, entryPoint};
		Slang::ComPtr<slang::IComponentType> composedProgram;
		if (SLANG_FAILED(session->createCompositeComponentType(components, 2, composedProgram.writeRef(),
															   diagnosticBlob.writeRef()))) {
			OWL_CORE_ERROR("Slang: Failed to compose program for entry point '{}' in module '{}'.", name, iModuleName)
			return result;
		}

		Slang::ComPtr<slang::IComponentType> linkedProgram;
		if (SLANG_FAILED(composedProgram->link(linkedProgram.writeRef(), diagnosticBlob.writeRef()))) {
			if (diagnosticBlob != nullptr)

				OWL_CORE_ERROR("Slang: Linking failed for '{}': {}.", name,
							   static_cast<const char*>(diagnosticBlob->getBufferPointer()))
			return result;
		}

		Slang::ComPtr<slang::IBlob> spirvBlob;
		if (SLANG_FAILED(linkedProgram->getEntryPointCode(0, 0, spirvBlob.writeRef(), diagnosticBlob.writeRef()))) {
			if (diagnosticBlob != nullptr)

				OWL_CORE_ERROR("Slang: Code generation failed for '{}': {}.", name,
							   static_cast<const char*>(diagnosticBlob->getBufferPointer()))
			return result;
		}

		result.spirvData[type] = extractSpirv(spirvBlob);
		OWL_CORE_TRACE("Slang: Compiled entry point '{}' ({}) -> {} SPIR-V words.", name, magic_enum::enum_name(type),
					   result.spirvData[type].size())
	}

	if (result.spirvData.empty()) {
		OWL_CORE_ERROR("Slang: Module '{}' has no known entry point.", iModuleName)
		return result;
	}

	result.success = true;
	return result;
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
	std::error_code error;
	if (!exists(iFile, error))
		return std::unexpected{StoredProblem{.kind = Absent, .reason = "absent"}};
	if (!isShaderCacheValid(iFile, iCacheKey))
		return std::unexpected{
				StoredProblem{.kind = Outdated, .reason = "key of another source or Slang version, or no .hash"}};
	const auto bytes = file_size(iFile, error);
	if (error || bytes == 0)
		return std::unexpected{
				StoredProblem{.kind = Damaged,
							  .reason = std::format("unreadable or empty ({})", error ? error.message() : "0 bytes")}};
	if (bytes % sizeof(uint32_t) != 0)
		return std::unexpected{
				StoredProblem{.kind = Damaged,
							  .reason = std::format("{} bytes, not a whole number of SPIR-V words", bytes)}};
	// Read from disk, not the in-memory cache: a file replaced since must be checked again.
	std::vector<uint32_t> words(static_cast<size_t>(bytes) / sizeof(uint32_t));
	std::ifstream in(iFile, std::ios::in | std::ios::binary);
	if (!in.read(reinterpret_cast<char*>(words.data()), static_cast<std::streamsize>(bytes)))
		return std::unexpected{StoredProblem{.kind = Damaged, .reason = "read error"}};
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
					 const bool iForVulkan) -> std::optional<LoadedSpirv> {
	const std::string api = iForVulkan ? "vulkan" : "opengl";
	auto compiled = compileSlangToSpirv(iSource, iShaderName, iForVulkan);
	if (!compiled.success) {
		OWL_CORE_ERROR("Slang compilation failed for shader {}/{} ({}).", iRenderer, iShaderName, api)
		return std::nullopt;
	}
	if (app::Application::instanced()) {
		const auto cacheKey = getShaderCacheKey(iSource, iRenderer + "/" + iShaderName, iForVulkan);
		createCacheDirectoryIfNeeded(iRenderer, api);
		for (const auto& [stage, data]: compiled.spirvData) {
			const auto cachedPath = getShaderCachedPath(iShaderName, iRenderer, api, stage);
			if (!writeCachedShader(cachedPath, data))
				OWL_CORE_WARN("Failed to write the compiled shader {}.", cachedPath.string())
			writeShaderHash(cachedPath, cacheKey);
		}
	}
	return LoadedSpirv{.stages = std::move(compiled.spirvData), .origin = {}};
}
}// namespace

auto loadOrCompileSpirv(const std::string& iSource, const std::string& iShaderName, const std::string& iRenderer,
						const bool iForVulkan, const std::vector<gpu::ShaderType>& iStages)
		-> std::optional<LoadedSpirv> {
	OWL_PROFILE_FUNCTION()

	const std::string api = iForVulkan ? "vulkan" : "opengl";
	const auto cacheKey = getShaderCacheKey(iSource, iRenderer + "/" + iShaderName, iForVulkan);
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
			OWL_CORE_WARN("Shader {}/{} ({}): stored SPIR-V rejected, {}.", iRenderer, iShaderName, api,
						  stages.error().reason)
			damaged = true;
		} else if (stages.error().kind == StoredProblem::Kind::Outdated) {
			OWL_CORE_INFO("Shader {}/{} ({}): stored SPIR-V outdated, {}.", iRenderer, iShaderName, api,
						  stages.error().reason)
		}
		return std::nullopt;
	};
	if (iStages.empty())
		return compileAndCache(iSource, iShaderName, iRenderer, iForVulkan);
	if (app::Application::instanced()) {
		if (auto loaded = tryFolder(
					[&](const gpu::ShaderType iStage) -> std::filesystem::path {
						return getShaderCachedPath(iShaderName, iRenderer, api, iStage);
					},
					true);
			loaded.has_value()) {
			OWL_CORE_INFO("Using cached {} shader {}/{}.", api, iRenderer, iShaderName)
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
			OWL_CORE_INFO("Using precompiled {} shader {}/{} from {}.", api, iRenderer, iShaderName, folder.string())
			return loaded;
		}
	}
	if (!damaged) {
		OWL_CORE_INFO("Compiling Slang shader {}/{} for {}.", iRenderer, iShaderName, api)
		return compileAndCache(iSource, iShaderName, iRenderer, iForVulkan);
	}
	OWL_CORE_WARN("Shader {}/{} ({}): no usable stored SPIR-V, recompiling from the Slang source.", iRenderer,
				  iShaderName, api)
	auto loaded = compileAndCache(iSource, iShaderName, iRenderer, iForVulkan);
	if (loaded.has_value())
		OWL_CORE_WARN("Shader {}/{} ({}): recompiled, the cache now holds the new SPIR-V.", iRenderer, iShaderName, api)
	return loaded;
}

auto recompileSpirv(const std::string& iSource, const std::string& iShaderName, const std::string& iRenderer,
					const bool iForVulkan, const std::filesystem::path& iOrigin, const std::string_view iReason)
		-> std::optional<LoadedSpirv> {
	OWL_PROFILE_FUNCTION()

	const std::string api = iForVulkan ? "vulkan" : "opengl";
	if (iOrigin.empty()) {
		OWL_CORE_ERROR("Shader {}/{} ({}): SPIR-V just compiled from the Slang source rejected, {}.", iRenderer,
					   iShaderName, api, iReason)
		return std::nullopt;
	}
	OWL_CORE_WARN("Shader {}/{} ({}): stored SPIR-V from {} rejected, {}; recompiling from the Slang source.",
				  iRenderer, iShaderName, api, iOrigin.string(), iReason)
	auto loaded = compileAndCache(iSource, iShaderName, iRenderer, iForVulkan);
	if (loaded.has_value())
		OWL_CORE_WARN("Shader {}/{} ({}): recompiled, the cache now holds the new SPIR-V.", iRenderer, iShaderName, api)
	return loaded;
}

}// namespace owl::renderer::utils
