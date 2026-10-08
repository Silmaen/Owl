/**
 * @file Application.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "app/EngineContext.h"
#include "app/HotReload.h"
#include "app/layer/LayerStack.h"
#include "core/Macros.h"
#include "core/Timestep.h"
#include "core/task/Scheduler.h"
#include "core/task/Task.h"
#include "data/assets/pack/PackReader.h"
#include "data/fonts/FontLibrary.h"
#include "event/AppEvent.h"
#include "renderer/gpu/RenderAPI.h"
#include "sound/SoundAPI.h"
#include "window/Window.h"

#include <cstdint>
#include <filesystem>
#include <list>
#include <optional>
#include <string>
#include <vector>

// Forward declaration of the program entry point (definition in EntryPoint.h).
auto main(int iArgc, char* iArgv[]) -> int;

namespace owl::gui {
class UiLayer;
}// namespace owl::gui

namespace owl::app {
/// Default Windows width.
constexpr uint32_t g_DefaultWindowsWidth{1600};
/// Default Windows height.
constexpr uint32_t g_DefaultWindowsHeight{960};
/**
 * @brief
 *  Parameters to give to the application.
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
	renderer::gpu::RenderAPI::Type renderer{renderer::gpu::RenderAPI::Type::Vulkan};
	/// sound system's type.
	sound::SoundAPI::Type sound{sound::SoundAPI::Type::OpenAl};
	/// If the application should use ImGui overlay.
	bool hasGui{true};
	/// If extra debugging symbols should be loaded.
	bool useDebugging{false};
	/// Run application in Dummy mode.
	bool isDummy{false};
	/// Optional path to an asset pack file (opened before renderer init).
	std::string packFile{};
	/// Game name for save directories (set by project system).
	std::string gameName{};
	/// Requested windowing platform (`windowPlatform` in config.yml; the `OWL_WINDOW_PLATFORM` variable overrides it).
	window::Platform windowPlatform{window::Platform::Auto};
	/// Desktop application identifier (Wayland `app_id`, X11 `WM_CLASS`); derived from #name when empty.
	std::string appId{};
	/// Under Wayland, write a hidden user desktop entry named after #appId so the compositor shows the icon.
	bool installDesktopEntry{true};
	/// Read and rewrite `config.yml` from the working directory (off: the parameters above are used as given).
	bool useConfigFile{true};
	/// Synchronise presentation with the display (set before the swap chain is created).
	bool vSync{true};
	/// Reload the assets that change on disk (editor, development runner); never active with an asset pack open.
	bool hotReload{false};

	/**
	 * @brief
	 *  Access to the given command line argument.
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
	 * @brief
	 *  Load from a yaml config file.
	 * @param[in] iFile The file to load.
	 */
	void loadFromFile(const std::filesystem::path& iFile);

	/**
	 * @brief
	 *  Save To a yaml file.
	 * @param[in] iFile The file to save.
	 */
	void saveToFile(const std::filesystem::path& iFile) const;
};
// NOLINTEND(readability-redundant-member-init)

/**
 * @brief
 *  CPU time of the phases of one main-loop iteration, in milliseconds.
 */
struct OWL_API FrameTimings {
	/// `RenderCommand::beginFrame` (Vulkan: fence wait and swap chain image acquisition).
	double beginFrameMs{0.0};
	/// `onUpdate` of every layer.
	double layersMs{0.0};
	/// ImGui overlay (`onImGuiRender` of every layer and the overlay draw).
	double guiMs{0.0};
	/// `RenderCommand::endFrame` (last batch submission).
	double endFrameMs{0.0};
	/// Sound frame.
	double soundMs{0.0};
	/// Window update: event polling and buffer swap or present.
	double presentMs{0.0};
	/// Task scheduler frame (termination callbacks).
	double schedulerMs{0.0};
	/// Whole iteration.
	double totalMs{0.0};
};

/**
 * @brief
 *  Root class defining the application to run.
 */
class OWL_API Application {
public:
	Application() = delete;

	Application(const Application&) = delete;

	Application(Application&&) = delete;

	auto operator=(const Application&) -> Application& = delete;

	auto operator=(Application&&) -> Application& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iAppParams Application parameters.
	 */
	explicit Application(AppParams iAppParams);

	/**
	 * @brief
	 *  Access to Application instance.
	 * @return Single instance of application.
	 */
	static auto get() -> Application& { return *s_instance; }

	/**
	 * @brief
	 *  Only check for app existence.
	 * @return True if application is instanced.
	 */
	static auto instanced() -> bool { return s_instance != nullptr; }

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~Application();

	/**
	 * @brief
	 *  Event Callback function.
	 * @param[in,out] ioEvent Event received.
	 */
	void onEvent(event::Event& ioEvent);

	/**
	 * @brief
	 *  Adding a layer on top of the layers.
	 * @param[in] iLayer The new layer to add.
	 */
	void pushLayer(shared<app::layer::Layer>&& iLayer);

