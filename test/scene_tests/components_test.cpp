#include "testHelper.h"

#include <core/Application.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

TEST(SceneComponent, key) {
	EXPECT_EQ(owl::scene::component::AnimatedSpriteRenderer::key(), "AnimatedSpriteRenderer");
	EXPECT_EQ(owl::scene::component::Camera::key(), "Camera");
	EXPECT_EQ(owl::scene::component::CircleRenderer::key(), "CircleRenderer");
	EXPECT_EQ(owl::scene::component::EntityLink::key(), "EntityLink");
	EXPECT_EQ(owl::scene::component::PhysicBody::key(), "PhysicBody");
	EXPECT_EQ(owl::scene::component::Player::key(), "Player");
	EXPECT_EQ(owl::scene::component::SpriteRenderer::key(), "SpriteRenderer");
	EXPECT_EQ(owl::scene::component::Tag::key(), "Tag");
	EXPECT_EQ(owl::scene::component::Text::key(), "TextRenderer");
	EXPECT_EQ(owl::scene::component::Transform::key(), "Transform");
	EXPECT_EQ(owl::scene::component::Trigger::key(), "Trigger");
	EXPECT_EQ(owl::scene::component::Visibility::key(), "Visibility");
}

TEST(SceneComponent, name) {
	EXPECT_EQ(owl::scene::component::AnimatedSpriteRenderer::name(), "Animated Sprite");
	EXPECT_EQ(owl::scene::component::Camera::name(), "Camera");
	EXPECT_EQ(owl::scene::component::CircleRenderer::name(), "Circle Renderer");
	EXPECT_EQ(owl::scene::component::EntityLink::name(), "Entity Link");
	EXPECT_EQ(owl::scene::component::PhysicBody::name(), "Physical body");
	EXPECT_EQ(owl::scene::component::Player::name(), "Player");
	EXPECT_EQ(owl::scene::component::SpriteRenderer::name(), "Sprite Renderer");
	EXPECT_EQ(owl::scene::component::Text::name(), "Text Renderer");
	EXPECT_EQ(owl::scene::component::Transform::name(), "Transform");
	EXPECT_EQ(owl::scene::component::Trigger::name(), "Trigger");
	EXPECT_EQ(owl::scene::component::Visibility::name(), "Visibility");
}

TEST(SceneComponent, TextSerializeDeserializeRoundTrip) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	auto app = owl::mkShared<owl::core::Application>(owl::core::AppParams{.args = nullptr,
																		   .frameLogFrequency = 0,
																		   .name = "textRoundTrip",
																		   .assetsPattern = "",
																		   .icon = "",
																		   .width = 0,
																		   .height = 0,
																		   .argCount = 0,
																		   .renderer = owl::renderer::RenderAPI::Type::Null,
																		   .hasGui = false,
																		   .useDebugging = false,
																		   .isDummy = true});
	const auto sc = owl::mkShared<owl::scene::Scene>();
	auto ent = sc->createEntityWithUUID(100, "TextEntity");
	auto& text = ent.addOrReplaceComponent<owl::scene::component::Text>();
	text.text = "Hello World";
	text.color = {0.5f, 0.25f, 0.75f, 1.0f};
	text.kerning = 1.5f;
	text.lineSpacing = 2.0f;
	text.font = app->getFontLibrary().getDefaultFont();

	const owl::scene::SceneSerializer saver(sc);
	const auto fs = std::filesystem::temp_directory_path() / "tempTextRoundTrip.yml";
	saver.serialize(fs);
	ASSERT_TRUE(exists(fs));

	const auto sc2 = owl::mkShared<owl::scene::Scene>();
	const owl::scene::SceneSerializer loader(sc2);
	EXPECT_TRUE(loader.deserialize(fs));

	const auto entities = sc2->getAllEntities();
	ASSERT_EQ(entities.size(), 1u);
	const auto& text2 = entities[0].getComponent<owl::scene::component::Text>();
	EXPECT_EQ(text2.text, "Hello World");
	EXPECT_NEAR(text2.color.x(), 0.5f, 0.01f);
	EXPECT_NEAR(text2.color.y(), 0.25f, 0.01f);
	EXPECT_NEAR(text2.color.z(), 0.75f, 0.01f);
	EXPECT_NEAR(text2.color.w(), 1.0f, 0.01f);
	EXPECT_NEAR(text2.kerning, 1.5f, 0.01f);
	EXPECT_NEAR(text2.lineSpacing, 2.0f, 0.01f);

	remove(fs);
	owl::core::Application::invalidate();
	app.reset();
	owl::core::Log::invalidate();
}

