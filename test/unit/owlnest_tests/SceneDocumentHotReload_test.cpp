/**
 * @file SceneDocumentHotReload_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/SceneDocument.h"
#include "nestTestHelper.h"

#include <scene/component/Tag.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

using namespace owl;
using namespace owl::nest;

namespace {

auto hasEntityNamed(const scene::Scene& iScene, const std::string& iName) -> bool {
	const auto view = iScene.registry.view<scene::component::Tag>();
	return std::ranges::any_of(view, [&view, &iName](const auto iEntity) -> bool {
		return view.get<scene::component::Tag>(iEntity).tag == iName;
	});
}

void writeText(const std::filesystem::path& iPath, const std::string& iContent) {
	std::ofstream file(iPath, std::ios::binary | std::ios::trunc);
	file << iContent;
}

class SceneDocumentHotReload : public test::NestTest {};

}// namespace

TEST_F(SceneDocumentHotReload, CleanDocumentTakesTheFileAndKeepsItOnError) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_scenedoc_hotreload";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	const auto path = dir / "level.owl";

	const auto open = mkShared<scene::Scene>();
	static_cast<void>(open->createEntity("First"));
	SceneDocument doc;
	doc.applyLoadedScene(open, path, {64, 64});
	EXPECT_FALSE(doc.reloadFromDisk({64, 64}));

	const auto onDisk = mkShared<scene::Scene>();
	static_cast<void>(onDisk->createEntity("Second"));
	writeText(path, scene::SceneSerializer(onDisk).serializeToString());
	EXPECT_TRUE(doc.reloadFromDisk({64, 64}));
	ASSERT_TRUE(doc.getEditorScene());
	EXPECT_TRUE(hasEntityNamed(*doc.getEditorScene(), "Second"));
	EXPECT_TRUE(doc.consumeSceneSwapped());

	EXPECT_FALSE(doc.reloadFromDisk({64, 64}));

	writeText(path, "Scene: [unterminated");
	EXPECT_FALSE(doc.reloadFromDisk({64, 64}));
	EXPECT_TRUE(hasEntityNamed(*doc.getEditorScene(), "Second"));

	std::filesystem::remove_all(dir);
}