	/**
	 * @brief
	 *  Adding an overlay on top of everything.
	* @param[in] iOverlay The new overlay.
	*/
	void pushOverlay(shared<app::layer::Layer>&& iOverlay);

	/**
	 * @brief
	 *  Access to the window.
	 * @return The Window.
	 */
	[[nodiscard]] auto getWindow() const -> const window::Window& { return *mp_appWindow.get(); }

	/**
	 * @brief
	 *  Mutable access to the window (for runtime property changes).
	 * @return The Window.
	 */
	[[nodiscard]] auto getWindow() -> window::Window& { return *mp_appWindow.get(); }

	/**
	 * @brief
	 *  Access to the Gui layer.
	 * @return The gui layer.
	 */
	[[nodiscard]] auto getImGuiLayer() const -> const shared<gui::UiLayer>& { return mp_imGuiLayer; }

	/**
	 * @brief
	 *  Set the window title.
	 * @param[in] iTitle The new title.
	 */
	void setWindowTitle(const std::string& iTitle);

	/**
	 * @brief
	 *  Request the application to terminate.
	 */
	void close();

	/**
	 * @brief
	 *  Start or stop timing the phases of the main loop (off by default).
	 * @param[in] iEnabled True to time the next iterations.
	 */
	void setFrameTimingsEnabled(const bool iEnabled) { m_frameTimingsEnabled = iEnabled; }

	/**
	 * @brief
	 *  Get the phase timings of the last completed main-loop iteration.
	 * @return The timings, zero while timing is off.
	 */
	[[nodiscard]] auto getLastFrameTimings() const -> const FrameTimings& { return m_lastFrameTimings; }

	/**
	 * @brief
	 *  Request the application to terminate.
	 */
	static void invalidate();

	/**
	 * @brief
	 *  Get the working directory.
	 * @return The current working directory.
	 */
	[[nodiscard]] auto getWorkingDirectory() const -> const std::filesystem::path& { return m_workingDirectory; }

	/**
	 * @brief
	 *  Structure holding asset directory information.
	 */
	struct AssetDirectory {
		/// Asset directory name.
		std::string title{"assets"};
		/// Asset path.
		std::filesystem::path assetsPath;
	};

	/**
	 * @brief
	 *  Get the working directory.
	 * @return The current working directory.
	 */
	[[nodiscard]] auto getAssetDirectories() const -> const std::list<AssetDirectory>& { return m_assetDirectories; }

	/**
	 * @brief
	 *  Add an asset directory (inserted at front, highest priority).
	 * @param[in] iDir The asset directory to add.
	 */
	void addAssetDirectory(const AssetDirectory& iDir);

