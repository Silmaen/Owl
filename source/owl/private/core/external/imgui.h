/**
 * @file imgui.h
 * @author Silmaen
 * @date 12/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#ifdef OWL_PLATFORM_ANDROID
#include <backends/imgui_impl_android.h>
#else
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl2.h>
#include <backends/imgui_impl_opengl3.h>
#endif
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
