/**
 * @file Application.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include <filesystem>

#include "Log.h"
#include "Timestep.h"
#include "event/AppEvent.h"
#include "gui/ImGuiLayer.h"
#include "layer/LayerStack.h"
#include "input/Window.h"

/**
 * @brief Main entry point
 * @param argc Number of arguments
 * @param argv List of argument
 * @return Error code
 */
int main(int argc, char *argv[]);

namespace owl::core {

/**
 * @brief Parameters to give to the application
 */
struct OWL_API AppParams{
	/// Application's title
	std::string name = "Owl Engine";
	/// Application's assets pattern
	std::string assetsPattern = "assets";
	/// Number of command line arguments
	int argCount = 0;
	/// List of command line argument
	char** args = nullptr;
	/**
	 * @brief Access to the given command line argument
	 * @param index Id of the argument
	 * @return The argument
	 */
	const char* operator[](int index)const{
		OWL_CORE_ASSERT(index < argCount, "Bad command line index")
		return args[index];
	}
};
/**
 * @brief Class Application
 */
class OWL_API Application {
public:
	Application() = delete;
	Application(const Application &) = delete;
	Application(Application &&) = delete;
	Application &operator=(const Application &) = delete;
	Application &operator=(Application &&) = delete;
	/**
	 * @brief Default constructor.
	 */
	explicit Application(const AppParams& appParams);
	/**
	 * @brief Access to Application instance
	 * @return Single instance of application
	 */
	static Application &get() { return *instance; }
	/**
	 * @brief Destructor.
	 */
	virtual ~Application();
	/**
	 * @brief Event Callback function
	 * @param e Event received
		 */
	void onEvent(event::Event &e);

	/**
	 * @brief Adding a layer on top of the layers
	 * @param layer The new layer to add
	 */
	void pushLayer(shrd<layer::Layer> &&layer);
	/**
	* @brief Adding an overlay on top of everything
	* @param overlay The new overlay
	*/
	void pushOverlay(shrd<layer::Layer> &&overlay);

	/**
	 * @brief Access to the window
	 * @return The Window
	 */
	[[nodiscard]] const input::Window &getWindow() const {
		return *appWindow.get();
	}
	/**
	 * @brief Access to the Gui layer
	 * @return The gui layer
	 */
	shrd<gui::ImGuiLayer> getImGuiLayer() { return imGuiLayer; }


	/**
	 * @brief Request the application to terminate
	 */
	void close();

	/**
	 * @brief Get the working directory
	 * @return The current working directory
	 */
	[[nodiscard]] const std::filesystem::path &getWorkingDirectory() const {
		return workingDirectory;
	}

	/**
	 * @brief Get the working directory
	 * @return The current working directory
	 */
	[[nodiscard]] const std::filesystem::path &getAssetDirectory() const {
		return assetDirectory;
	}

	/**
	 * @brief Helper function used to redefine assets location
	 * @param pattern The pattern to search for
	 * @return True if found the assets
	 */
	bool searchAssets(const std::string& pattern);

	/**
	 * @brief Enable the docking environment
	 */
	void enableDocking();

	/**
	 * @brief Disable the docking environment
	 */
	void disableDocking();
	/**
	 * @brief Access to init parameters
	 * @return Init parameters
	 */
	[[nodiscard]]const AppParams& getInitParams()const{return initParams;}
private:
	/**
	 * @brief Runs the application
	 */
	void run();
	/**
	* @brief Action on window close.
	* @param e The close event
	* @return True if succeeded
	*/
	bool onWindowClosed(event::WindowCloseEvent &e);
	/**
	* @brief Action on window resize.
	* @param e the resize event
	* @return True if succeeded
	*/
	bool onWindowResized(event::WindowResizeEvent &e);
	/// Pointer to the window
	uniq<input::Window> appWindow;
	/// Pointer to the GUI Layer
	shrd<gui::ImGuiLayer> imGuiLayer;
	/// Running state
	bool running = true;
	/// If Window minimized
	bool minimized = false;
	/// The stack of layers
	layer::LayerStack layerStack;
	/// Base Path to the working Directory
	std::filesystem::path workingDirectory;
	/// Base Path to the asset Directory
	std::filesystem::path assetDirectory;
	/// Time steps management
	Timestep stepper;
	/// Initialization parameters
	AppParams initParams;
	/// The application Instance
	static Application *instance;
	/// Mark the main entrypoint function as friend
	friend int ::main(int argc, char** argv);
};

/**
 * @brief Create an application (Must be defined in the client)
 * @return The application
 */
extern shrd<Application> createApplication(int argc, char** argv);

}// namespace owl::core
