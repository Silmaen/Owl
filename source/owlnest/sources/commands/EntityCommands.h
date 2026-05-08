/**
 * @file EntityCommands.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../EntitySnapshot.h"
#include "../UndoCommand.h"

namespace owl::nest::commands {
/**
 * @brief
 *  Command for creating a new entity.
 *
 * Redo creates the entity; undo destroys it.
 */
class CreateEntityCommand final : public SceneUndoCommand {
public:
	CreateEntityCommand(const CreateEntityCommand&) = delete;

	CreateEntityCommand(CreateEntityCommand&&) = default;

	auto operator=(const CreateEntityCommand&) -> CreateEntityCommand& = delete;

	auto operator=(CreateEntityCommand&&) -> CreateEntityCommand& = default;

	/**
	 * @brief
	 *  Construct after an entity has already been created.
	 * @param[in] iEntity The just-created entity.
	 */
	explicit CreateEntityCommand(const scene::Entity& iEntity);

	/**
	 * @brief
	 *  Destructor.
	 */
	~CreateEntityCommand() override;

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void redo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override;

private:
	/// Captured serialized state of the entity (so undo can recreate it identically).
	EntitySnapshot m_snapshot;
	/// Display name used in the Edit-menu undo/redo entry.
	std::string m_name;
};
/**
 * @brief
 *  Command for deleting a single entity (children reparented to grandparent).
 *
 * Redo destroys the entity; undo restores it from snapshot.
 */
class DeleteEntityCommand final : public SceneUndoCommand {
public:
	DeleteEntityCommand(const DeleteEntityCommand&) = delete;

	DeleteEntityCommand(DeleteEntityCommand&&) = default;

	auto operator=(const DeleteEntityCommand&) -> DeleteEntityCommand& = delete;

	auto operator=(DeleteEntityCommand&&) -> DeleteEntityCommand& = default;

	/**
	 * @brief
	 *  Construct before an entity is destroyed.
	 * @param[in] iEntity The entity about to be destroyed.
	 */
	explicit DeleteEntityCommand(const scene::Entity& iEntity);

	/**
	 * @brief
	 *  Destructor.
	 */
	~DeleteEntityCommand() override;

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void redo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override;

private:
	/// Captured serialized state of the deleted entity (used to recreate on undo).
	EntitySnapshot m_snapshot;
	/// Display name used in the Edit-menu undo/redo entry.
	std::string m_name;
	/// Parent UUID (for re-parenting on undo).
	core::UUID m_parentUuid{0};
	/// Children UUIDs (for re-parenting on undo).
	std::vector<core::UUID> m_childrenUuids;
};
/**
 * @brief
 *  Command for deleting an entity and all its descendants.
 *
 * Redo destroys the subtree; undo restores the entire subtree from snapshot.
 */
class DeleteSubtreeCommand final : public SceneUndoCommand {
public:
	DeleteSubtreeCommand(const DeleteSubtreeCommand&) = delete;

	DeleteSubtreeCommand(DeleteSubtreeCommand&&) = default;

	auto operator=(const DeleteSubtreeCommand&) -> DeleteSubtreeCommand& = delete;

	auto operator=(DeleteSubtreeCommand&&) -> DeleteSubtreeCommand& = default;

	/**
	 * @brief
	 *  Construct before a subtree is destroyed.
	 * @param[in] iEntity The root entity of the subtree.
	 * @param[in] iScene The scene.
	 */
	DeleteSubtreeCommand(const scene::Entity& iEntity, const scene::Scene& iScene);

	/**
	 * @brief
	 *  Destructor.
	 */
	~DeleteSubtreeCommand() override;

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void redo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override;

private:
	/// Captured serialized state of the subtree (used to recreate the whole branch on undo).
	SubtreeSnapshot m_snapshot;
	/// Display name used in the Edit-menu undo/redo entry.
	std::string m_name;
	/// Parent UUID of the subtree root (for re-parenting on undo).
	core::UUID m_parentUuid{0};
};
/**
 * @brief
 *  Command for duplicating a single entity.
 *
 * Redo creates the duplicate; undo destroys it.
 */
class DuplicateEntityCommand final : public SceneUndoCommand {
public:
	DuplicateEntityCommand(const DuplicateEntityCommand&) = delete;

	DuplicateEntityCommand(DuplicateEntityCommand&&) = default;

	auto operator=(const DuplicateEntityCommand&) -> DuplicateEntityCommand& = delete;

	auto operator=(DuplicateEntityCommand&&) -> DuplicateEntityCommand& = default;

	/**
	 * @brief
	 *  Construct after an entity has been duplicated.
	 * @param[in] iOriginal The original entity.
	 * @param[in] iDuplicate The newly duplicated entity.
	 */
	DuplicateEntityCommand(const scene::Entity& iOriginal, const scene::Entity& iDuplicate);

	/**
	 * @brief
	 *  Destructor.
	 */
	~DuplicateEntityCommand() override;

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void redo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override;

private:
	/// Captured state of the duplicate; redo recreates it from this snapshot.
	EntitySnapshot m_duplicateSnapshot;
	/// Display name used in the Edit-menu undo/redo entry.
	std::string m_name;
};
/**
 * @brief
 *  Command for duplicating an entity subtree.
 *
 * Redo creates the duplicate subtree; undo destroys it.
 */
class DuplicateSubtreeCommand final : public SceneUndoCommand {
public:
	DuplicateSubtreeCommand(const DuplicateSubtreeCommand&) = delete;

	DuplicateSubtreeCommand(DuplicateSubtreeCommand&&) = default;

	auto operator=(const DuplicateSubtreeCommand&) -> DuplicateSubtreeCommand& = delete;

	auto operator=(DuplicateSubtreeCommand&&) -> DuplicateSubtreeCommand& = default;

	/**
	 * @brief
	 *  Construct after a subtree has been duplicated.
	 * @param[in] iOriginal The original root entity.
	 * @param[in] iDuplicateRoot The duplicated root entity.
	 * @param[in] iScene The scene (for capturing the subtree snapshot).
	 */
	DuplicateSubtreeCommand(const scene::Entity& iOriginal, const scene::Entity& iDuplicateRoot,
							const scene::Scene& iScene);

	/**
	 * @brief
	 *  Destructor.
	 */
	~DuplicateSubtreeCommand() override;

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void redo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override;

private:
	/// Captured state of the duplicate subtree; redo recreates it from this snapshot.
	SubtreeSnapshot m_duplicateSnapshot;
	/// Display name used in the Edit-menu undo/redo entry.
	std::string m_name;
};

}// namespace owl::nest::commands
