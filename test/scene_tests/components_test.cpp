#include "testHelper.h"

#include <scene/component/components.h>

TEST(SceneComponent, key) {
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
