/**
 * @file MaterialDocument_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/MaterialDocument.h"
#include "document/viewer/MeshPreview.h"
#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>

using namespace owl;
using namespace owl::nest;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class MaterialDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_material_document_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_file = m_root / "gold.owlmat";
		renderer::MaterialAsset asset;
		asset.values.metallic = 1.f;
		ASSERT_TRUE(asset.save(m_file));
		ASSERT_TRUE(m_document.loadFromFile(m_file));
	}

	void TearDown() override {
		m_document.onDetach();
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	std::filesystem::path m_root;
	std::filesystem::path m_file;
	MaterialDocument m_document;
};
OWL_DIAG_POP

}// namespace

TEST_F(MaterialDocumentTest, ReadsTheFile) {
	EXPECT_EQ(m_document.type(), DocumentType::Material);
	EXPECT_EQ(m_document.title(), "gold.owlmat");
	EXPECT_EQ(m_document.filePath(), m_file);
	EXPECT_FLOAT_EQ(m_document.asset().values.metallic, 1.f);
	EXPECT_FALSE(m_document.isDirty());
	EXPECT_FALSE(m_document.canUndo());
	m_document.onUpdate(core::Timestep{});
}

TEST_F(MaterialDocumentTest, RefusesAFileThatIsNotAMaterial) {
	const auto bad = m_root / "bad.owlmat";
	EXPECT_FALSE(m_document.loadFromFile(bad));
	EXPECT_EQ(m_document.filePath(), m_file);
}

TEST_F(MaterialDocumentTest, EditsAreUndoableAndTrackTheDirtyState) {
	auto values = m_document.asset().values;
	values.roughness = 0.2f;
	EXPECT_TRUE(m_document.setValues(values, renderer::g_allMaterialFields));
	EXPECT_FALSE(m_document.setValues(values, renderer::g_allMaterialFields)) << "no change, no step";
	EXPECT_TRUE(m_document.isDirty());
	EXPECT_TRUE(m_document.canUndo());
	m_document.performUndo();
	EXPECT_FLOAT_EQ(m_document.asset().values.roughness, renderer::MaterialParameters{}.roughness);
	EXPECT_FALSE(m_document.isDirty());
	EXPECT_TRUE(m_document.canRedo());
	m_document.performRedo();
	EXPECT_FLOAT_EQ(m_document.asset().values.roughness, 0.2f);
	EXPECT_TRUE(m_document.isDirty());
}

TEST_F(MaterialDocumentTest, SliderDragsMergeIntoOneStep) {
	auto values = m_document.asset().values;
	for (const float roughness: {0.1f, 0.2f, 0.3f}) {
		values.roughness = roughness;
		EXPECT_TRUE(m_document.setValues(values, renderer::g_allMaterialFields, true));
	}
	m_document.performUndo();
	EXPECT_FLOAT_EQ(m_document.asset().values.roughness, renderer::MaterialParameters{}.roughness);
	EXPECT_FALSE(m_document.canUndo());
}

TEST_F(MaterialDocumentTest, ParentOverridesAndSaveRoundTrip) {
	EXPECT_TRUE(m_document.setParent("materials/base.owlmat"));
	EXPECT_FALSE(m_document.setParent("materials/base.owlmat"));
	EXPECT_FALSE(m_document.setParent("materials/gold.png")) << "only a material can be a parent";
	EXPECT_FALSE(m_document.setParent("other/gold.owlmat")) << "not its own parent";
	auto values = m_document.asset().values;
	values.roughness = 0.9f;
	EXPECT_TRUE(m_document.setValues(values, static_cast<uint32_t>(renderer::MaterialField::Roughness)));
	ASSERT_TRUE(m_document.save());
	EXPECT_FALSE(m_document.isDirty());

	const auto reloaded = renderer::MaterialAsset::load(m_file);
	ASSERT_TRUE(reloaded.has_value());
	EXPECT_EQ(reloaded->parent.generic_string(), "materials/base.owlmat");
	EXPECT_EQ(reloaded->overrides, static_cast<uint32_t>(renderer::MaterialField::Roughness));
	EXPECT_FLOAT_EQ(reloaded->values.roughness, 0.9f);

	EXPECT_TRUE(m_document.setParent({}));
	EXPECT_TRUE(m_document.asset().parent.empty());
	m_document.performUndo();
	EXPECT_EQ(m_document.asset().parent.generic_string(), "materials/base.owlmat");
}

TEST_F(MaterialDocumentTest, SavesAsAnotherFileAndRestoresAnAutosave) {
	auto values = m_document.asset().values;
	values.baseColor = {1.f, 0.f, 0.f, 1.f};
	ASSERT_TRUE(m_document.setValues(values, renderer::g_allMaterialFields));
	const auto snapshot = m_document.recoverySnapshot();
	ASSERT_TRUE(snapshot.has_value());

	const auto copy = m_root / "red.owlmat";
	ASSERT_TRUE(m_document.saveAs(copy));
	EXPECT_EQ(m_document.filePath(), copy);
	EXPECT_TRUE(exists(copy));

	MaterialDocument other;
	EXPECT_TRUE(other.restoreRecoverySnapshot(*snapshot));
	EXPECT_FLOAT_EQ(other.asset().values.baseColor.x(), 1.f);
	EXPECT_TRUE(other.isDirty());
	EXPECT_FALSE(other.restoreRecoverySnapshot("not: [a material"));
}

TEST_F(MaterialDocumentTest, BuildsThePreviewMaterialFromTheEditedValues) {
	auto values = m_document.asset().values;
	values.roughness = 0.25f;
	ASSERT_TRUE(m_document.setValues(values, renderer::g_allMaterialFields));
	EXPECT_FLOAT_EQ(m_document.buildMaterial().getParameters().roughness, 0.25f);
	EXPECT_FLOAT_EQ(m_document.buildMaterial().getParameters().metallic, 1.f);
}

TEST_F(MaterialDocumentTest, PreviewShapeResetsTheView) {
	EXPECT_EQ(m_document.previewShape(), viewer::PreviewShape::Sphere);
	m_document.setPreviewShape(viewer::PreviewShape::Cube);
	EXPECT_EQ(m_document.previewShape(), viewer::PreviewShape::Cube);
}

TEST(MaterialPreview, BuildsClosedShapes) {
	core::Log::init(core::Log::Level::Off);
	for (const auto shape: {viewer::PreviewShape::Sphere, viewer::PreviewShape::Cube}) {
		const auto mesh = viewer::makePreviewShape(shape);
		ASSERT_NE(mesh, nullptr);
		EXPECT_FALSE(mesh->getGeometry().isEmpty());
		EXPECT_EQ(mesh->getSubmeshes().size(), 1u);
		EXPECT_GT(mesh->getGeometry().boundsMax.x(), 0.5f);
		EXPECT_LT(mesh->getGeometry().boundsMin.y(), -0.5f);
	}
	core::Log::invalidate();
}
