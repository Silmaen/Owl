/**
 * @file UiLayer.cpp
 * @author Silmaen
 * @date 05/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/UiLayer.h"

#include "core/Application.h"
#include "core/external/glfw3.h"
#include "core/external/imgui.h"
#include "gui/utils.h"
#include "renderer/RenderCommand.h"
#include "renderer/vulkan/internal/VulkanHandler.h"

#include <input/Input.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wzero-as-null-pointer-constant")
#include <ImGuizmo.h>
OWL_DIAG_POP

namespace owl::gui {

/// memory fonts...
#include "Roboto-Bold.embed"
#include "Roboto-Italic.embed"
#include "Roboto-Regular.embed"

UiLayer::UiLayer() : Layer("ImGuiLayer") {}
UiLayer::~UiLayer() = default;

void UiLayer::onAttach() {
	// Setup Dear ImGui context
	OWL_PROFILE_FUNCTION()

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;// Enable Keyboard Controls
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;		// Enable Gamepad

	// Docking configuration
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;// Enable Docking
	io.ConfigDockingWithShift = true;
	io.ConfigDockingTransparentPayload = true;
	// ViewPort configuration
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;// Enable Multi-Viewport
	io.ConfigViewportsNoDecoration = true;
	io.ConfigViewportsNoAutoMerge = false;

	// Better fonts
	ImFontConfig fontConfig;
	fontConfig.FontDataOwnedByAtlas = false;
	// NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast)
	ImFont* robotoFont = io.Fonts->AddFontFromMemoryTTF(const_cast<void*>(static_cast<const void*>(g_RobotoRegular)),
														sizeof(g_RobotoRegular), 20.0f, &fontConfig);
	io.Fonts->AddFontFromMemoryTTF(const_cast<void*>(static_cast<const void*>(g_RobotoBold)), sizeof(g_RobotoBold),
								   20.0f, &fontConfig);
	io.Fonts->AddFontFromMemoryTTF(const_cast<void*>(static_cast<const void*>(g_RobotoItalic)), sizeof(g_RobotoItalic),
								   20.0f, &fontConfig);
	// NOLINTEND(cppcoreguidelines-pro-type-const-cast)
	io.FontDefault = robotoFont;

	setTheme();

	if (m_withApp && core::Application::get().getWindow().getType() == input::Type::Glfw) {
		auto* window = static_cast<GLFWwindow*>(core::Application::get().getWindow().getNativeWindow());
		if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::OpenGL) {
			ImGui_ImplGlfw_InitForOpenGL(window, true);
		} else if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::Vulkan) {
			ImGui_ImplGlfw_InitForVulkan(window, true);
		}
	}
	if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::OpenGL) {
		ImGui_ImplOpenGL3_Init("#version 410");
	} else if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::Vulkan) {
		auto& vkh = renderer::vulkan::internal::VulkanHandler::get();
		std::vector<VkFormat> formats;
		ImGui_ImplVulkan_InitInfo info = vkh.toImGuiInfo(formats);
		ImGui_ImplVulkan_Init(&info);
		ImGui_ImplVulkan_CreateFontsTexture();
	}
}

void UiLayer::onDetach() {
	OWL_PROFILE_FUNCTION()
	if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::OpenGL)
		ImGui_ImplOpenGL3_Shutdown();
	if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::Vulkan) {
		const auto& vkc = renderer::vulkan::internal::VulkanCore::get();
		vkDeviceWaitIdle(vkc.getLogicalDevice());
		ImGui_ImplVulkan_Shutdown();
	}
	if (m_withApp && core::Application::get().getWindow().getType() == input::Type::Glfw) {
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

void UiLayer::begin() const {
	if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::OpenGL)
		ImGui_ImplOpenGL3_NewFrame();
	else if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::Vulkan) {
		ImGui_ImplVulkan_NewFrame();
	} else {
		const ImGuiIO& io = ImGui::GetIO();
		// fake load fonts
		unsigned char* pixels = nullptr;
		int width = 0;
		int height = 0;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	}
	if (m_withApp && core::Application::get().getWindow().getType() == input::Type::Glfw) {
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
		const core::Application& app = core::Application::get();
		io.DisplaySize = vec(app.getWindow().getSize());
	}
	ImGui::EndFrame();
	// Rendering
	if (renderer::RenderCommand::getApi() != renderer::RenderAPI::Type::OpenGL &&
		renderer::RenderCommand::getApi() != renderer::RenderAPI::Type::Vulkan)
		return;
	ImGui::Render();
	if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::OpenGL)
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	else if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::Vulkan) {
		const auto& vkh = renderer::vulkan::internal::VulkanHandler::get();
		renderer::RenderCommand::beginBatch();
		renderer::RenderCommand::nextSubpass();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkh.getCurrentCommandBuffer());
		renderer::RenderCommand::endBatch();
	}

	if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
		GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		if (renderer::RenderCommand::getApi() != renderer::RenderAPI::Type::OpenGL)
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
	// ======================
	// Colors

	// Text 1 2
	colors[ImGuiCol_Text] = vec(iTheme.text);
	// colors[ImGuiCol_TextDisabled] = vec(iTheme.textDisabled);
	// Window Background 2 3 4 5
	colors[ImGuiCol_WindowBg] = vec(iTheme.windowBackground);
	colors[ImGuiCol_ChildBg] = vec(iTheme.childBackground);
	colors[ImGuiCol_PopupBg] = vec(iTheme.backgroundPopup);
	colors[ImGuiCol_Border] = vec(iTheme.border);
	// colors[ImGuiCol_BorderShadow] = vec(iTheme.border);
	// Frame BG 7 8 9
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
	// PlotLines 42 43 44 44
	// colors[ImGuiCol_PlotLines] = vec(iTheme.Text);
	// colors[ImGuiCol_PlotLinesHovered] = vec(iTheme.Text);
	// colors[ImGuiCol_PlotHistogram] = vec(iTheme.Text);
	// colors[ImGuiCol_PlotHistogramHovered] = vec(iTheme.Text);
	/// Tables 46 47 48 49 50
	colors[ImGuiCol_TableHeaderBg] = vec(iTheme.groupHeader);
	// colors[ImGuiCol_TableBorderStrong] = vec.(iTheme.Text);
	colors[ImGuiCol_TableBorderLight] = vec(iTheme.border);
	// colors[ImGuiCol_TableRowBg] = vec.(iTheme.Text);
	// colors[ImGuiCol_TableRowBgAlt] = vec.(iTheme.Text);
	// Text Selected 51
	// colors[ImGuiCol_TextSelectedBg] = vec.(iTheme.Text);
	// Drag n DRop 52
	// colors[ImGuiCol_DragDropTarget] = vec.(iTheme.Text);
	// Nav 53 54 55
	// colors[ImGuiCol_NavHighlight] = vec.(iTheme.Text);
	// colors[ImGuiCol_NavWindowingHighlight] = vec.(iTheme.Text);
	// colors[ImGuiCol_NavWindowingDimBg] = vec.(iTheme.Text);
	// Modal window 56
	// colors[ImGuiCol_ModalWindowDimBg] = vec.(iTheme.Text);

	//========================================================
	// Style
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
	// When viewports are enabled we tweak WindowRounding/WindowBg so platform
	// windows can look identical to regular ones.
	if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}
}
OWL_DIAG_POP

void UiLayer::initializeDocking() {
	static bool dockSpaceOpen = true;
	static constexpr bool optFullScreenPersistant = true;
	constexpr bool optFullScreen = optFullScreenPersistant;
	static constexpr ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_None;
	// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
	// because it would be confusing to have two docking targets within each others.
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
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
	// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle
	// the pass-thru hole, so we ask Begin() to not render a background.
	if constexpr ((dockSpaceFlags & ImGuiDockNodeFlags_PassthruCentralNode) != 0)
		windowFlags |= ImGuiWindowFlags_NoBackground;
	// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
	// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
	// all active windows docked into it will lose their parent and become undocked.
	// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
	// any change of dock space/settings would lead to windows being stuck in limbo and never being visible.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("OwlDockSpace", &dockSpaceOpen, windowFlags);
	ImGui::PopStyleVar();
	if (optFullScreen)
		ImGui::PopStyleVar(2);
	// DockSpace
	const ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();
	const float minWinSizeX = style.WindowMinSize.x;
	style.WindowMinSize.x = 370.0f;
	if ((io.ConfigFlags & ImGuiConfigFlags_DockingEnable) != 0) {
		const ImGuiID dockSpaceId = ImGui::GetID("OwlDockSpace");
		ImGui::DockSpace(dockSpaceId, ImVec2(0.0f, 0.0f), dockSpaceFlags);
	}
	style.WindowMinSize.x = minWinSizeX;
}
// NOLINTEND(readability-convert-member-functions-to-static)

}// namespace owl::gui
