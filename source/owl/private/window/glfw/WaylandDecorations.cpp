/**
 * @file WaylandDecorations.cpp
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "WaylandDecorations.h"
#include "debug/Profiler.h"

#ifdef OWL_PLATFORM_LINUX
#include <dlfcn.h>
#endif

#include <cstdint>
#include <string_view>

namespace owl::window::glfw {

#ifdef OWL_PLATFORM_LINUX
namespace {

// Minimal libwayland-client ABI: the opaque types and the exported functions behind the inline registry helpers of
// wayland-client-protocol.h, so the engine needs neither the Wayland headers nor a link-time dependency.
struct WlInterface;
using DisplayConnectFn = void* (*) (const char*);
using DisplayDisconnectFn = void (*)(void*);
using DisplayRoundtripFn = int (*)(void*);
using ProxyGetVersionFn = uint32_t (*)(void*);
using ProxyMarshalFlagsFn = void* (*) (void*, uint32_t, const WlInterface*, uint32_t, uint32_t, ...);
using ProxyAddListenerFn = int (*)(void*, void (**)(), void*);
using ProxyDestroyFn = void (*)(void*);

/// Opcode of `wl_display.get_registry`.
constexpr uint32_t g_displayGetRegistry = 1;

/// Layout of `wl_registry_listener`.
struct RegistryListener {
	/// `global` event: one compositor global.
	void (*global)(void*, void*, uint32_t, const char*, uint32_t);
	/// `global_remove` event.
	void (*globalRemove)(void*, void*, uint32_t);
};

void onGlobal(void* ioFound, [[maybe_unused]] void* iRegistry, [[maybe_unused]] uint32_t iName,
			  const char* iInterface, [[maybe_unused]] uint32_t iVersion) {
	if (std::string_view{iInterface} == "zxdg_decoration_manager_v1")
		*static_cast<bool*>(ioFound) = true;
}

void onGlobalRemove([[maybe_unused]] void* iData, [[maybe_unused]] void* iRegistry, [[maybe_unused]] uint32_t iName) {}

template<typename Fn>
auto symbol(void* iLibrary, const char* iName) -> Fn {
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): dlsym returns the function address as void*.
	return reinterpret_cast<Fn>(dlsym(iLibrary, iName));
}

}// namespace

auto compositorDrawsDecorations() -> bool {
	OWL_PROFILE_FUNCTION()

	void* library = dlopen("libwayland-client.so.0", RTLD_LAZY | RTLD_LOCAL);
	if (library == nullptr)
		return false;
	const auto connect = symbol<DisplayConnectFn>(library, "wl_display_connect");
	const auto disconnect = symbol<DisplayDisconnectFn>(library, "wl_display_disconnect");
	const auto roundtrip = symbol<DisplayRoundtripFn>(library, "wl_display_roundtrip");
	const auto getVersion = symbol<ProxyGetVersionFn>(library, "wl_proxy_get_version");
	const auto marshalFlags = symbol<ProxyMarshalFlagsFn>(library, "wl_proxy_marshal_flags");
	const auto addListener = symbol<ProxyAddListenerFn>(library, "wl_proxy_add_listener");
	const auto destroy = symbol<ProxyDestroyFn>(library, "wl_proxy_destroy");
	const auto* registryInterface = static_cast<const WlInterface*>(dlsym(library, "wl_registry_interface"));
	bool found = false;
	if (connect != nullptr && disconnect != nullptr && roundtrip != nullptr && getVersion != nullptr &&
		marshalFlags != nullptr && addListener != nullptr && destroy != nullptr && registryInterface != nullptr) {
		if (void* display = connect(nullptr); display != nullptr) {
			void* registry =
					marshalFlags(display, g_displayGetRegistry, registryInterface, getVersion(display), 0, nullptr);
			static constexpr RegistryListener listener{.global = onGlobal, .globalRemove = onGlobalRemove};
			if (registry != nullptr) {
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-type-const-cast): libwayland's listener ABI is an array of function pointers.
				addListener(registry, reinterpret_cast<void (**)()>(const_cast<RegistryListener*>(&listener)), &found);
				roundtrip(display);
				destroy(registry);
			}
			disconnect(display);
		}
	}
	dlclose(library);
	return found;
}
#else
auto compositorDrawsDecorations() -> bool { return false; }
#endif

}// namespace owl::window::glfw
