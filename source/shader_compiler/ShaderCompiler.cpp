/**
 * @file ShaderCompiler.cpp
 * @author Silmaen
 * @date 17/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "slangInclude.h"

#include <core/Log.h>
#include <debug/Profiler.h>
#include <renderer/utils/shaderFileUtils.h>

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The Slang compiler of Owl Nest, the build tools and the tests, installed in the engine when the program starts:
// the runner of an exported game does not link this library, nor Slang.

namespace owl::renderer::utils {
namespace {

auto getOrCreateGlobalSession() -> Slang::ComPtr<slang::IGlobalSession> {
	static Slang::ComPtr<slang::IGlobalSession> session;
	if (session == nullptr) {
		if (SLANG_FAILED(slang::createGlobalSession(session.writeRef()))) {
			OWL_CORE_ERROR("Slang: Cannot create the global session.")
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

auto extractText(slang::IBlob* iBlob) -> std::string {
	return {static_cast<const char*>(iBlob->getBufferPointer()), iBlob->getBufferSize()};
}

// Slang copies the `@interpolate` of a stage input onto the plain structures it makes from it, which have no
// `@location`: WGSL refuses the attribute there, so it leaves the fields without one.
auto stripStrayInterpolation(const std::string& iWgsl) -> std::string {
	std::string result;
	result.reserve(iWgsl.size());
	size_t start = 0;
	while (start < iWgsl.size()) {
		const auto end = std::min(iWgsl.find('\n', start), iWgsl.size());
		std::string line = iWgsl.substr(start, end - start);
		if (const auto attribute = line.find("@interpolate(");
			attribute != std::string::npos && line.find("@location(") == std::string::npos) {
			if (const auto close = line.find(')', attribute); close != std::string::npos)
				line.erase(attribute, close - attribute + 1);
		}
		result += line;
		if (end < iWgsl.size())
			result += '\n';
		start = end + 1;
	}
	return result;
}

auto compileWithSlang(const std::string& iSource, const std::string& iModuleName, const ShaderTarget iTarget)
		-> SlangCompilationResult {
	OWL_PROFILE_FUNCTION()

	SlangCompilationResult result;
	auto globalSession = getOrCreateGlobalSession();
	if (globalSession == nullptr)
		return result;

	const bool wgsl = iTarget == ShaderTarget::WebGpu;
	const std::string profile{getShaderTargetProfile(iTarget)};
	const std::string macro{getShaderTargetMacro(iTarget)};
	slang::TargetDesc targetDesc{};
	targetDesc.format = wgsl ? SLANG_WGSL : SLANG_SPIRV;
	if (!wgsl) {
		targetDesc.profile = globalSession->findProfile(profile.c_str());
		targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
	}
	slang::PreprocessorMacroDesc macroDesc{};
	macroDesc.name = macro.c_str();
	macroDesc.value = "1";
	slang::SessionDesc sessionDesc{};
	sessionDesc.targets = &targetDesc;
	sessionDesc.targetCount = 1;
	sessionDesc.preprocessorMacros = &macroDesc;
	sessionDesc.preprocessorMacroCount = 1;
	Slang::ComPtr<slang::ISession> session;
	if (SLANG_FAILED(globalSession->createSession(sessionDesc, session.writeRef()))) {
		OWL_CORE_ERROR("Slang: Cannot create a session for module '{}'.", iModuleName)
		return result;
	}

	Slang::ComPtr<slang::IBlob> diagnosticBlob;
	auto* module = session->loadModuleFromSourceString(iModuleName.c_str(), iModuleName.c_str(), iSource.c_str(),
													   diagnosticBlob.writeRef());
	if (module == nullptr) {
		if (diagnosticBlob != nullptr)
			OWL_CORE_ERROR("Slang: Loading module '{}' failed: {}.", iModuleName,
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
		if (SLANG_FAILED(module->findEntryPointByName(name, entryPoint.writeRef())))
			continue;

		slang::IComponentType* const components[] = {module, entryPoint};
		Slang::ComPtr<slang::IComponentType> composedProgram;
		if (SLANG_FAILED(session->createCompositeComponentType(components, 2, composedProgram.writeRef(),
															   diagnosticBlob.writeRef()))) {
			OWL_CORE_ERROR("Slang: Cannot compose the program of entry point '{}' in module '{}'.", name, iModuleName)
			return result;
		}

		Slang::ComPtr<slang::IComponentType> linkedProgram;
		if (SLANG_FAILED(composedProgram->link(linkedProgram.writeRef(), diagnosticBlob.writeRef()))) {
			if (diagnosticBlob != nullptr)
				OWL_CORE_ERROR("Slang: Linking of '{}' failed: {}.", name,
							   static_cast<const char*>(diagnosticBlob->getBufferPointer()))
			return result;
		}

		Slang::ComPtr<slang::IBlob> codeBlob;
		if (SLANG_FAILED(linkedProgram->getEntryPointCode(0, 0, codeBlob.writeRef(), diagnosticBlob.writeRef()))) {
			if (diagnosticBlob != nullptr)
				OWL_CORE_ERROR("Slang: Code generation of '{}' in module '{}' for {} failed: {}.", name, iModuleName,
							   getShaderTargetFolder(iTarget),
							   static_cast<const char*>(diagnosticBlob->getBufferPointer()))
			return result;
		}

		if (wgsl) {
			result.wgslData[type] = stripStrayInterpolation(extractText(codeBlob));
			OWL_CORE_TRACE("Slang: Compiled entry point '{}' ({}) -> {} WGSL characters.", name,
						   magic_enum::enum_name(type), result.wgslData[type].size())
		} else {
			result.spirvData[type] = extractSpirv(codeBlob);
			OWL_CORE_TRACE("Slang: Compiled entry point '{}' ({}) -> {} SPIR-V words.", name,
						   magic_enum::enum_name(type), result.spirvData[type].size())
		}
	}

	if (result.spirvData.empty() && result.wgslData.empty()) {
		OWL_CORE_ERROR("Slang: Module '{}' has no known entry point.", iModuleName)
		return result;
	}

	result.success = true;
	return result;
}

// Installs the compiler before main: every program linking this object library compiles shaders. Nothing can catch an
// exception thrown before main: a failure leaves the program without compiler, as the game runner.
// NOLINTNEXTLINE(cert-err58-cpp,bugprone-throwing-static-initialization) the noexcept installer catches everything
[[maybe_unused]] const bool g_installed = []() noexcept -> bool {
	try {
		setShaderCompiler(compileWithSlang);
		return true;
	} catch (...) { return false; }
}();

}// namespace
}// namespace owl::renderer::utils
