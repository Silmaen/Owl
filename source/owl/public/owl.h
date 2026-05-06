/**
 * @file owl.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

// -------- STD ------------
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <queue>
// -------------------------

// ------- core ------------
#include "core/Application.h"
#include "core/Log.h"
#include "core/Timestep.h"
#include "core/layer/Layer.h"
#include "core/utils/FileDialog.h"
#include "core/utils/FileUtils.h"
#include "core/utils/StringUtils.h"
// -------------------------

// ------ Debugging --------
#include "debug/Profiler.h"
#include "debug/Tracker.h"
// -------------------------

#include "event/KeyEvent.h"
#include "event/MouseEvent.h"
#include "input/CameraOrthoController.h"
#include "input/Input.h"
#include "io/video/Manager.h"
#include "math/math.h"
#include "window/Window.h"

// -------- gui ------------
#include "gui/BaseDrawPanel.h"
#include "gui/BasePanel.h"
#include "gui/Guizmo.h"
#include "gui/Theme.h"
#include "gui/UiLayer.h"
#include "io/serial/Manager.h"
#include "math/Transform.h"
#include "math/simpleFunctions.h"

#include "gui/component/render.h"
#include "gui/utils.h"
#include "gui/widgets/ButtonBar.h"
// ------- scene -----------
#include "scene/Entity.h"
#include "scene/GameState.h"
#include "scene/SaveManager.h"
#include "scene/Scene.h"
#include "scene/SceneCamera.h"
#include "scene/SceneSerializer.h"
#include "scene/ScreenTransition.h"
#include "scene/UiInputSystem.h"
#include "scene/component/components.h"
//--------------------------

// ------- script ----------
#include "script/ScriptEngine.h"
#include "script/ScriptInstance.h"
// -------------------------

// ------ renderer ---------
#include "renderer/CameraEditor.h"
#include "renderer/CameraOrtho.h"
#include "renderer/RenderStack.h"
#include "renderer/Renderer.h"
#include "renderer/Renderer2D.h"
#include "renderer/RendererRaycast.h"
#include "renderer/gpu/Buffer.h"
#include "renderer/gpu/Framebuffer.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/Texture.h"
// -------------------------
