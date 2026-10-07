/**
 * @file InspectorEditTracker_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/InspectorEditTracker.h"
#include "nestTestHelper.h"
#include "panel/SceneHierarchy.h"
#include "testHelper.h"

#include <cstdio>
#include <gui/UiLayer.h>
#include <imgui_stdlib.h>
#include <scene/PrefabSerializer.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <string>
#include <string_view>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::test;
using namespace owl::scene::component;

namespace {

/// Mouse buttons used by the simulated input.
constexpr int g_leftButton = 0;

/**
 * @brief
 *  Headless ImGui harness driving an `InspectorEditTracker` with simulated mouse and keyboard input.
 */
class InspectorEditTest : public NestTest {
protected:
	void SetUp() override {
		NestTest::SetUp();
		m_layer.disableApp();
		m_layer.onAttach();
		m_entity = m_scene.createEntity("Edited");
	}

	void TearDown() override {
		m_layer.onDetach();
		NestTest::TearDown();
	}

	// Run one ImGui frame: an optional source window, then the inspector window holding the bodies.
	void frame(const std::function<void()>& iBodies, const std::function<void()>& iSource = {}) {
		m_layer.begin();
		if (iSource) {
			ImGui::SetNextWindowPos({450.f, 0.f});
			ImGui::SetNextWindowSize({300.f, 200.f});
			ImGui::Begin("Source");
			iSource();
			ImGui::End();
		}
		ImGui::SetNextWindowPos({0.f, 0.f});
		ImGui::SetNextWindowSize({400.f, 500.f});
		ImGui::Begin("Inspector");
		m_tracker.beginFrame(m_entity, &m_scene, &m_undo);
		iBodies();
		m_tracker.endFrame(&m_scene, &m_undo);
		ImGui::End();
		m_layer.end();
	}

	// Draw one component body wrapped by the tracker.
	void body(const char* iKey, const char* iName, const std::function<void()>& iWidgets) {
		m_tracker.beginComponent(m_entity, iKey);
		iWidgets();
		m_tracker.endComponent(m_entity, iKey, iName, &m_undo);
	}

	// Center of the last submitted item.
	static auto itemCenter() -> ImVec2 {
		const auto min = ImGui::GetItemRectMin();
		const auto max = ImGui::GetItemRectMax();
		return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
	}

	static void moveMouse(const ImVec2& iPos) { ImGui::GetIO().AddMousePosEvent(iPos.x, iPos.y); }

	static void setButton(const bool iDown) { ImGui::GetIO().AddMouseButtonEvent(g_leftButton, iDown); }

	// Count the undo steps by undoing everything, then redo them back.
	auto undoDepth() -> size_t {
		size_t depth = 0;
		while (m_undo.canUndo()) {
			m_undo.undo(m_scene);
			++depth;
		}
		for (size_t i = 0; i < depth; ++i) m_undo.redo(m_scene);
		return depth;
	}

	// Body with a DragFloat bound to the circle thickness; records the widget center.
	void thicknessBody() {
		body(CircleRenderer::key(), CircleRenderer::name(), [this]() -> void {
			ImGui::DragFloat("Thickness", &m_entity.getComponent<CircleRenderer>().thickness, 0.01f);
			m_target = itemCenter();
		});
	}

	// Drag the thickness field horizontally by iDelta pixels, then release.
	void dragThickness(const float iDelta) {
		moveMouse(m_target);
		frame([this]() -> void { thicknessBody(); });
		setButton(true);
		frame([this]() -> void { thicknessBody(); });
		for (int step = 1; step <= 4; ++step) {
			moveMouse({m_target.x + iDelta * static_cast<float>(step) / 4.f, m_target.y});
			frame([this]() -> void { thicknessBody(); });
		}
		setButton(false);
		frame([this]() -> void { thicknessBody(); });
		moveMouse(m_target);
		frame([this]() -> void { thicknessBody(); });
	}

	/// ImGui context owner.
	gui::UiLayer m_layer;
	/// The tracker under test.
	panel::InspectorEditTracker m_tracker;
	/// The inspected entity.
	scene::Entity m_entity;
	/// Screen position of the widget the test interacts with.
	ImVec2 m_target{0.f, 0.f};
};

}// namespace

