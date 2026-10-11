/**
 * @file UiLayer.cpp
 * @author Silmaen
 * @date 05/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/UiLayer.h"

#include "app/Application.h"
#include "core/external/glfw3.h"
#include "core/external/imgui.h"
#include "gui/FontPreviewCache.h"
#include "gui/utils.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/vulkan/internal/FrameProfiler.h"
#include "renderer/gpu/vulkan/internal/VulkanHandler.h"

#include <input/Input.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wzero-as-null-pointer-constant")
#include <ImGuizmo.h>
OWL_DIAG_POP

#include <cstdint>

namespace owl::gui {

namespace {
float g_uiFontSize = 20.f;
// Requested code-editor font size, applied on the next `UiLayer::onAttach`.
float g_codeFontSize = 13.f;
// ID of the main dockspace, refreshed each docked frame.
uint32_t g_dockSpaceId = 0;
// Theme applied last; the panels read their tokens from it.
Theme g_theme;
// Content scale of the primary monitor, applied to the style sizes and the fonts.
float g_dpiScale = 1.f;
// Fonts of the typographic roles, loaded in `onAttach` (null before, or headless).
ImFont* g_bodyFont = nullptr;
ImFont* g_strongFont = nullptr;
ImFont* g_monoFont = nullptr;

auto toImGui(const math::vec4& iColor) -> ImVec4 { return {iColor.x(), iColor.y(), iColor.z(), iColor.w()}; }

void applyColors(const Theme& iTheme, ImGuiStyle& ioStyle) {
	using T = Theme;
	auto& colors = ioStyle.Colors;
	const auto set = [&colors](const ImGuiCol iSlot, const math::vec4& iColor) -> void {
		colors[iSlot] = toImGui(iColor);
	};
	const auto clear = T::withAlpha(iTheme.surface0, 0.f);
	set(ImGuiCol_Text, iTheme.text);
	set(ImGuiCol_TextDisabled, iTheme.textDisabled);
	set(ImGuiCol_WindowBg, iTheme.surface1);
	set(ImGuiCol_ChildBg, clear);
	set(ImGuiCol_PopupBg, iTheme.surface2);
	set(ImGuiCol_Border, iTheme.line);
	set(ImGuiCol_BorderShadow, clear);
	set(ImGuiCol_FrameBg, iTheme.well);
	set(ImGuiCol_FrameBgHovered, T::mix(iTheme.well, iTheme.surface3, 0.6f));
	set(ImGuiCol_FrameBgActive, T::mix(iTheme.well, iTheme.surface4, 0.7f));
	set(ImGuiCol_TitleBg, iTheme.surface0);
	set(ImGuiCol_TitleBgActive, iTheme.surface0);
	set(ImGuiCol_TitleBgCollapsed, iTheme.surface0);
	set(ImGuiCol_MenuBarBg, iTheme.surface2);
	set(ImGuiCol_ScrollbarBg, clear);
	set(ImGuiCol_ScrollbarGrab, iTheme.surface3);
	set(ImGuiCol_ScrollbarGrabHovered, iTheme.surface4);
	set(ImGuiCol_ScrollbarGrabActive, iTheme.textDisabled);
	set(ImGuiCol_CheckMark, iTheme.onAccent);
	set(ImGuiCol_CheckboxSelectedBg, iTheme.accent);
	set(ImGuiCol_SliderGrab, T::mix(iTheme.surface4, iTheme.text, 0.3f));
	set(ImGuiCol_SliderGrabActive, iTheme.textMuted);
	set(ImGuiCol_Button, iTheme.surface3);
	set(ImGuiCol_ButtonHovered, iTheme.surface4);
	set(ImGuiCol_ButtonActive, T::mix(iTheme.surface4, iTheme.accent, 0.25f));
	set(ImGuiCol_Header, iTheme.surface3);
	set(ImGuiCol_HeaderHovered, iTheme.surface4);
	set(ImGuiCol_HeaderActive, T::mix(iTheme.surface4, iTheme.text, 0.08f));
	set(ImGuiCol_Separator, iTheme.lineSoft);
	set(ImGuiCol_SeparatorHovered, T::withAlpha(iTheme.accent, 0.6f));
	set(ImGuiCol_SeparatorActive, iTheme.accent);
	set(ImGuiCol_ResizeGrip, T::withAlpha(iTheme.textDisabled, 0.2f));
	set(ImGuiCol_ResizeGripHovered, T::withAlpha(iTheme.accent, 0.6f));
	set(ImGuiCol_ResizeGripActive, iTheme.accent);
	set(ImGuiCol_InputTextCursor, iTheme.text);
	set(ImGuiCol_Tab, iTheme.surface0);
	set(ImGuiCol_TabHovered, iTheme.surface3);
	set(ImGuiCol_TabSelected, iTheme.surface1);
	set(ImGuiCol_TabSelectedOverline, iTheme.accent);
	set(ImGuiCol_TabDimmed, iTheme.surface0);
	set(ImGuiCol_TabDimmedSelected, iTheme.surface1);
	set(ImGuiCol_TabDimmedSelectedOverline, clear);
	set(ImGuiCol_DockingPreview, T::withAlpha(iTheme.accent, 0.45f));
	set(ImGuiCol_DockingEmptyBg, iTheme.surface0);
	set(ImGuiCol_PlotLines, iTheme.textMuted);
	set(ImGuiCol_PlotLinesHovered, iTheme.accent);
	set(ImGuiCol_PlotHistogram, iTheme.accent);
	set(ImGuiCol_PlotHistogramHovered, iTheme.text);
	set(ImGuiCol_TableHeaderBg, iTheme.surface2);
	set(ImGuiCol_TableBorderStrong, iTheme.line);
	set(ImGuiCol_TableBorderLight, iTheme.lineSoft);
	set(ImGuiCol_TableRowBg, clear);
	set(ImGuiCol_TableRowBgAlt, T::withAlpha(iTheme.text, 0.025f));
	set(ImGuiCol_TextLink, iTheme.info);
	set(ImGuiCol_TextSelectedBg, T::withAlpha(iTheme.accent, 0.35f));
	set(ImGuiCol_TreeLines, iTheme.lineSoft);
	set(ImGuiCol_DragDropTarget, iTheme.accent);
	set(ImGuiCol_DragDropTargetBg, T::withAlpha(iTheme.accent, 0.12f));
	set(ImGuiCol_UnsavedMarker, iTheme.textMuted);
	set(ImGuiCol_NavCursor, iTheme.accent);
	set(ImGuiCol_NavWindowingHighlight, T::withAlpha(iTheme.accent, 0.7f));
	set(ImGuiCol_NavWindowingDimBg, T::withAlpha(iTheme.surface0, 0.5f));
	set(ImGuiCol_ModalWindowDimBg, T::withAlpha(iTheme.surface0, 0.6f));
}

void applyMetrics(const Theme& iTheme, ImGuiStyle& ioStyle) {
	// Every spacing sits on the 4 px grid of the style guide.
	ioStyle.WindowPadding = {8.f, 8.f};
	ioStyle.FramePadding = {7.f, 4.f};
	ioStyle.CellPadding = {6.f, 3.f};
	ioStyle.ItemSpacing = {8.f, 5.f};
	ioStyle.ItemInnerSpacing = {5.f, 4.f};
	ioStyle.IndentSpacing = iTheme.indentSpacing;
	ioStyle.ScrollbarSize = 11.f;
	ioStyle.GrabMinSize = 10.f;
	ioStyle.WindowBorderSize = 0.f;
	ioStyle.ChildBorderSize = 1.f;
	ioStyle.PopupBorderSize = 1.f;
	ioStyle.FrameBorderSize = 0.f;
	ioStyle.TabBorderSize = 0.f;
	ioStyle.TabBarBorderSize = 0.f;
	ioStyle.TabBarOverlineSize = 2.f;
	ioStyle.WindowRounding = iTheme.windowRounding;
	ioStyle.ChildRounding = iTheme.windowRounding;
	ioStyle.PopupRounding = iTheme.windowRounding;
	ioStyle.FrameRounding = iTheme.frameRounding;
	ioStyle.GrabRounding = iTheme.frameRounding;
	ioStyle.ScrollbarRounding = 6.f;
	ioStyle.TabRounding = iTheme.tabRounding;
	ioStyle.DockingSeparatorSize = 3.f;
	ioStyle.SeparatorTextBorderSize = 1.f;
	ioStyle.WindowTitleAlign = {0.f, 0.5f};
}

std::vector<shared<renderer::gpu::Texture>> g_deferredTextureReleases;

auto resolveAssetFile(const std::filesystem::path& iRelative) -> std::filesystem::path {
	if (!app::Application::instanced())
		return {};
	const auto& app = app::Application::get();
	for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
		const auto full = assetsPath / iRelative;
		if (exists(full))
			return full;
	}
	return {};
}

// Private-use area: the text fonts' own glyphs there would shadow the Lucide icons merged after them.
constexpr ImWchar g_privateUseExcluded[] = {0xE000, 0xF8FF, 0};

auto loadFont(const char* iRelative, const float iSize) -> ImFont* {
	const auto path = resolveAssetFile(iRelative);
	if (path.empty())
		return nullptr;
	ImFontConfig config;
	config.GlyphExcludeRanges = g_privateUseExcluded;
	return ImGui::GetIO().Fonts->AddFontFromFileTTF(path.string().c_str(), iSize, &config);
}

// Merge the Lucide glyphs (`gui::icon`) into the font loaded last.
void mergeIconFont(const float iSize) {
	const auto path = resolveAssetFile("fonts/lucide/lucide.ttf");
	if (path.empty()) {
		OWL_CORE_WARN("UiLayer: could not load lucide.ttf from engine_assets/fonts/lucide/, icons render as boxes.")
		return;
	}
	ImFontConfig config;
	config.MergeMode = true;
	config.PixelSnapH = true;
	// Lucide draws on a 24-unit grid centred on the em box; this sits it on the text's optical centre.
	config.GlyphOffset = {0.f, iSize * 0.12f};
	ImGui::GetIO().Fonts->AddFontFromFileTTF(path.string().c_str(), iSize, &config);
}
}// namespace

void UiLayer::setUiFontSize(const float iSize) { g_uiFontSize = std::clamp(iSize, 12.f, 32.f); }

void UiLayer::setCodeFontSize(const float iSize) { g_codeFontSize = std::clamp(iSize, 8.f, 48.f); }

auto UiLayer::codeFontSize() -> float { return g_codeFontSize; }

auto UiLayer::getDockSpaceId() -> uint32_t { return g_dockSpaceId; }

auto UiLayer::getTheme() -> const Theme& { return g_theme; }

auto UiLayer::getDpiScale() -> float { return g_dpiScale; }

void UiLayer::pushTextStyle(const TextStyle iStyle) {
	const float base = ImGui::GetStyle().FontSizeBase;
	switch (iStyle) {
		case TextStyle::Body:
			ImGui::PushFont(g_bodyFont, base);
			return;
		case TextStyle::Strong:
			ImGui::PushFont(g_strongFont, base);
			return;
		case TextStyle::Title:
			ImGui::PushFont(g_strongFont, base * 1.15f);
			return;
		case TextStyle::Section:
			ImGui::PushFont(g_strongFont, base * 0.9f);
			return;
		case TextStyle::Label:
			ImGui::PushFont(g_bodyFont, base * 0.875f);
			return;
		case TextStyle::Caption:
			ImGui::PushFont(g_strongFont, base * 0.75f);
			return;
		case TextStyle::Value:
			ImGui::PushFont(g_monoFont != nullptr ? g_monoFont : g_bodyFont, base * 0.92f);
			return;
		case TextStyle::Code:
			ImGui::PushFont(g_monoFont != nullptr ? g_monoFont : g_bodyFont, g_codeFontSize);
			return;
	}
	ImGui::PushFont(g_bodyFont, base);
}

void UiLayer::popTextStyle() { ImGui::PopFont(); }

UiLayer::UiLayer() : Layer("ImGuiLayer") {}

UiLayer::~UiLayer() = default;

void UiLayer::onAttach() {
	// Setup Dear ImGui context
	OWL_PROFILE_FUNCTION()

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	if (m_withApp) {
		const auto& params = app::Application::get().getInitParams();
		if (params.argCount > 0 && params.args != nullptr) {
			const auto stem = std::filesystem::path(params[0]).stem().string();
			m_iniFilePath = (app::Application::get().getWorkingDirectory() / (stem + "_imgui.ini")).string();
			io.IniFilename = m_iniFilePath.c_str();
		}
	}
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;// Enable Keyboard Controls
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;		// Enable Gamepad

	// Docking configuration
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;// Enable Docking
	io.ConfigDockingWithShift = false;
	io.ConfigDockingTransparentPayload = true;
	// ViewPort configuration
	if (m_withApp && app::Application::get().getWindow().getPlatform() == window::Platform::Wayland) {
		OWL_CORE_INFO("UiLayer: Multi-viewports disabled under Wayland (GLFW cannot position windows).")
	} else {
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	}
	io.ConfigViewportsNoDecoration = true;
	io.ConfigViewportsNoAutoMerge = false;

	const float uiSize = g_uiFontSize;
	if (m_withApp && app::Application::get().getWindow().getType() == window::Type::Glfw)
		g_dpiScale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
	g_bodyFont = nullptr;
	g_strongFont = nullptr;
	g_monoFont = nullptr;
	if (m_withApp) {
		g_bodyFont = loadFont("fonts/inter/Inter-Regular.ttf", uiSize);
		if (g_bodyFont == nullptr) {
			OWL_CORE_ERROR("UiLayer: could not load Inter-Regular.ttf from engine_assets/fonts/inter/, using Roboto.")
			g_bodyFont = loadFont("fonts/roboto/Roboto-Regular.ttf", uiSize);
		}
		if (g_bodyFont != nullptr)
			mergeIconFont(uiSize);
		g_strongFont = loadFont("fonts/inter/Inter-SemiBold.ttf", uiSize);
		if (g_strongFont != nullptr)
			mergeIconFont(uiSize);
		g_monoFont = loadFont("fonts/jetbrainsmono/JetBrainsMono-Regular.ttf", g_codeFontSize);
		if (g_monoFont == nullptr)
			OWL_CORE_ERROR("UiLayer: could not load JetBrainsMono-Regular.ttf — code editor will fall back to UI font.")
	}
	if (g_bodyFont == nullptr)
		g_bodyFont = io.Fonts->AddFontDefault();
	if (g_strongFont == nullptr)
		g_strongFont = g_bodyFont;
	mp_codeFont = g_monoFont;
	io.FontDefault = g_bodyFont;
	io.ConfigDpiScaleFonts = true;
	io.ConfigDpiScaleViewports = true;

	setTheme();

	if (m_withApp && app::Application::get().getWindow().getType() == window::Type::Glfw) {
		auto* window = static_cast<GLFWwindow*>(app::Application::get().getWindow().getNativeWindow());
		if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::OpenGL) {
			ImGui_ImplGlfw_InitForOpenGL(window, true);
		} else if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan) {
			ImGui_ImplGlfw_InitForVulkan(window, true);
		}
	}
	if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::OpenGL) {
		ImGui_ImplOpenGL3_Init("#version 410");
	} else if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan) {
		auto& vkh = renderer::gpu::vulkan::internal::VulkanHandler::get();
		std::vector<VkFormat> formats;
		ImGui_ImplVulkan_InitInfo info = vkh.toImGuiInfo(formats);
		ImGui_ImplVulkan_Init(&info);
	}
}

void UiLayer::onDetach() {
	OWL_PROFILE_FUNCTION()

	// Free font-preview framebuffers before tearing down the renderer.
	FontPreviewCache::get().clear();
	if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::OpenGL)
		ImGui_ImplOpenGL3_Shutdown();
	if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan) {
		const auto& vkc = renderer::gpu::vulkan::internal::VulkanCore::get();
		renderer::gpu::vulkan::internal::FrameProfiler::get().deviceWaitIdle(vkc.getLogicalDevice());
		ImGui_ImplVulkan_Shutdown();
	}
	// Flush textures deferred after the last rendered frame; end() clears this only per-frame, so they would else leak.
	g_deferredTextureReleases.clear();
	if (m_withApp && app::Application::get().getWindow().getType() == window::Type::Glfw) {
		ImGui_ImplGlfw_Shutdown();
	}
	ImGui::DestroyContext();
}

void UiLayer::onEvent(event::Event& ioEvent) {
	if (m_blockEvent) {
		const ImGuiIO& io = ImGui::GetIO();
		ioEvent.handled |= ioEvent.isInCategory(event::Category::Mouse) && io.WantCaptureMouse;
		ioEvent.handled |= ioEvent.isInCategory(event::Category::Keyboard) && io.WantCaptureKeyboard;
	}
}

void UiLayer::deferTextureRelease(shared<renderer::gpu::Texture> iTexture) {
	if (iTexture)
		g_deferredTextureReleases.push_back(std::move(iTexture));
}

void UiLayer::begin() const {
	if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::OpenGL)
		ImGui_ImplOpenGL3_NewFrame();
	else if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan) {
		ImGui_ImplVulkan_NewFrame();
	} else {
		const ImGuiIO& io = ImGui::GetIO();
		// fake load fonts
		unsigned char* pixels = nullptr;
		int width = 0;
		int height = 0;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	}
	if (m_withApp && app::Application::get().getWindow().getType() == window::Type::Glfw) {
		ImGui_ImplGlfw_NewFrame();
	} else {
		ImGuiIO& io = ImGui::GetIO();
		// dummy data
		io.DisplaySize = ImVec2(800, 600);
		io.DeltaTime = 1.0f / 60.0f;
	}
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();
	if (m_dockingEnable) {
		initializeDocking();
	}
}

void UiLayer::end() const {
	OWL_PROFILE_FUNCTION()

	if (m_dockingEnable) {
		ImGui::End();
	}
	ImGuiIO& io = ImGui::GetIO();
	if (m_withApp) {
		const app::Application& app = app::Application::get();
		io.DisplaySize = vec(app.getWindow().getSize());
	}
	ImGui::EndFrame();
	// Rendering
	if (renderer::gpu::RenderCommand::getApi() != renderer::gpu::RenderAPI::Type::OpenGL &&
		renderer::gpu::RenderCommand::getApi() != renderer::gpu::RenderAPI::Type::Vulkan)
		return;
	ImGui::Render();
	if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::OpenGL)
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	else if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan) {
		const auto& vkh = renderer::gpu::vulkan::internal::VulkanHandler::get();
		renderer::gpu::RenderCommand::beginBatch();
		renderer::gpu::RenderCommand::nextSubpass();
		if (VkCommandBuffer cmd = vkh.getRenderPassCommandBuffer(); cmd != VK_NULL_HANDLE)
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
		else
			OWL_CORE_FRAME_TRACE("UiLayer: skipped ImGui submission (no swapchain image this frame).")
		renderer::gpu::RenderCommand::endBatch();
	}
	g_deferredTextureReleases.clear();

	if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
		GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		// Platform windows make their own GL context current: give the main one back (Vulkan has none).
		if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::OpenGL)
			glfwMakeContextCurrent(backupCurrentContext);
	}
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void UiLayer::setTheme(const Theme& iTheme) {
	g_theme = iTheme;
	auto& style = ImGui::GetStyle();
	// Start from a fresh style so the DPI scaling below never compounds over repeated calls.
	const float fontSize = g_uiFontSize;
	style = ImGuiStyle();
	applyColors(iTheme, style);
	applyMetrics(iTheme, style);
	style.ScaleAllSizes(g_dpiScale);
	style.FontSizeBase = fontSize;
	style.FontScaleDpi = g_dpiScale;
	style.WindowMenuButtonPosition = ImGuiDir_Right;
	style.ColorButtonPosition = ImGuiDir_Left;
	if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}
}
OWL_DIAG_POP

void UiLayer::initializeDocking() const {
	static bool dockSpaceOpen = true;
	static constexpr bool optFullScreenPersistent = true;
	constexpr bool optFullScreen = optFullScreenPersistent;
	static constexpr ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_None;
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
	if (optFullScreen) {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
					   ImGuiWindowFlags_NoMove;
		windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	}
	if constexpr ((dockSpaceFlags & ImGuiDockNodeFlags_PassthruCentralNode) != 0)
		windowFlags |= ImGuiWindowFlags_NoBackground;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("OwlDockSpace", &dockSpaceOpen, windowFlags);
	ImGui::PopStyleVar();
	if (optFullScreen)
		ImGui::PopStyleVar(2);
	if (m_topBarCallback)
		m_topBarCallback();
	// DockSpace
	const ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();
	const float minWinSizeX = style.WindowMinSize.x;
	style.WindowMinSize.x = 370.0f;
	if ((io.ConfigFlags & ImGuiConfigFlags_DockingEnable) != 0) {
		const ImGuiID dockSpaceId = ImGui::GetID("OwlDockSpace");
		g_dockSpaceId = dockSpaceId;
		if (m_dockLayoutCallback)
			m_dockLayoutCallback(dockSpaceId);
		const float bottomBarHeight = m_bottomBarCallback ? ImGui::GetFrameHeightWithSpacing() : 0.0f;
		ImGui::DockSpace(dockSpaceId, ImVec2(0.0f, -bottomBarHeight), dockSpaceFlags);
	} else {
		OWL_CORE_WARN("Docking is not enabled.")
	}
	style.WindowMinSize.x = minWinSizeX;
	if (m_bottomBarCallback)
		m_bottomBarCallback();
}

}// namespace owl::gui
