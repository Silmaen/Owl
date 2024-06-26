/**
 * @file UiLayer.cpp
 * @author Silmaen
 * @date 05/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "UiLayer.h"
#include "core/Application.h"
#include "core/external/glfw3.h"
#include "core/external/imgui.h"
#include "renderer/RenderCommand.h"
#include "renderer/vulkan/internal/VulkanHandler.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wzero-as-null-pointer-constant")
#include <ImGuizmo.h>
OWL_DIAG_POP

namespace owl::gui {

namespace {

ImVec4 vec2Im(const math::vec4& iVec) { return {iVec.x(), iVec.y(), iVec.z(), iVec.w()}; }

}// namespace

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
	// Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;// Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;// Enable Multi-Viewport /
	// Platform Windows
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;

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

	if (m_withApp) {
		auto* window = static_cast<GLFWwindow*>(core::Application::get().getWindow().getNativeWindow());

		if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::OpenGL) {
			ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplOpenGL3_Init("#version 410");
		}
		if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::Vulkan) {
			ImGui_ImplGlfw_InitForVulkan(window, true);
			auto& vkh = renderer::vulkan::internal::VulkanHandler::get();
			std::vector<VkFormat> formats;
			ImGui_ImplVulkan_InitInfo info = vkh.toImGuiInfo(formats);
			ImGui_ImplVulkan_Init(&info);
			ImGui_ImplVulkan_CreateFontsTexture();
		}
	}
}

void UiLayer::onDetach() {
	OWL_PROFILE_FUNCTION()
	if (m_withApp) {
		if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::OpenGL)
			ImGui_ImplOpenGL3_Shutdown();
		if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::Vulkan) {
			const auto& vkc = renderer::vulkan::internal::VulkanCore::get();
			vkDeviceWaitIdle(vkc.getLogicalDevice());
			ImGui_ImplVulkan_Shutdown();
		}
		ImGui_ImplGlfw_Shutdown();
	}
	ImGui::DestroyContext();
}

void UiLayer::onEvent(event::Event& ioEvent) {
	if (m_blockEvent) {
		const ImGuiIO& io = ImGui::GetIO();
		ioEvent.handled |= ioEvent.isInCategory(event::Category::Mouse) & io.WantCaptureMouse;
		ioEvent.handled |= ioEvent.isInCategory(event::Category::Keyboard) & io.WantCaptureKeyboard;
	}
}

void UiLayer::begin() {
	if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::OpenGL)
		ImGui_ImplOpenGL3_NewFrame();
	else if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::Vulkan) {
		ImGui_ImplVulkan_NewFrame();
	} else
		return;
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();
	if (m_dockingEnable) {
		initializeDocking();
	}
}

void UiLayer::end() const {
	OWL_PROFILE_FUNCTION()
	if ((renderer::RenderCommand::getApi() != renderer::RenderAPI::Type::OpenGL) &&
		(renderer::RenderCommand::getApi() != renderer::RenderAPI::Type::Vulkan))
		return;
	if (m_dockingEnable) {
		ImGui::End();
	}
	ImGuiIO& io = ImGui::GetIO();
	const core::Application& app = core::Application::get();
	io.DisplaySize =
			ImVec2(static_cast<float>(app.getWindow().getWidth()), static_cast<float>(app.getWindow().getHeight()));
	// Rendering
	ImGui::Render();
	if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::OpenGL)
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::Vulkan) {
		const auto& vkh = renderer::vulkan::internal::VulkanHandler::get();
		renderer::RenderCommand::beginBatch();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkh.getCurrentCommandBuffer());
		renderer::RenderCommand::endBatch();
	}

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		if (renderer::RenderCommand::getApi() != renderer::RenderAPI::Type::OpenGL)
			glfwMakeContextCurrent(backupCurrentContext);
	}
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void UiLayer::setTheme(const Theme& iTheme) {

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	auto& colors = ImGui::GetStyle().Colors;
	// ======================
	// Colors

	// Text
	colors[ImGuiCol_Text] = vec2Im(iTheme.text);
	// 1 ImGuiCol_TextDisabled
	// Window Background 2 3 4 5
	colors[ImGuiCol_WindowBg] = ImVec4{0.1f, 0.105f, 0.11f, 1.0f};
	colors[ImGuiCol_ChildBg] = vec2Im(iTheme.background);
	colors[ImGuiCol_PopupBg] = vec2Im(iTheme.backgroundPopup);
	colors[ImGuiCol_Border] = vec2Im(iTheme.backgroundDark);
	// 6 ImGuiCol_BorderShadow
	// Frame BG 7 8 9
	colors[ImGuiCol_FrameBg] = vec2Im(iTheme.propertyField);
	colors[ImGuiCol_FrameBgHovered] = vec2Im(iTheme.propertyField);
	colors[ImGuiCol_FrameBgActive] = vec2Im(iTheme.propertyField);
	// Title 10 11 12
	colors[ImGuiCol_TitleBg] = vec2Im(iTheme.titleBar);
	colors[ImGuiCol_TitleBgActive] = vec2Im(iTheme.titleBar);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
	// Menubar 13
	colors[ImGuiCol_MenuBarBg] = ImVec4{0.0f, 0.0f, 0.0f, 0.0f};
	// Scrollbar 14 15 16 17
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.0f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.0f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.0f);
	// Checkbox 18
	colors[ImGuiCol_CheckMark] = vec2Im(iTheme.text);
	// Slider 19 20
	colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 0.7f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.66f, 0.66f, 0.66f, 1.0f);
	// Buttons 21 22 23
	colors[ImGuiCol_Button] = ImVec4{0.22f, 0.22f, 0.22f, 0.784f};
	colors[ImGuiCol_ButtonHovered] = ImVec4{0.275f, 0.275f, 0.275f, 1.0f};
	colors[ImGuiCol_ButtonActive] = ImVec4{0.22f, 0.22f, 0.22f, 0.588f};
	// Headers 24 25 26
	colors[ImGuiCol_Header] = vec2Im(iTheme.groupHeader);
	colors[ImGuiCol_HeaderHovered] = vec2Im(iTheme.groupHeader);
	colors[ImGuiCol_HeaderActive] = vec2Im(iTheme.groupHeader);
	// Separator 27 28 29
	colors[ImGuiCol_Separator] = vec2Im(iTheme.backgroundDark);
	colors[ImGuiCol_SeparatorActive] = vec2Im(iTheme.highlight);
	colors[ImGuiCol_SeparatorHovered] = ImColor(39, 185, 242, 150);
	// Resize Grip 30 31 32
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);
	// Tabs 33 34 35 36 37
	colors[ImGuiCol_Tab] = vec2Im(iTheme.titleBar);
	colors[ImGuiCol_TabHovered] = ImVec4{0.38f, 0.3805f, 0.381f, 1.0f};
	colors[ImGuiCol_TabActive] = ImVec4{0.28f, 0.2805f, 0.281f, 1.0f};
	colors[ImGuiCol_TabUnfocused] = vec2Im(iTheme.titleBar);
	colors[ImGuiCol_TabUnfocusedActive] = colors[ImGuiCol_TabHovered];
	// 38 ImGuiCol_DockingPreview
	// 39 ImGuiCol_DockingEmptyBg
	// 40 ImGuiCol_PlotLines
	// 41 ImGuiCol_PlotLinesHovered
	// 42 ImGuiCol_PlotHistogram
	// 43 ImGuiCol_PlotHistogramHovered
	/// Tables 44 46
	colors[ImGuiCol_TableHeaderBg] = vec2Im(iTheme.groupHeader);
	// 45 ImGuiCol_TableBorderStrong
	colors[ImGuiCol_TableBorderLight] = vec2Im(iTheme.backgroundDark);
	// 47 ImGuiCol_TableRowBg
	// 48 ImGuiCol_TableRowBgAlt
	// 49 ImGuiCol_TextSelectedBg
	// 50 ImGuiCol_DragDropTarget
	// 51 ImGuiCol_NavHighlight
	// 52 ImGuiCol_NavWindowingHighlight
	// 53 ImGuiCol_NavWindowingDimBg
	// 54 ImGuiCol_ModalWindowDimBg

	//========================================================
	// Style
	auto& style = ImGui::GetStyle();
	// When viewports are enabled we tweak WindowRounding/WindowBg so platform
	// windows can look identical to regular ones.
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}
	style.FrameRounding = 2.5f;
	style.FrameBorderSize = 1.0f;
	style.IndentSpacing = 11.0f;
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
	if (dockSpaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
		windowFlags |= ImGuiWindowFlags_NoBackground;
	// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
	// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
	// all active windows docked into it will lose their parent and become undocked.
	// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
	// any change of dock space/settings would lead to windows being stuck in limbo and never being visible.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace Demo", &dockSpaceOpen, windowFlags);
	ImGui::PopStyleVar();
	if (optFullScreen)
		ImGui::PopStyleVar(2);
	// DockSpace
	const ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();
	const float minWinSizeX = style.WindowMinSize.x;
	style.WindowMinSize.x = 370.0f;
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
		const ImGuiID dockSpaceId = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockSpaceId, ImVec2(0.0f, 0.0f), dockSpaceFlags);
	}
	style.WindowMinSize.x = minWinSizeX;
}

}// namespace owl::gui
