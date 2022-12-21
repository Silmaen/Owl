/**
 * @file ImGuiLayer.cpp
 * @author Silmaen
 * @date 05/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "ImGuiLayer.h"
#include "core/Application.h"
#include "core/external/glfw3.h"
#include "core/external/imgui.h"
#include "input/Input.h"

namespace owl::gui {

ImGuiLayer::ImGuiLayer() : core::layer::Layer("ImGuiLayer") {}
ImGuiLayer::~ImGuiLayer() = default;

void ImGuiLayer::onAttach() {// Setup Dear ImGui context
	OWL_PROFILE_FUNCTION()

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |=
			ImGuiConfigFlags_NavEnableKeyboard;// Enable Keyboard Controls
											   // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;		// Enable Gamepad
											   // Controls
#ifdef IMGUI_IMPL_HAS_DOCKING
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;// Enable Multi-Viewport /
													   // Platform Windows
#endif
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
#ifdef IMGUI_IMPL_HAS_DOCKING
	// When viewports are enabled we tweak WindowRounding/WindowBg so platform
	// windows can look identical to regular ones.
	ImGuiStyle &style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}
#endif
	SetDarkThemeColors();

	auto *window = static_cast<GLFWwindow *>(
			core::Application::get().getWindow().getNativeWindow());
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410");
}

void ImGuiLayer::onDetach() {
	OWL_PROFILE_FUNCTION()

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void ImGuiLayer::onEvent([[maybe_unused]] event::Event &event) {
	ImGuiIO& io = ImGui::GetIO();
	event.handled |= event.isInCategory(event::category::Mouse) & io.WantCaptureMouse;
	event.handled |= event.isInCategory(event::category::Keyboard) & io.WantCaptureKeyboard;
}

void ImGuiLayer::Begin() {
	OWL_PROFILE_FUNCTION()

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
#ifdef IMGUI_IMPL_HAS_DOCKING
	if (dockingEnable){
		//ImGui::Begin("Docking");
		ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(),dockspace_flags);
	}
#endif
}

void ImGuiLayer::End() {
	OWL_PROFILE_FUNCTION()
#ifdef IMGUI_IMPL_HAS_DOCKING
	if (dockingEnable){
		//ImGui::End();
	}
#endif
	ImGuiIO &io = ImGui::GetIO();
	core::Application &app = core::Application::get();
	io.DisplaySize = ImVec2(static_cast<float>(app.getWindow().getWidth()),
							static_cast<float>(app.getWindow().getHeight()));
	// Rendering
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

#ifdef IMGUI_IMPL_HAS_DOCKING
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		GLFWwindow *backup_current_context = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backup_current_context);
	}
#endif
}

void ImGuiLayer::SetDarkThemeColors() {
	auto &colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_WindowBg] = ImVec4{0.1f, 0.105f, 0.11f, 1.0f};

	// Headers
	colors[ImGuiCol_Header] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
	colors[ImGuiCol_HeaderHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
	colors[ImGuiCol_HeaderActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

	// Buttons
	colors[ImGuiCol_Button] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
	colors[ImGuiCol_ButtonHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
	colors[ImGuiCol_ButtonActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

	// Frame BG
	colors[ImGuiCol_FrameBg] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
	colors[ImGuiCol_FrameBgHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
	colors[ImGuiCol_FrameBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

	// Tabs
	colors[ImGuiCol_Tab] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
	colors[ImGuiCol_TabHovered] = ImVec4{0.38f, 0.3805f, 0.381f, 1.0f};
	colors[ImGuiCol_TabActive] = ImVec4{0.28f, 0.2805f, 0.281f, 1.0f};
	colors[ImGuiCol_TabUnfocused] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};

	// Title
	colors[ImGuiCol_TitleBg] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
	colors[ImGuiCol_TitleBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
}

}// namespace owl::gui