TEST_F(InspectorEditTest, NoSerializationWithoutInteraction) {
	m_entity.addComponent<CircleRenderer>();
	const auto start = panel::InspectorEditTracker::serializationCount();
	frame([this]() -> void { thicknessBody(); });
	moveMouse(m_target);
	for (int i = 0; i < 30; ++i) {
		moveMouse({m_target.x + static_cast<float>(i), m_target.y});
		frame([this]() -> void { thicknessBody(); });
	}
	EXPECT_EQ(panel::InspectorEditTracker::serializationCount(), start);
	EXPECT_FALSE(m_tracker.isEditing());
	EXPECT_FALSE(m_undo.canUndo());
}

TEST_F(InspectorEditTest, DragRecordsOneStep) {
	m_entity.addComponent<CircleRenderer>().thickness = 0.5f;
	frame([this]() -> void { thicknessBody(); });
	const auto start = panel::InspectorEditTracker::serializationCount();
	dragThickness(40.f);
	const float edited = m_entity.getComponent<CircleRenderer>().thickness;
	EXPECT_GT(edited, 0.5f);
	EXPECT_FALSE(m_tracker.isEditing());
	EXPECT_EQ(undoDepth(), 1u);
	EXPECT_EQ(m_undo.undoDescription(), "Modify Circle Renderer");
	EXPECT_LE(panel::InspectorEditTracker::serializationCount() - start, 4u);

	m_undo.undo(m_scene);
	EXPECT_FLOAT_EQ(m_entity.getComponent<CircleRenderer>().thickness, 0.5f);
	m_undo.redo(m_scene);
	EXPECT_FLOAT_EQ(m_entity.getComponent<CircleRenderer>().thickness, edited);
}

TEST_F(InspectorEditTest, RapidEditsMerge) {
	m_entity.addComponent<CircleRenderer>().thickness = 0.5f;
	frame([this]() -> void { thicknessBody(); });
	dragThickness(40.f);
	dragThickness(40.f);
	EXPECT_GT(m_entity.getComponent<CircleRenderer>().thickness, 0.5f);
	EXPECT_EQ(undoDepth(), 1u);
	m_undo.undo(m_scene);
	EXPECT_FLOAT_EQ(m_entity.getComponent<CircleRenderer>().thickness, 0.5f);
}

TEST_F(InspectorEditTest, ClickWithoutChangeRecordsNothing) {
	m_entity.addComponent<CircleRenderer>().thickness = 0.5f;
	frame([this]() -> void { thicknessBody(); });
	moveMouse(m_target);
	frame([this]() -> void { thicknessBody(); });
	setButton(true);
	frame([this]() -> void { thicknessBody(); });
	setButton(false);
	frame([this]() -> void { thicknessBody(); });
	frame([this]() -> void { thicknessBody(); });
	EXPECT_FALSE(m_tracker.isEditing());
	EXPECT_FALSE(m_undo.canUndo());
}

TEST_F(InspectorEditTest, CheckboxRecordsOneStep) {
	m_entity.addComponent<Camera>().primary = true;
	const auto draw = [this]() -> void {
		body(Camera::key(), Camera::name(), [this]() -> void {
			ImGui::Checkbox("Primary", &m_entity.getComponent<Camera>().primary);
			m_target = itemCenter();
		});
	};
	frame(draw);
	moveMouse({m_target.x - 40.f, m_target.y});
	frame(draw);
	moveMouse(m_target);
	frame(draw);
	setButton(true);
	frame(draw);
	setButton(false);
	frame(draw);
	frame(draw);
	EXPECT_FALSE(m_entity.getComponent<Camera>().primary);
	EXPECT_EQ(undoDepth(), 1u);
	m_undo.undo(m_scene);
	EXPECT_TRUE(m_entity.getComponent<Camera>().primary);
	m_undo.redo(m_scene);
	EXPECT_FALSE(m_entity.getComponent<Camera>().primary);
}

TEST_F(InspectorEditTest, ComboSelectionRecordsOneStep) {
	m_entity.addComponent<RendererTag>().rendererName = "First";
	ImVec2 itemPos{0.f, 0.f};
	const auto draw = [this, &itemPos]() -> void {
		body(RendererTag::key(), RendererTag::name(), [this, &itemPos]() -> void {
			auto& name = m_entity.getComponent<RendererTag>().rendererName;
			if (ImGui::BeginCombo("Layer", name.c_str())) {
				for (const char* option: {"First", "Second"}) {
					if (ImGui::Selectable(option, name == option))
						name = option;
					if (std::string_view{option} == "Second")
						itemPos = itemCenter();
				}
				ImGui::EndCombo();
				return;
			}
			m_target = itemCenter();
		});
	};
	frame(draw);
	moveMouse(m_target);
	frame(draw);
	setButton(true);
	frame(draw);
	setButton(false);
	frame(draw);
	ASSERT_TRUE(m_tracker.isEditing());
	frame(draw);
	frame(draw);
	moveMouse(itemPos);
	frame(draw);
	frame(draw);
	setButton(true);
	frame(draw);
	setButton(false);
	frame(draw);
	frame(draw);
	EXPECT_EQ(m_entity.getComponent<RendererTag>().rendererName, "Second");
	EXPECT_FALSE(m_tracker.isEditing());
	EXPECT_EQ(undoDepth(), 1u);
	m_undo.undo(m_scene);
	EXPECT_EQ(m_entity.getComponent<RendererTag>().rendererName, "First");
}

