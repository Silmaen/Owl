/**
 * @file Application.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "window/Window.h"
#include "Timestep.h"
#include "core/task/Task.h"
#include "event/AppEvent.h"
#include "data/fonts/FontLibrary.h"
#include "gui/UiLayer.h"
#include "layer/LayerStack.h"
#include "renderer/RenderAPI.h"
#include "sound/SoundAPI.h"
#include "task/Scheduler.h"

#include <filesystem>
#include <list>

auto main(int iArgc, char* iArgv[]) -> int;

namespace owl::core {


/// Default Windows width.
constexpr uint32_t g_DefaultWindowsWidth{1600};
/// Default Windows height.
constexpr uint32_t g_DefaultWindowsHeight{960};
/**
 * @brief Parameters to give to the application.
 */
// NOLINTBEGIN(readability-redundant-member-init)
struct OWL_API AppParams {
	/// List of command line argument.
	char** args{nullptr};
	/// The frequency for the frame debugging
	uint64_t frameLogFrequency{0};
	/// Application's title.
	std::string name{"Owl Engine"};
	/// Application's assets pattern.
	std::string assetsPattern{};
	/// Application's icon.
	std::string icon{};
	/// Windows width.
	uint32_t width{g_DefaultWindowsWidth};
	/// Windows height.
	uint32_t height{g_DefaultWindowsHeight};
	/// Number of command line arguments.
	int argCount{0};
	/// Renderer's type.
	renderer::RenderAPI::Type renderer{renderer::RenderAPI::Type::Vulkan};
	/// sound system's type.
	sound::SoundAPI::Type sound{sound::SoundAPI::Type::OpenAl};
	/// If the application should use ImGui overlay.
	bool hasGui{true};
	/// If extra debugging symbols should be loaded.
	bool useDebugging{false};
	/// Run application in Dummy mode.
	bool isDummy{false};

	/**
	 * @brief Access to the given command line argument.
	 * @param[in] iIndex Index of the argument.
	 * @return The argument.
	 */
	auto operator[](const int iIndex) const -> const char* {
		OWL_CORE_ASSERT(iIndex < argCount, "Bad command line index.")
		OWL_DIAG_PUSH
		OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
		return args[iIndex];
		OWL_DIAG_POP
	}

	/**
	 * @brief Load from a yaml config file.
	 * @param[in] iFile The file to load.
	 */
	void loadFromFile(const std::filesystem::path& iFile);

	/**
	 * @brief Save To a yaml file.
	 * @param[in] iFile The file to save.
	 */
	void saveToFile(const std::filesystem::path& iFile) const;
};
// NOLINTEND(readability-redundant-member-init)

/**
 * @brief Root class defining the application to run.
 */
class OWL_API Application {
public:
	Application() = delete;

	Application(const Application&) = delete;

	Application(Application&&) = delete;

	auto operator=(const Application&) -> Application& = delete;

	auto operator=(Application&&) -> Application& = delete;

	/**
	 * @brief Default constructor.
	 * @param[in] iAppParams Application parameters.
	 */
	explicit Application(AppParams iAppParams);

	/**
	 * @brief Access to Application instance.
	 * @return Single instance of application.
	 */
	static auto get() -> Application& { return *s_instance; }

	/**
	 * @brief Only check for app existence.
	 * @return True if application is instanced.
	 */
	static auto instanced() -> bool { return s_instance != nullptr; }

	/**
	 * @brief Destructor.
	 */
	virtual ~Application();

	/**
	 * @brief Event Callback function.
	 * @param[in,out] ioEvent Event received.
	 */
	void onEvent(event::Event& ioEvent);

	/**
	 * @brief Adding a layer on top of the layers.
	 * @param[in] iLayer The new layer to add.
	 */
	void pushLayer(shared<layer::Layer>&& iLayer);

	/**
	* @brief Adding an overlay on top of everything.
	* @param[in] iOverlay The new overlay.
	*/
	void pushOverlay(shared<layer::Layer>&& iOverlay);

	/**
	 * @brief Access to the window.
	 * @return The Window.
	 */
	[[nodiscard]] auto getWindow() const -> const window::Window& { return *mp_appWindow.get(); }

	/**
	 * @brief Access to the Gui layer.
	 * @return The gui layer.
	 */
	auto getImGuiLayer() -> shared<gui::UiLayer> { return mp_imGuiLayer; }

	/**
	 * @brief Request the application to terminate.
	 */
	void close();

	/**
	 * @brief Request the application to terminate.
	 */
	static void invalidate();

