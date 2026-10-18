/**
 * @file DemoHudLayout_test.cpp
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/SettingsManager.h>
#include <scene/component/components.h>
#include <scene/ui/HudLayout.h>

#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <ostream>
#include <set>
#include <string>
#include <system_error>
#include <vector>

using namespace owl;
using scene::component::UiRect;
using scene::ui::HudNode;

namespace {

/// The demo documents under test: the HUDs of `engine_demo/huds` and the canvases of the four menu scenes.
struct DemoDocument {
	/// Path under `engine_demo`.
	std::string path;
	/// Whether it is an `.owlhud` (else a scene).
	bool isHud = true;
};

auto operator<<(std::ostream& iOut, const DemoDocument& iDocument) -> std::ostream& { return iOut << iDocument.path; }

/// Viewports from 4:3 to 21:9, from 640x360 to 2560x1440 (and the ultra-wide 3440x1440).
const std::vector<math::vec2ui> g_viewports{{640u, 360u},   {800u, 600u},   {1024u, 768u},  {1280u, 720u},
											{1920u, 1080u}, {2560u, 1080u}, {2560u, 1440u}, {3440u, 1440u}};

/// Player interface sizes of the settings slider, from 75 % to 200 %.
const std::vector<float> g_uiScales{0.75f, 1.f, 1.25f, 1.5f, 2.f};

/// Overlap tolerance, in screen pixels (anti-aliased borders and rounding).
constexpr float g_tolerance = 1.f;

auto describe(const scene::Scene& iScene, const HudNode& iNode) -> std::string {
	const scene::Entity entity{iNode.entity, const_cast<scene::Scene*>(&iScene)};
	return std::format("{} [{:.0f},{:.0f} {:.0f}x{:.0f}]", entity.getComponent<scene::component::Tag>().tag,
					   iNode.bounds.position.x(), iNode.bounds.position.y(), iNode.bounds.size.x(),
					   iNode.bounds.size.y());
}

auto overlaps(const UiRect::Bounds& iA, const UiRect::Bounds& iB) -> bool {
	return iA.position.x() + iA.size.x() - g_tolerance > iB.position.x() &&
		   iB.position.x() + iB.size.x() - g_tolerance > iA.position.x() &&
		   iA.position.y() + iA.size.y() - g_tolerance > iB.position.y() &&
		   iB.position.y() + iB.size.y() - g_tolerance > iA.position.y();
}

auto contains(const UiRect::Bounds& iOuter, const UiRect::Bounds& iInner) -> bool {
	return iInner.position.x() >= iOuter.position.x() - g_tolerance &&
		   iInner.position.y() >= iOuter.position.y() - g_tolerance &&
		   iInner.position.x() + iInner.size.x() <= iOuter.position.x() + iOuter.size.x() + g_tolerance &&
		   iInner.position.y() + iInner.size.y() <= iOuter.position.y() + iOuter.size.y() + g_tolerance;
}

class DemoHudLayoutTest : public ::testing::TestWithParam<DemoDocument> {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }
};

// Rules of the overlap check: doc/pages/design/hud-system.md, "Demo HUD rules".
void checkLayout(const shared<scene::Scene>& iScene, const std::string& iName) {
	const auto& scn = iScene;
	std::set<entt::entity> popups;
	for (const auto entity: scn->registry.view<scene::component::Visibility>()) {
		auto& visibility = scn->registry.get<scene::component::Visibility>(entity);
		if (!visibility.gameVisible && scn->registry.all_of<scene::component::UiWindow>(entity))
			popups.insert(entity);
		visibility.gameVisible = true;
	}

	app::EngineContext context;
	scn->setEngineContext(&context);
	size_t topLevelSeen = 0;
	for (const float uiScale: g_uiScales) {
		context.getSettings().set("ui_scale", uiScale);
		for (const auto& viewport: g_viewports) {
			const std::string where =
					std::format("{} at {}x{}, ui_scale {}", iName, viewport.x(), viewport.y(), uiScale);
			auto& layout = scn->getHudLayout();
			layout.compute(*scn, viewport, false);
			const auto nodes = layout.getNodes();
			const UiRect::Bounds screen{.position = {0.f, 0.f},
										.size = {static_cast<float>(viewport.x()), static_cast<float>(viewport.y())}};
			std::vector<const HudNode*> topLevel;
			for (const auto& node: nodes) {
				if (node.parent == HudNode::noParent && node.bounds.size.x() > 0.f && node.bounds.size.y() > 0.f)
					topLevel.push_back(&node);
				if (node.parent == HudNode::noParent)
					continue;
				const auto& parent = nodes[node.parent];
				if (!scn->registry.all_of<scene::component::UiLayout>(parent.entity) ||
					scn->registry.all_of<scene::component::UiScrollView>(parent.entity))
					continue;
				EXPECT_TRUE(contains(parent.content, node.bounds))
						<< where << ": " << describe(*scn, node) << " leaves its layout container "
						<< describe(*scn, parent);
			}
			topLevelSeen += topLevel.size();
			for (const auto* node: topLevel)
				EXPECT_TRUE(contains(screen, node->bounds))
						<< where << ": " << describe(*scn, *node) << " leaves the viewport";
			for (size_t i = 0; i < topLevel.size(); ++i) {
				for (size_t j = i + 1; j < topLevel.size(); ++j) {
					if (popups.contains(topLevel[i]->entity) != popups.contains(topLevel[j]->entity))
						continue;
					EXPECT_FALSE(overlaps(topLevel[i]->bounds, topLevel[j]->bounds))
							<< where << ": " << describe(*scn, *topLevel[i]) << " overlaps "
							<< describe(*scn, *topLevel[j]);
				}
			}
		}
	}
	EXPECT_GT(topLevelSeen, 0u) << "nothing was laid out for " << iName;
	scn->setEngineContext(nullptr);
}

auto loadDocument(const std::filesystem::path& iFile) -> shared<scene::Scene> {
	auto scn = mkShared<scene::Scene>();
	const auto kind = iFile.extension() == ".owlhud" ? scene::DocumentKind::Hud : scene::DocumentKind::Scene;
	if (!scene::SceneSerializer(scn, kind).deserialize(iFile).has_value())
		return nullptr;
	return scn;
}

}// namespace

TEST_P(DemoHudLayoutTest, noTwoTopLevelWidgetsOverlapAtAnyScaleOrAspect) {
	const auto& document = GetParam();
	const auto file = owl::test::getRootPath() / "engine_demo" / document.path;
	ASSERT_TRUE(std::filesystem::exists(file)) << file;
	const auto scn = loadDocument(file);
	ASSERT_NE(scn, nullptr) << file;
	checkLayout(scn, document.path);
}

// Every HUD of a sample game, and every scene of it holding a canvas (its menus), follows the demo rules.
TEST(SampleHudLayout, noTwoTopLevelWidgetsOverlapInAnySampleGame) {
	core::Log::init(core::Log::Level::Off);
	const auto samples = owl::test::getRootPath() / "samples";
	std::error_code ec;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(samples, ec)) {
		const auto& file = entry.path();
		if (file.extension() != ".owlhud" && file.extension() != ".owl")
			continue;
		const auto scn = loadDocument(file);
		ASSERT_NE(scn, nullptr) << file;
		if (file.extension() == ".owl" && scn->registry.view<scene::component::Canvas>().empty())
			continue;
		checkLayout(scn, file.lexically_relative(samples).generic_string());
	}
	core::Log::invalidate();
}


INSTANTIATE_TEST_SUITE_P(
		DemoHuds, DemoHudLayoutTest,
		::testing::Values(DemoDocument{"huds/gameplay.owlhud", true}, DemoDocument{"huds/iso_town.owlhud", true},
						  DemoDocument{"huds/raycast.owlhud", true}, DemoDocument{"huds/voxel.owlhud", true},
						  DemoDocument{"huds/world_map.owlhud", true}, DemoDocument{"scenes/main_menu.owl", false},
						  DemoDocument{"scenes/settings_menu.owl", false}, DemoDocument{"scenes/game_over.owl", false},
						  DemoDocument{"scenes/victory.owl", false}),
		[](const ::testing::TestParamInfo<DemoDocument>& iInfo) -> std::string {
			auto name = std::filesystem::path{iInfo.param.path}.stem().string();
			return name;
		});

TEST(DemoHudSettings, theInterfaceSizeDefaultsToOneHundredPercent) {
	const auto file = owl::test::getRootPath() / "engine_demo" / "game_settings.yml";
	std::ifstream in(file);
	ASSERT_TRUE(in.is_open()) << file;
	std::string line;
	bool found = false;
	while (std::getline(in, line)) {
		if (line.contains("key: ui_scale")) {
			found = true;
			std::string type;
			std::string value;
			std::getline(in, type);
			std::getline(in, value);
			EXPECT_TRUE(value.contains("value: 1.0")) << value;
		}
	}
	EXPECT_TRUE(found);
}
