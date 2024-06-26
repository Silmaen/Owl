/**
 * @file owl.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

// -------- STD ------------
#include <filesystem>
#include <fstream>
// -------------------------

// ------- 3rd party -------
#include <entt/entt.hpp>
#include <imgui.h>
// -------------------------

// ------- core ------------
#include "core/Application.h"
#include "core/Log.h"
#include "core/Timestep.h"
#include "core/layer/Layer.h"
#include "core/utils/FileDialog.h"
#include "core/utils/FileUtils.h"
// -------------------------

// ------ Debugging --------
#include "debug/Profiler.h"
#include "debug/Tracker.h"
// -------------------------

#include "event/KeyEvent.h"
#include "event/MouseEvent.h"
#include "gui/BaseDrawPanel.h"
#include "gui/Theme.h"
#include "gui/UiLayer.h"
#include "input/CameraOrthoController.h"
#include "input/Input.h"
#include "input/Window.h"
#include "input/video/Manager.h"
#include "math/Transform.h"
#include "math/linAlgebra.h"
#include "math/simpleFunctions.h"

// ------- scene -----------
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/SceneCamera.h"
#include "scene/SceneSerializer.h"
#include "scene/component/Camera.h"
#include "scene/component/CircleRenderer.h"
#include "scene/component/ID.h"
#include "scene/component/NativeScript.h"
#include "scene/component/SpriteRenderer.h"
#include "scene/component/Tag.h"
#include "scene/component/Transform.h"
//--------------------------

// ------ renderer ---------
#include "renderer/Buffer.h"
#include "renderer/CameraEditor.h"
#include "renderer/CameraOrtho.h"
#include "renderer/Framebuffer.h"
#include "renderer/RenderCommand.h"
#include "renderer/Renderer.h"
#include "renderer/Renderer2D.h"
#include "renderer/ShaderLibrary.h"
#include "renderer/Texture.h"
// -------------------------