	/**
	 * @brief
	 *  Remove an asset directory by path.
	 * @param[in] iPath The path of the asset directory to remove.
	 */
	void removeAssetDirectory(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Enable the docking environment.
		 */
	void enableDocking() const;

	/**
	 * @brief
	 *  Disable the docking environment.
	 */
	void disableDocking() const;

	/**
	 * @brief
	 *  Access to init parameters.
	 * @return Init parameters.
	 */
	[[nodiscard]] auto getInitParams() -> AppParams& { return m_initParams; }

	/**
	 * @brief
	 *  Access to init parameters.
	 * @return Init parameters.
	 */
	[[nodiscard]] auto getInitParams() const -> const AppParams& { return m_initParams; }

	/**
	 * @brief
	 *  State of the application.
	 */
	enum struct State : uint8_t {
		Created,/// Application just created.
		Running,/// Application is running.
		Stopped,/// Application Stopped.
		Error/// Application in error.
	};

	/**
	 * @brief
	 *  Get the application's state.
	 * @return The current application's state.
	 */
	[[nodiscard]] auto getState() const -> const State& { return m_state; }

	/**
	 * @brief
	 *  Set the process exit code returned by `main` once the application stops.
	 * @param[in] iCode The exit code (0 means success).
	 */
	void setExitCode(const int iCode) { m_exitCode = iCode; }

	/**
	 * @brief
	 *  Get the process exit code.
	 * @return The exit code, 1 when the application ended in the Error state and no code was set.
	 */
	[[nodiscard]] auto getExitCode() const -> int {
		return m_exitCode == 0 && m_state == State::Error ? 1 : m_exitCode;
	}

	/**
	 * @brief
	 *  Get the application's time stepper.
	 * @return The application's time stepper.
	 */
	[[nodiscard]] auto getTimeStep() const -> const core::Timestep& { return m_stepper; }

	/**
	 * @brief
	 *  Access to the font library.
	 * @return The Font Library.
	 */
	[[nodiscard]] auto getFontLibrary() -> data::fonts::FontLibrary& { return m_fontLibrary; }

	/**
	 * @brief
	 *  Access to the font library.
	 * @return The Font Library.
	 */
	[[nodiscard]] auto getFontLibrary() const -> const data::fonts::FontLibrary& { return m_fontLibrary; }

	/**
	 * @brief
	 *  Access to the task scheduler.
	 * @return The task scheduler.
	 */
	[[nodiscard]] auto getTaskScheduler() -> core::task::Scheduler& { return m_scheduler; }

	/**
	 * @brief
	 *  Engine state shared by the scenes of this application (screen transition, settings, voxel meshes).
	 * @return The engine context.
	 */
	[[nodiscard]] auto getEngineContext() -> EngineContext& { return *mp_engineContext; }

	/**
	 * @brief
	 *  Access to the task scheduler.
	 * @return The task scheduler.
	 */
	[[nodiscard]] auto getTaskScheduler() const -> const core::task::Scheduler& { return m_scheduler; }

	/**
	 * @brief
	 *  Access to the hot reload of the assets.
	 * @return The hot reload.
	 */
	[[nodiscard]] auto getHotReload() -> HotReload& { return m_hotReload; }

	/**
	 * @brief
	 *  Turn the hot reload on or off; it stays off while an asset pack is open.
	 * @param[in] iEnabled True to watch the asset directories.
	 */
	void setHotReloadEnabled(bool iEnabled);

	/**
	 * @brief
	 *  Open an asset pack file for runtime loading.
	 * @param[in] iPackFile Path to the pack file.
	 * @return True if the pack was opened successfully.
	 */
	auto openPack(const std::filesystem::path& iPackFile) -> bool;

	/**
	 * @brief
	 *  Close the current asset pack.
	 */
	void closePack();

	/**
	 * @brief
	 *  Check if an asset pack is currently open.
	 * @return True if a pack is open.
	 */
	[[nodiscard]] auto hasOpenPack() const -> bool;

	/**
	 * @brief
	 *  Read an asset from the open pack.
	 * @param[in] iPath The asset path inside the pack.
	 * @return The raw asset data, or nullopt if not found.
	 */
	[[nodiscard]] auto loadFromPack(const std::string& iPath) const -> std::optional<std::vector<uint8_t>>;

	/**
	 * @brief
	 *  Check if the open pack contains an asset.
	 * @param[in] iPath The asset path inside the pack.
	 * @return True if the asset exists in the pack.
	 */
	[[nodiscard]] auto packContains(const std::string& iPath) const -> bool;

	/**
	 * @brief
	 *  Access the pack reader.
	 * @return The pack reader (may not be open).
	 */
	[[nodiscard]] auto getPackReader() const -> const data::assets::pack::PackReader& { return m_packReader; }

private:
	/**
	 * @brief
	 *  Helper function used to search for assets location.
	 * @param[in] iPattern The pattern to search for.
	 * @return Optional path for the asse if found.
	 */
	[[nodiscard]] auto searchAssets(const std::string& iPattern) const -> std::optional<std::filesystem::path>;

	/**
	 * @brief
	 *  Runs the application.
	 */
	void run();

	/**
	 * @brief
	 *  Action on window close.
	 * @param[in] iEvent The close event.
	 * @return True if succeeded.
	 */
	auto onWindowClosed(const event::WindowCloseEvent& iEvent) -> bool;

	/**
	 * @brief
	 *  Action on window resize.
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
	/// Process exit code returned by `main`.
	int m_exitCode = 0;
	/// If Window minimized.
	bool m_minimized = false;
	/// True while the main-loop phases are timed.
	bool m_frameTimingsEnabled = false;
	/// Phase timings of the last completed main-loop iteration.
	FrameTimings m_lastFrameTimings;
	/// The stack of layers.
	app::layer::LayerStack m_layerStack;
	/// Base Path to the working Directory.
	std::filesystem::path m_workingDirectory;
	/// Base Path to the asset Directory.
	std::list<AssetDirectory> m_assetDirectories;
	/// Time steps management.
	core::Timestep m_stepper;
	/// Initialization parameters.
	AppParams m_initParams;
	/// The font library.
	data::fonts::FontLibrary m_fontLibrary;
	/// The application Instance.
	static Application* s_instance;
	/// The task Scheduler.
	core::task::Scheduler m_scheduler;
	/// Watches the asset directories and reloads what changed.
	HotReload m_hotReload;
	/// Engine state shared by the scenes, released before the renderer shuts down.
	uniq<EngineContext> mp_engineContext = mkUniq<EngineContext>();
	/// The asset pack reader.
	data::assets::pack::PackReader m_packReader;
	/// Mark the main entrypoint function as friend.
	friend auto ::main(int iArgc, char** iArgv) -> int;
};

/**
 * @brief
 *  Create an application (Must be defined in the client).
 * @param[in] iArgc Number of arguments.
 * @param[in] iArgv List of argument.
 * @return The application.
 */
extern auto createApplication(int iArgc, char** iArgv) -> shared<Application>;

}// namespace owl::app
