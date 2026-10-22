/**
 * @file Material_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/Material.h>
#include <renderer/MaterialLibrary.h>
#include <renderer/gpu/RenderCommand.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>

using namespace owl;
using namespace owl::renderer;

namespace {
class MaterialTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		gpu::RenderCommand::create(gpu::RenderAPI::Type::Null);
		m_dir = std::filesystem::temp_directory_path() / "owl_material_test";
		std::filesystem::create_directories(m_dir);
	}

	void TearDown() override {
		std::filesystem::remove_all(m_dir);
		gpu::RenderCommand::invalidate();
		core::Log::invalidate();
	}

	[[nodiscard]] auto write(const std::string& iName, const MaterialAsset& iAsset) const -> std::filesystem::path {
		const auto file = m_dir / iName;
		EXPECT_TRUE(iAsset.save(file));
		return file;
	}

	/// Folder of the material files.
	std::filesystem::path m_dir;
};

auto gold() -> MaterialAsset {
	MaterialAsset asset;
	asset.values.baseColor = {1.f, 0.8f, 0.3f, 1.f};
	asset.values.metallic = 1.f;
	asset.values.roughness = 0.25f;
	asset.values.emissive = {0.1f, 0.f, 0.f};
	asset.values.clearcoat = 0.5f;
	asset.values.shadingModel = ShadingModel::Toon;
	asset.values.alphaMode = AlphaMode::Mask;
	asset.values.maps[static_cast<size_t>(MaterialMap::Normal)] = "nam:textures/gold_normal.png";
	return asset;
}

}// namespace

TEST_F(MaterialTest, aRootMaterialRoundTripsEveryField) {
	const auto read = MaterialAsset::fromYaml(gold().toYaml());
	ASSERT_TRUE(read.has_value());
	EXPECT_TRUE(read->parent.empty());
	EXPECT_EQ(read->overrides, g_allMaterialFields);
	EXPECT_FLOAT_EQ(read->values.metallic, 1.f);
	EXPECT_FLOAT_EQ(read->values.roughness, 0.25f);
	EXPECT_FLOAT_EQ(read->values.baseColor.y(), 0.8f);
	EXPECT_FLOAT_EQ(read->values.clearcoat, 0.5f);
	EXPECT_EQ(read->values.shadingModel, ShadingModel::Toon);
	EXPECT_EQ(read->values.alphaMode, AlphaMode::Mask);
	EXPECT_EQ(read->values.maps[static_cast<size_t>(MaterialMap::Normal)], "nam:textures/gold_normal.png");
}

TEST_F(MaterialTest, anInstanceWritesOnlyItsOverrides) {
	MaterialAsset instance;
	instance.parent = "materials/gold.owlmat";
	instance.values.roughness = 0.9f;
	instance.overrides = static_cast<uint32_t>(MaterialField::Roughness);
	const auto text = instance.toYaml();
	EXPECT_NE(text.find("roughness"), std::string::npos);
	EXPECT_EQ(text.find("metallic"), std::string::npos);
	const auto read = MaterialAsset::fromYaml(text);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->parent.generic_string(), "materials/gold.owlmat");
	EXPECT_EQ(read->overrides, static_cast<uint32_t>(MaterialField::Roughness));
}

TEST_F(MaterialTest, aDocumentThatIsNotAMaterialIsRefused) {
	EXPECT_FALSE(MaterialAsset::fromYaml("Scene: x").has_value());
	EXPECT_FALSE(MaterialAsset::fromYaml("[not, a, map").has_value());
	EXPECT_FALSE(MaterialAsset::load(m_dir / "missing.owlmat").has_value());
}

TEST_F(MaterialTest, overridesApplyOnlyTheirFieldsAndRoundTrip) {
	MaterialOverrides overrides;
	overrides.values.baseColor = {0.f, 1.f, 0.f, 1.f};
	overrides.set(MaterialField::BaseColor);
	MaterialParameters values = gold().values;
	overrides.applyTo(values);
	EXPECT_FLOAT_EQ(values.baseColor.y(), 1.f);
	EXPECT_FLOAT_EQ(values.metallic, 1.f);
	const auto read = MaterialOverrides::fromYaml(overrides.toYaml());
	EXPECT_EQ(read, overrides);
	overrides.set(MaterialField::BaseColor, false);
	EXPECT_TRUE(overrides.isEmpty());
	EXPECT_TRUE(overrides.toYaml().empty());
}

TEST_F(MaterialTest, theLibraryResolvesAParentChain) {
	const auto parent = write("gold.owlmat", gold());
	MaterialAsset rough;
	rough.parent = parent;
	rough.values.roughness = 0.9f;
	rough.overrides = static_cast<uint32_t>(MaterialField::Roughness);
	const auto child = write("rough_gold.owlmat", rough);
	MaterialLibrary library;
	const auto material = library.get(child);
	ASSERT_NE(material, nullptr);
	EXPECT_FLOAT_EQ(material->getParameters().roughness, 0.9f);
	EXPECT_FLOAT_EQ(material->getParameters().metallic, 1.f);
	EXPECT_EQ(library.get(child), material);
}

TEST_F(MaterialTest, theLibraryRefusesACyclicChain) {
	MaterialAsset a;
	a.parent = m_dir / "b.owlmat";
	a.overrides = 0;
	MaterialAsset b;
	b.parent = m_dir / "a.owlmat";
	b.overrides = 0;
	const auto fileA = write("a.owlmat", a);
	std::ignore = write("b.owlmat", b);
	MaterialLibrary library;
	EXPECT_EQ(library.get(fileA), nullptr);
}

TEST_F(MaterialTest, invalidatingAParentDropsItsInstances) {
	const auto parent = write("gold.owlmat", gold());
	MaterialAsset rough;
	rough.parent = parent;
	rough.overrides = 0;
	const auto child = write("rough_gold.owlmat", rough);
	MaterialLibrary library;
	const auto before = library.get(child);
	ASSERT_NE(before, nullptr);
	const auto generation = library.getGeneration();
	auto changed = gold();
	changed.values.metallic = 0.f;
	std::ignore = write("gold.owlmat", changed);
	EXPECT_EQ(library.invalidate(parent), 2u);
	EXPECT_GT(library.getGeneration(), generation);
	const auto after = library.get(child);
	ASSERT_NE(after, nullptr);
	EXPECT_FLOAT_EQ(after->getParameters().metallic, 0.f);
	EXPECT_EQ(library.invalidate(m_dir / "unrelated.owlmat"), 0u);
}

TEST_F(MaterialTest, aMaterialWithOverridesKeepsTheOtherValues) {
	const Material base{gold().values};
	MaterialOverrides overrides;
	overrides.values.alphaMode = AlphaMode::Blend;
	overrides.set(MaterialField::AlphaMode);
	const auto blended = base.withOverrides(overrides);
	EXPECT_TRUE(blended.isBlended());
	EXPECT_FALSE(base.isBlended());
	EXPECT_FLOAT_EQ(blended.getParameters().roughness, 0.25f);
}

TEST_F(MaterialTest, separateMapsReadTheirOwnChannel) {
	MaterialAsset asset;
	asset.values.maps[static_cast<size_t>(MaterialMap::Metallic)] = "nam:textures/metal.png";
	asset.values.maps[static_cast<size_t>(MaterialMap::Roughness)] = "nam:textures/rough.png";
	asset.values.metallicChannel = TextureChannel::R;
	asset.values.roughnessChannel = TextureChannel::A;
	const auto read = MaterialAsset::fromYaml(asset.toYaml());
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->values.maps[static_cast<size_t>(MaterialMap::Metallic)], "nam:textures/metal.png");
	EXPECT_EQ(read->values.maps[static_cast<size_t>(MaterialMap::Roughness)], "nam:textures/rough.png");
	EXPECT_EQ(read->values.metallicChannel, TextureChannel::R);
	EXPECT_EQ(read->values.roughnessChannel, TextureChannel::A);
	EXPECT_EQ(read->values.occlusionChannel, TextureChannel::R);
}

TEST_F(MaterialTest, theDefaultChannelsMatchAPackedArmImage) {
	const MaterialParameters values;
	EXPECT_EQ(values.occlusionChannel, TextureChannel::R);
	EXPECT_EQ(values.roughnessChannel, TextureChannel::G);
	EXPECT_EQ(values.metallicChannel, TextureChannel::B);
	EXPECT_EQ(mapField(MaterialMap::Roughness), MaterialField::RoughnessMap);
}

TEST_F(MaterialTest, differingFieldsNamesEveryChangedField) {
	const MaterialParameters defaults;
	EXPECT_EQ(defaults.differingFields(defaults), 0u);
	auto values = gold().values;
	EXPECT_EQ(values.differingFields(values), 0u);
	const auto fields = values.differingFields(defaults);
	EXPECT_NE(fields & static_cast<uint32_t>(MaterialField::Metallic), 0u);
	EXPECT_NE(fields & static_cast<uint32_t>(MaterialField::NormalMap), 0u);
	EXPECT_EQ(fields & static_cast<uint32_t>(MaterialField::SheenColor), 0u);
	values.copyFields(defaults, fields);
	EXPECT_EQ(values.differingFields(defaults), 0u);
}