	/**
	 * @brief Get the working directory.
	 * @return The current working directory.
	 */
	[[nodiscard]] auto getWorkingDirectory() const -> const std::filesystem::path& { return m_workingDirectory; }

	/**
	 * @brief Structure holding asset directory information
	 */
	struct AssetDirectory {
		/// Asset directory name.
		std::string title{"assets"};
		/// Asset path.
		std::filesystem::path assetsPath;
	};

	/**
	 * @brief Get the working directory.
	 * @return The current working directory.
	 */
	[[nodiscard]] auto getAssetDirectories() const -> const std::list<AssetDirectory>& { return m_assetDirectories; }

	/**
		 * @brief Enable the docking environment.
		 */
	void enableDocking() const;

	/**
	 * @brief Disable the docking environment.
	 */
	void disableDocking() const;

	/**
	 * @brief Access to init parameters.
	 * @return Init parameters.
	 */
	[[nodiscard]] auto getInitParams() -> AppParams& { return m_initParams; }

	/**
	 * @brief Access to init parameters.
	 * @return Init parameters.
	 */
	[[nodiscard]] auto getInitParams() const -> const AppParams& { return m_initParams; }

	/**
	 * @brief State of the application.
	 */
	enum struct State : uint8_t {
		Created,/// Application just created.
		Running,/// Application is running.
		Stopped,/// Application Stopped.
		Error/// Application in error.
	};

	/**
	 * @brief Get the application's state.
	 * @return The current application's state.
	 */
	[[nodiscard]] auto getState() const -> const State& { return m_state; }


	/**
	 * @brief Get the application's time stepper.
	 * @return The application's time stepper.
	 */
	[[nodiscard]] auto getTimeStep() const -> const Timestep& { return m_stepper; }

	/**
	 * @brief Access to the font library.
	 * @return The Font Library.
	 */
	[[nodiscard]] auto getFontLibrary() -> data::fonts::FontLibrary& { return m_fontLibrary; }

	/**
	 * @brief Access to the font library.
	 * @return The Font Library.
	 */
	[[nodiscard]] auto getFontLibrary() const -> const data::fonts::FontLibrary& { return m_fontLibrary; }

	/**
	 * @brief Access to the task scheduler.
	 * @return The task scheduler.
	 */
	[[nodiscard]] auto getTaskScheduler() -> task::Scheduler& { return m_scheduler; }

	/**
	 * @brief Access to the task scheduler.
	 * @return The task scheduler.
	 */
	[[nodiscard]] auto getTaskScheduler() const -> const task::Scheduler& { return m_scheduler; }

private:
	/**
	 * @brief Helper function used to search for assets location.
	 * @param[in] iPattern The pattern to search for.
	 * @return Optional path for the asse if found.
	 */
	[[nodiscard]] auto searchAssets(const std::string& iPattern) const -> std::optional<std::filesystem::path>;

	/**
	 * @brief Runs the application.
	 */
	void run();

	/**
	* @brief Action on window close.
	* @param[in,out] ioEvent The close event.
	* @return True if succeeded.
	*/
	auto onWindowClosed(event::WindowCloseEvent& ioEvent) -> bool;

	/**
	* @brief Action on window resize.
	* @param[in,out] iEvent the resize event.
	* @return True if succeeded.
	*/
	auto onWindowResized(const event::WindowResizeEvent& iEvent) -> bool;

	/// Pointer to the window.
	uniq<window::Window> mp_appWindow;
	/// Pointer to the GUI Layer.
	shared<gui::UiLayer> mp_imGuiLayer = nullptr;
	/// Running state.
	State m_state = State::Created;
	/// If Window minimized.
	bool m_minimized = false;
	/// The stack of layers.
	layer::LayerStack m_layerStack;
	/// Base Path to the working Directory.
	std::filesystem::path m_workingDirectory;
	/// Base Path to the asset Directory.
	std::list<AssetDirectory> m_assetDirectories;
	/// Time steps management.
	Timestep m_stepper;
	/// Initialization parameters.
	AppParams m_initParams;
	/// The font library.
	data::fonts::FontLibrary m_fontLibrary;
	/// The application Instance.
	static Application* s_instance;
	/// The task Scheduler.
	task::Scheduler m_scheduler;

	/// Mark the main entrypoint function as friend.
	friend auto ::main(int iArgc, char** iArgv) -> int;
};

/**
 * @brief Create an application (Must be defined in the client).
 * @param[in] iArgc Number of arguments.
 * @param[in] iArgv List of argument.
 * @return The application.
 */
extern auto createApplication(int iArgc, char** iArgv) -> shared<Application>;

}// namespace owl::core
