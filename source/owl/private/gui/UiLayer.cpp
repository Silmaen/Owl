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
#include "renderer/gpu/vulkan/internal/VulkanHandler.h"

#include <input/Input.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wzero-as-null-pointer-constant")
#include <ImGuizmo.h>
OWL_DIAG_POP

namespace owl::gui {

namespace {
float g_uiFontSize = 20.f;
// Requested code-editor font size, applied on the next `UiLayer::onAttach`.
float g_codeFontSize = 13.f;
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
}// namespace

void UiLayer::setUiFontSize(const float iSize) { g_uiFontSize = std::clamp(iSize, 12.f, 32.f); }

void UiLayer::setCodeFontSize(const float iSize) { g_codeFontSize = std::clamp(iSize, 8.f, 48.f); }

auto UiLayer::codeFontSize() -> float { return g_codeFontSize; }

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
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;// Enable Multi-Viewport
	io.ConfigViewportsNoDecoration = true;
	io.ConfigViewportsNoAutoMerge = false;

	const ImFontConfig fontConfig;
	const float uiSize = g_uiFontSize;
	ImFont* robotoFont = nullptr;
	if (m_withApp) {
		if (const auto p = resolveAssetFile("fonts/roboto/Roboto-Regular.ttf"); !p.empty())
			robotoFont = io.Fonts->AddFontFromFileTTF(p.string().c_str(), uiSize, &fontConfig);
		if (robotoFont == nullptr) {
			OWL_CORE_ERROR("UiLayer: could not load Roboto-Regular.ttf from engine_assets/fonts/roboto/.")
			robotoFont = io.Fonts->AddFontDefault();
		}
		if (const auto p = resolveAssetFile("fonts/roboto/Roboto-Bold.ttf"); !p.empty())
			io.Fonts->AddFontFromFileTTF(p.string().c_str(), uiSize, &fontConfig);
		if (const auto p = resolveAssetFile("fonts/roboto/Roboto-Italic.ttf"); !p.empty())
			io.Fonts->AddFontFromFileTTF(p.string().c_str(), uiSize, &fontConfig);
		if (const auto p = resolveAssetFile("fonts/jetbrainsmono/JetBrainsMono-Regular.ttf"); !p.empty())
			mp_codeFont = io.Fonts->AddFontFromFileTTF(p.string().c_str(), g_codeFontSize, &fontConfig);
		else
			OWL_CORE_ERROR("UiLayer: could not load JetBrainsMono-Regular.ttf — code editor will fall back to UI font.")
	} else {
		robotoFont = io.Fonts->AddFontDefault();
	}
	io.FontDefault = robotoFont;

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
		vkDeviceWaitIdle(vkc.getLogicalDevice());
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
		if (VkCommandBuffer cmd = vkh.getCurrentCommandBuffer(); cmd != VK_NULL_HANDLE)
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
		else
			OWL_CORE_WARN("UiLayer: skipped ImGui submission — Vulkan command buffer not ready.")
		renderer::gpu::RenderCommand::endBatch();
	}
	g_deferredTextureReleases.clear();

	if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
		GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		if (renderer::gpu::RenderCommand::getApi() != renderer::gpu::RenderAPI::Type::OpenGL)
			glfwMakeContextCurrent(backupCurrentContext);
	}
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
// NOLINTBEGIN(readability-convert-member-functions-to-static)
void UiLayer::setTheme(const Theme& iTheme) {
	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	auto& colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = vec(iTheme.text);
	colors[ImGuiCol_WindowBg] = vec(iTheme.windowBackground);
	colors[ImGuiCol_ChildBg] = vec(iTheme.childBackground);
	colors[ImGuiCol_PopupBg] = vec(iTheme.backgroundPopup);
	colors[ImGuiCol_Border] = vec(iTheme.border);
	colors[ImGuiCol_FrameBg] = vec(iTheme.frameBackground);
	colors[ImGuiCol_FrameBgHovered] = vec(iTheme.frameBackgroundHovered);
	colors[ImGuiCol_FrameBgActive] = vec(iTheme.frameBackgroundActive);
	// Title 10 11 12
	colors[ImGuiCol_TitleBg] = vec(iTheme.titleBar);
	colors[ImGuiCol_TitleBgActive] = vec(iTheme.titleBarActive);
	colors[ImGuiCol_TitleBgCollapsed] = vec(iTheme.titleBarCollapsed);
	// Menubar 13
	colors[ImGuiCol_MenuBarBg] = vec(iTheme.menubarBackground);
	// Scrollbar 14 15 16 17
	colors[ImGuiCol_ScrollbarBg] = vec(iTheme.scrollbarBackground);
	colors[ImGuiCol_ScrollbarGrab] = vec(iTheme.scrollbarGrab);
	colors[ImGuiCol_ScrollbarGrabHovered] = vec(iTheme.scrollbarGrabHovered);
	colors[ImGuiCol_ScrollbarGrabActive] = vec(iTheme.scrollbarGrabActive);
	// Checkbox 18
	colors[ImGuiCol_CheckMark] = vec(iTheme.checkMark);
	// Slider 19 20
	colors[ImGuiCol_SliderGrab] = vec(iTheme.sliderGrab);
	colors[ImGuiCol_SliderGrabActive] = vec(iTheme.sliderGrabActive);
	// Buttons 21 22 23
	colors[ImGuiCol_Button] = vec(iTheme.button);
	colors[ImGuiCol_ButtonHovered] = vec(iTheme.buttonHovered);
	colors[ImGuiCol_ButtonActive] = vec(iTheme.buttonActive);
	// Headers 24 25 26
	colors[ImGuiCol_Header] = vec(iTheme.groupHeader);
	colors[ImGuiCol_HeaderHovered] = vec(iTheme.groupHeaderHovered);
	colors[ImGuiCol_HeaderActive] = vec(iTheme.groupHeaderActive);
	// Separator 27 28 29
	colors[ImGuiCol_Separator] = vec(iTheme.separator);
	colors[ImGuiCol_SeparatorActive] = vec(iTheme.separatorActive);
	colors[ImGuiCol_SeparatorHovered] = vec(iTheme.separatorHovered);
	// Resize Grip 30 31 32
	colors[ImGuiCol_ResizeGrip] = vec(iTheme.resizeGrip);
	colors[ImGuiCol_ResizeGripHovered] = vec(iTheme.resizeGripHovered);
	colors[ImGuiCol_ResizeGripActive] = vec(iTheme.resizeGripActive);
	// Tabs 33 34 35 36 37 38 39
	colors[ImGuiCol_TabHovered] = vec(iTheme.tabHovered);
	colors[ImGuiCol_Tab] = vec(iTheme.tab);
	colors[ImGuiCol_TabSelected] = vec(iTheme.tabSelected);
	colors[ImGuiCol_TabSelectedOverline] = vec(iTheme.tabSelectedOverline);
	colors[ImGuiCol_TabDimmed] = vec(iTheme.tabDimmed);
	colors[ImGuiCol_TabDimmedSelected] = vec(iTheme.tabDimmedSelected);
	colors[ImGuiCol_TabDimmedSelectedOverline] = vec(iTheme.tabDimmedSelectedOverline);
	// Docking 40 41
	colors[ImGuiCol_DockingPreview] = vec(iTheme.dockingPreview);
	colors[ImGuiCol_DockingEmptyBg] = vec(iTheme.dockingEmptyBackground);
	colors[ImGuiCol_TableHeaderBg] = vec(iTheme.groupHeader);
	// colours[ImGuiCol_TableBorderStrong] = vec.(iTheme.Text);
	colors[ImGuiCol_TableBorderLight] = vec(iTheme.border);
	auto& style = ImGui::GetStyle();
	// rounding
	style.WindowRounding = iTheme.windowRounding;
	style.FrameRounding = iTheme.frameRounding;
	style.FrameBorderSize = iTheme.frameBorderSize;
	style.TabRounding = iTheme.tabRounding;
	style.TabBarOverlineSize = iTheme.tabOverline;
	style.TabBorderSize = iTheme.tabBorder;
	style.GrabRounding = iTheme.controlsRounding;
	style.ScrollbarRounding = iTheme.controlsRounding;
	style.IndentSpacing = iTheme.indentSpacing;
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
		ImGui::DockSpace(dockSpaceId, ImVec2(0.0f, 0.0f), dockSpaceFlags);
	} else {
		OWL_CORE_WARN("Docking is not enabled.")
	}
	style.WindowMinSize.x = minWinSizeX;
}
// NOLINTEND(readability-convert-member-functions-to-static)

}// namespace owl::gui