TEST_F(InspectorEditTest, TextInputRecordsOneStepOnValidation) {
	m_entity.addComponent<EntityLink>();
	const auto draw = [this]() -> void {
		body(EntityLink::key(), EntityLink::name(), [this]() -> void {
			ImGui::InputText("Linked", &m_entity.getComponent<EntityLink>().linkedEntityName);
			m_target = itemCenter();
		});
	};
	frame(draw);
	moveMouse(m_target);
	frame(draw);
	setButton(true);
	frame(draw);
	setButton(false);
	frame(draw);
	for (const char chr: std::string_view{"door"}) {
		ImGui::GetIO().AddInputCharacter(static_cast<unsigned int>(chr));
		frame(draw);
	}
	EXPECT_EQ(m_entity.getComponent<EntityLink>().linkedEntityName, "door");
	EXPECT_TRUE(m_tracker.isEditing());
	EXPECT_FALSE(m_undo.canUndo());
	ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true);
	frame(draw);
	ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, false);
	frame(draw);
	frame(draw);
	EXPECT_FALSE(m_tracker.isEditing());
	EXPECT_EQ(undoDepth(), 1u);
	m_undo.undo(m_scene);
	EXPECT_TRUE(m_entity.getComponent<EntityLink>().linkedEntityName.empty());
	m_undo.redo(m_scene);
	EXPECT_EQ(m_entity.getComponent<EntityLink>().linkedEntityName, "door");
}

TEST_F(InspectorEditTest, DragAndDropIntoSlotRecordsOneStep) {
	m_entity.addComponent<EntityLink>();
	ImVec2 sourcePos{0.f, 0.f};
	const auto draw = [this]() -> void {
		body(EntityLink::key(), EntityLink::name(), [this]() -> void {
			ImGui::Button("Slot", {200.f, 40.f});
			m_target = itemCenter();
			if (ImGui::BeginDragDropTarget()) {
				if (const auto* payload = ImGui::AcceptDragDropPayload("ENTITY_NAME"); payload != nullptr)
					m_entity.getComponent<EntityLink>().linkedEntityName =
							std::string{static_cast<const char*>(payload->Data)};
				ImGui::EndDragDropTarget();
			}
		});
	};
	const auto source = [&sourcePos]() -> void {
		ImGui::Button("Asset", {100.f, 40.f});
		sourcePos = itemCenter();
		if (ImGui::BeginDragDropSource()) {
			constexpr std::string_view name{"target"};
			ImGui::SetDragDropPayload("ENTITY_NAME", name.data(), name.size() + 1);
			ImGui::EndDragDropSource();
		}
	};
	frame(draw, source);
	moveMouse(sourcePos);
	frame(draw, source);
	setButton(true);
	frame(draw, source);
	for (int step = 1; step <= 5; ++step) {
		const float t = static_cast<float>(step) / 5.f;
		moveMouse({sourcePos.x + (m_target.x - sourcePos.x) * t, sourcePos.y + (m_target.y - sourcePos.y) * t});
		frame(draw, source);
	}
	frame(draw, source);
	setButton(false);
	frame(draw, source);
	frame(draw, source);
	EXPECT_EQ(m_entity.getComponent<EntityLink>().linkedEntityName, "target");
	EXPECT_FALSE(m_tracker.isEditing());
	EXPECT_EQ(undoDepth(), 1u);
	m_undo.undo(m_scene);
	EXPECT_TRUE(m_entity.getComponent<EntityLink>().linkedEntityName.empty());
}