TEST(SceneComponent, TextSerializeWithFont) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	auto app = owl::mkShared<owl::core::Application>(owl::core::AppParams{.args = nullptr,
																		   .frameLogFrequency = 0,
																		   .name = "fontTextTest",
																		   .assetsPattern = "",
																		   .icon = "",
																		   .width = 0,
																		   .height = 0,
																		   .argCount = 0,
																		   .renderer = owl::renderer::RenderAPI::Type::Null,
																		   .hasGui = false,
																		   .useDebugging = false,
																		   .isDummy = true});

	auto& fontLibrary = app->getFontLibrary();
	const auto defaultFont = fontLibrary.getDefaultFont();
	ASSERT_NE(defaultFont, nullptr);

	// Create a scene with a Text component that has the default font.
	const auto sc = owl::mkShared<owl::scene::Scene>();
	auto ent = sc->createEntityWithUUID(101, "FontTextEntity");
	auto& text = ent.addOrReplaceComponent<owl::scene::component::Text>();
	text.text = "Font test";
	text.font = defaultFont;
	// Default font is default, so serialize should NOT emit font key.

	const owl::scene::SceneSerializer saver(sc);
	const auto fs = std::filesystem::temp_directory_path() / "tempTextFontComponent.yml";
	saver.serialize(fs);
	ASSERT_TRUE(exists(fs));

	// Deserialize — with Application active, font should be resolved.
	const auto sc2 = owl::mkShared<owl::scene::Scene>();
	const owl::scene::SceneSerializer loader(sc2);
	EXPECT_TRUE(loader.deserialize(fs));

	const auto entities = sc2->getAllEntities();
	ASSERT_EQ(entities.size(), 1u);
	const auto& text2 = entities[0].getComponent<owl::scene::component::Text>();
	EXPECT_EQ(text2.text, "Font test");
	// When no font key is serialized, deserialize uses default font.
	EXPECT_NE(text2.font, nullptr);
	EXPECT_TRUE(text2.font->isDefault());

	remove(fs);
	owl::core::Application::invalidate();
	app.reset();
	owl::core::Log::invalidate();
}

TEST(SceneComponent, TextEmptyString) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	auto app = owl::mkShared<owl::core::Application>(owl::core::AppParams{.args = nullptr,
																		   .frameLogFrequency = 0,
																		   .name = "textEmpty",
																		   .assetsPattern = "",
																		   .icon = "",
																		   .width = 0,
																		   .height = 0,
																		   .argCount = 0,
																		   .renderer = owl::renderer::RenderAPI::Type::Null,
																		   .hasGui = false,
																		   .useDebugging = false,
																		   .isDummy = true});
	const auto sc = owl::mkShared<owl::scene::Scene>();
	auto ent = sc->createEntityWithUUID(102, "EmptyTextEntity");
	auto& text = ent.addOrReplaceComponent<owl::scene::component::Text>();
	text.text = "";
	text.color = {1.0f, 1.0f, 1.0f, 1.0f};
	text.kerning = 0.0f;
	text.lineSpacing = 0.0f;

	const owl::scene::SceneSerializer saver(sc);
	const auto fs = std::filesystem::temp_directory_path() / "tempTextEmpty.yml";
	saver.serialize(fs);
	ASSERT_TRUE(exists(fs));

	const auto sc2 = owl::mkShared<owl::scene::Scene>();
	const owl::scene::SceneSerializer loader(sc2);
	EXPECT_TRUE(loader.deserialize(fs));

	const auto entities = sc2->getAllEntities();
	ASSERT_EQ(entities.size(), 1u);
	const auto& text2 = entities[0].getComponent<owl::scene::component::Text>();
	EXPECT_TRUE(text2.text.empty());
	EXPECT_NEAR(text2.kerning, 0.0f, 0.001f);
	EXPECT_NEAR(text2.lineSpacing, 0.0f, 0.001f);

	remove(fs);
	owl::core::Application::invalidate();
	app.reset();
	owl::core::Log::invalidate();
}