TEST_F(InspectorEditTest, SelectionChangeEndsTheEdit) {
	m_entity.addComponent<EntityLink>();
	const auto edited = m_entity;
	const auto draw = [this]() -> void {
		if (!m_entity.hasComponent<EntityLink>())
			return;
		body(EntityLink::key(), EntityLink::name(), [this]() -> void {
			ImGui::InputText("Linked", &m_entity.getComponent<EntityLink>().linkedEntityName);
			m_target = itemCenter();
		});
	};
	frame(draw);
	moveMouse(m_target);
	frame(draw);
	setButton(true);
	frame(draw);
	setButton(false);
	frame(draw);
	ImGui::GetIO().AddInputCharacter('x');
	frame(draw);
	ASSERT_TRUE(m_tracker.isEditing());
	m_entity = m_scene.createEntity("Other");
	frame(draw);
	EXPECT_FALSE(m_tracker.isEditing());
	EXPECT_EQ(undoDepth(), 1u);
	m_undo.undo(m_scene);
	EXPECT_TRUE(edited.getComponent<EntityLink>().linkedEntityName.empty());
}

TEST_F(InspectorEditTest, PrefabInstanceEditMarksOverride) {
	const auto prefabPath = std::filesystem::temp_directory_path() /
							std::format("owl_nest_inspector_{}.owlprefab", static_cast<uint64_t>(core::UUID{}));
	{
		scene::Scene source;
		auto root = source.createEntity("Crate");
		auto lid = source.createEntity("Lid");
		lid.addComponent<CircleRenderer>().thickness = 0.5f;
		source.setParent(lid, root);
		ASSERT_TRUE(scene::PrefabSerializer::serialize(root, source, prefabPath, "Crate"));
	}
	const auto root = scene::PrefabSerializer::instantiate(
			prefabPath, shared<scene::Scene>(shared<scene::Scene>{}, &m_scene), "prefabs/crate.owlprefab");
	std::filesystem::remove(prefabPath);
	ASSERT_TRUE(root);
	m_entity = m_scene.findEntityByUUID(childrenOf(root).front());
	ASSERT_TRUE(m_entity.hasComponent<CircleRenderer>());
	frame([this]() -> void { thicknessBody(); });
	dragThickness(40.f);
	const auto& link = root.getComponent<PrefabLink>();
	const auto canonical = link.findCanonicalUuid(static_cast<uint64_t>(m_entity.getUUID()));
	ASSERT_TRUE(canonical.has_value());
	EXPECT_TRUE(link.isOverridden(*canonical, CircleRenderer::key()));
	EXPECT_EQ(link.overriddenComponents.size(), 1u);
	m_undo.undo(m_scene);
	EXPECT_TRUE(root.getComponent<PrefabLink>().overriddenComponents.empty());
	EXPECT_FLOAT_EQ(m_entity.getComponent<CircleRenderer>().thickness, 0.5f);
	m_undo.redo(m_scene);
	EXPECT_TRUE(root.getComponent<PrefabLink>().isOverridden(*canonical, CircleRenderer::key()));
}

TEST_F(InspectorEditTest, InspectorIdleFramesDoNotSerialize) {
	auto scene = mkShared<scene::Scene>();
	auto entity = scene->createEntity("Rich");
	entity.addComponent<Camera>();
	entity.addComponent<SpriteRenderer>();
	entity.addComponent<CircleRenderer>();
	entity.addComponent<Text>();
	entity.addComponent<PhysicBody>();
	entity.addComponent<Trigger>();
	entity.addComponent<Player>();
	entity.addComponent<EntityLink>();
	entity.addComponent<SoundListener>();
	entity.addComponent<RendererTag>();
	entity.addComponent<FlyCamera>();
	panel::SceneHierarchy hierarchy{scene};
	hierarchy.setUndoManager(&m_undo);
	hierarchy.setSelectedEntity(entity);
	const auto start = panel::InspectorEditTracker::serializationCount();
	constexpr int warmup = 20;
	constexpr int frames = 300;
	using clk = std::chrono::steady_clock;
	clk::duration total{};
	for (int i = 0; i < warmup + frames; ++i) {
		moveMouse({static_cast<float>(i % 400), static_cast<float>((i * 7) % 600)});
		m_layer.begin();
		const auto t0 = clk::now();
		hierarchy.onImGuiRender();
		if (i >= warmup)
			total += clk::now() - t0;
		m_layer.end();
	}
	std::fputs(std::format("Inspector idle frame, 13 components: {:.4f} ms.\n",
						   std::chrono::duration<double, std::milli>{total}.count() / frames)
					   .c_str(),
			   stdout);
	EXPECT_EQ(panel::InspectorEditTracker::serializationCount(), start);
	EXPECT_FALSE(m_undo.canUndo());
}
