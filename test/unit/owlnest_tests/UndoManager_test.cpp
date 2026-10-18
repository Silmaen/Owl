/**
 * @file UndoManager_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "UndoManager.h"
#include "testHelper.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>

using namespace owl;
using namespace owl::nest;

namespace {

// Minimal undo target: a single integer.
struct Counter {
	// Current value.
	int value = 0;
};

// Command adding a delta to the counter; mergeable when `mergeKey` is non-zero.
class AddCommand final : public UndoCommand<Counter> {
public:
	AddCommand(const AddCommand&) = delete;

	AddCommand(AddCommand&&) = delete;

	auto operator=(const AddCommand&) -> AddCommand& = delete;

	auto operator=(AddCommand&&) -> AddCommand& = delete;

	explicit AddCommand(const int iDelta, const size_t iMergeKey = 0, const uint64_t iSelect = 0)
		: m_delta{iDelta}, m_mergeKey{iMergeKey} {
		m_selectAfterUndo = core::UUID{iSelect};
		m_selectAfterRedo = core::UUID{iSelect + 1};
	}

	~AddCommand() override = default;

	void undo(Counter& ioTarget) override { ioTarget.value -= m_delta; }

	void redo(Counter& ioTarget) override { ioTarget.value += m_delta; }

	[[nodiscard]] auto description() const -> std::string override { return std::format("Add {}", m_delta); }

	[[nodiscard]] auto mergeWith(const UndoCommand& iOther) -> bool override {
		const auto* other = dynamic_cast<const AddCommand*>(&iOther);
		if (other == nullptr)
			return false;
		m_delta += other->m_delta;
		m_timestamp = other->m_timestamp;
		return true;
	}

	[[nodiscard]] auto typeId() const -> size_t override { return m_mergeKey; }

private:
	int m_delta;
	size_t m_mergeKey;
};

// Apply a command to the target then hand it to the manager, as the editor panels do.
void pushApplied(UndoManager<Counter>& ioManager, Counter& ioTarget, uniq<AddCommand> iCommand) {
	iCommand->redo(ioTarget);
	ioManager.push(std::move(iCommand));
}

}// namespace

TEST(UndoManager, StartsEmptyAndClean) {
	const UndoManager<Counter> manager;
	EXPECT_FALSE(manager.canUndo());
	EXPECT_FALSE(manager.canRedo());
	EXPECT_FALSE(manager.isDirty());
	EXPECT_TRUE(manager.undoDescription().empty());
	EXPECT_TRUE(manager.redoDescription().empty());
	EXPECT_EQ(manager.lastSelectionHint(), core::UUID{0});
}

TEST(UndoManager, MarkUnsavedMakesTheStateDirtyUntilTheNextSave) {
	UndoManager<Counter> manager;
	manager.markUnsaved();
	EXPECT_TRUE(manager.isDirty());
	manager.markSaved();
	EXPECT_FALSE(manager.isDirty());
}

TEST(UndoManager, ExecuteAppliesAndRecords) {
	UndoManager<Counter> manager;
	Counter target;
	manager.execute(mkUniq<AddCommand>(3, 0, 10), target);
	EXPECT_EQ(target.value, 3);
	EXPECT_TRUE(manager.canUndo());
	EXPECT_FALSE(manager.canRedo());
	EXPECT_EQ(manager.undoDescription(), "Add 3");
	EXPECT_EQ(manager.lastSelectionHint(), core::UUID{11});
}

TEST(UndoManager, UndoRedoRoundTrip) {
	UndoManager<Counter> manager;
	Counter target;
	manager.execute(mkUniq<AddCommand>(2, 0, 20), target);
	manager.execute(mkUniq<AddCommand>(5), target);
	EXPECT_EQ(target.value, 7);

	manager.undo(target);
	EXPECT_EQ(target.value, 2);
	EXPECT_EQ(manager.redoDescription(), "Add 5");
	manager.undo(target);
	EXPECT_EQ(target.value, 0);
	EXPECT_EQ(manager.lastSelectionHint(), core::UUID{20});
	EXPECT_FALSE(manager.canUndo());

	manager.redo(target);
	EXPECT_EQ(manager.lastSelectionHint(), core::UUID{21});
	manager.redo(target);
	EXPECT_EQ(target.value, 7);
	EXPECT_FALSE(manager.canRedo());
}

TEST(UndoManager, UndoRedoOnEmptyStacksAreNoOps) {
	UndoManager<Counter> manager;
	Counter target{.value = 4};
	manager.undo(target);
	manager.redo(target);
	EXPECT_EQ(target.value, 4);
}

TEST(UndoManager, PushClearsRedoStack) {
	UndoManager<Counter> manager;
	Counter target;
	manager.execute(mkUniq<AddCommand>(1), target);
	manager.undo(target);
	ASSERT_TRUE(manager.canRedo());
	manager.execute(mkUniq<AddCommand>(4), target);
	EXPECT_FALSE(manager.canRedo());
	EXPECT_EQ(target.value, 4);
}

TEST(UndoManager, MergesSameTypeWithinWindow) {
	UndoManager<Counter> manager;
	Counter target;
	pushApplied(manager, target, mkUniq<AddCommand>(1, 42));
	pushApplied(manager, target, mkUniq<AddCommand>(2, 42));
	pushApplied(manager, target, mkUniq<AddCommand>(3, 42));
	EXPECT_EQ(target.value, 6);
	EXPECT_EQ(manager.undoDescription(), "Add 6");
	manager.undo(target);
	EXPECT_EQ(target.value, 0);
	EXPECT_FALSE(manager.canUndo());
}

TEST(UndoManager, DoesNotMergeAfterWindow) {
	UndoManager<Counter> manager;
	Counter target;
	pushApplied(manager, target, mkUniq<AddCommand>(1, 42));
	auto late = mkUniq<AddCommand>(2, 42);
	late->m_timestamp += std::chrono::milliseconds{1500};
	pushApplied(manager, target, std::move(late));
	manager.undo(target);
	EXPECT_EQ(target.value, 1);
	EXPECT_TRUE(manager.canUndo());
}

TEST(UndoManager, MergesAtWindowBoundary) {
	UndoManager<Counter> manager;
	Counter target;
	pushApplied(manager, target, mkUniq<AddCommand>(1, 42));
	auto edge = mkUniq<AddCommand>(2, 42);
	edge->m_timestamp += std::chrono::milliseconds{999};
	pushApplied(manager, target, std::move(edge));
	manager.undo(target);
	EXPECT_EQ(target.value, 0);
}

TEST(UndoManager, DoesNotMergeDifferentTypesOrTypeZero) {
	UndoManager<Counter> manager;
	Counter target;
	pushApplied(manager, target, mkUniq<AddCommand>(1, 42));
	pushApplied(manager, target, mkUniq<AddCommand>(2, 43));
	pushApplied(manager, target, mkUniq<AddCommand>(3));
	pushApplied(manager, target, mkUniq<AddCommand>(4));
	manager.undo(target);
	EXPECT_EQ(target.value, 6);
	manager.undo(target);
	EXPECT_EQ(target.value, 3);
	manager.undo(target);
	EXPECT_EQ(target.value, 1);
}

TEST(UndoManager, MergeCanBeDisabled) {
	UndoManager<Counter> manager;
	manager.setMergeEnabled(false);
	Counter target;
	pushApplied(manager, target, mkUniq<AddCommand>(1, 42));
	pushApplied(manager, target, mkUniq<AddCommand>(2, 42));
	manager.undo(target);
	EXPECT_EQ(target.value, 1);
}

TEST(UndoManager, MaxDepthDropsOldest) {
	UndoManager<Counter> manager;
	manager.setMaxDepth(3);
	Counter target;
	for (int i = 1; i <= 5; ++i) manager.execute(mkUniq<AddCommand>(i), target);
	EXPECT_EQ(target.value, 15);
	int undone = 0;
	while (manager.canUndo()) {
		manager.undo(target);
		++undone;
	}
	EXPECT_EQ(undone, 3);
	EXPECT_EQ(target.value, 3);
}

TEST(UndoManager, DefaultMaxDepthIsHundred) {
	UndoManager<Counter> manager;
	Counter target;
	for (int i = 0; i < 120; ++i) manager.execute(mkUniq<AddCommand>(1), target);
	int undone = 0;
	while (manager.canUndo()) {
		manager.undo(target);
		++undone;
	}
	EXPECT_EQ(undone, 100);
	EXPECT_EQ(target.value, 20);
}

TEST(UndoManager, DirtyFollowsSavePoint) {
	UndoManager<Counter> manager;
	Counter target;
	manager.execute(mkUniq<AddCommand>(1), target);
	EXPECT_TRUE(manager.isDirty());
	manager.markSaved();
	EXPECT_FALSE(manager.isDirty());
	manager.undo(target);
	EXPECT_TRUE(manager.isDirty());
	manager.redo(target);
	EXPECT_FALSE(manager.isDirty());
	manager.execute(mkUniq<AddCommand>(2), target);
	EXPECT_TRUE(manager.isDirty());
	manager.undo(target);
	EXPECT_FALSE(manager.isDirty());
}

TEST(UndoManager, DirtyAfterEvictionBackToSavePoint) {
	UndoManager<Counter> manager;
	manager.setMaxDepth(2);
	Counter target;
	manager.execute(mkUniq<AddCommand>(1), target);
	manager.markSaved();
	manager.execute(mkUniq<AddCommand>(2), target);
	manager.execute(mkUniq<AddCommand>(3), target);
	EXPECT_TRUE(manager.isDirty());
	manager.undo(target);
	manager.undo(target);
	EXPECT_EQ(target.value, 1);
	EXPECT_FALSE(manager.isDirty());
}

TEST(UndoManager, ClearResetsHistoryAndDirty) {
	UndoManager<Counter> manager;
	Counter target;
	manager.execute(mkUniq<AddCommand>(1, 0, 5), target);
	manager.execute(mkUniq<AddCommand>(1), target);
	manager.undo(target);
	manager.clear();
	EXPECT_FALSE(manager.canUndo());
	EXPECT_FALSE(manager.canRedo());
	EXPECT_FALSE(manager.isDirty());
	EXPECT_EQ(manager.lastSelectionHint(), core::UUID{0});
}

TEST(UndoManager, DirtyAfterSaveUndoEdit) {
	UndoManager<Counter> manager;
	manager.setMergeEnabled(false);
	Counter target;
	manager.execute(mkUniq<AddCommand>(1), target);
	manager.markSaved();
	manager.undo(target);
	manager.execute(mkUniq<AddCommand>(42), target);
	EXPECT_TRUE(manager.isDirty());
}

TEST(UndoManager, DirtyAfterMergeIntoSavedCommand) {
	UndoManager<Counter> manager;
	Counter target;
	pushApplied(manager, target, mkUniq<AddCommand>(1, 42));
	manager.markSaved();
	pushApplied(manager, target, mkUniq<AddCommand>(2, 42));
	EXPECT_TRUE(manager.isDirty());
	manager.undo(target);
	EXPECT_TRUE(manager.isDirty());
}

TEST(UndoManager, DirtyWhenSavePointDroppedFromRedo) {
	UndoManager<Counter> manager;
	manager.setMergeEnabled(false);
	Counter target;
	manager.execute(mkUniq<AddCommand>(1), target);
	manager.execute(mkUniq<AddCommand>(2), target);
	manager.markSaved();
	manager.undo(target);
	manager.undo(target);
	manager.execute(mkUniq<AddCommand>(5), target);
	manager.execute(mkUniq<AddCommand>(7), target);
	EXPECT_TRUE(manager.isDirty());
}

TEST(UndoManager, ClearAfterEditsIsClean) {
	UndoManager<Counter> manager;
	Counter target;
	manager.execute(mkUniq<AddCommand>(1), target);
	manager.markSaved();
	manager.execute(mkUniq<AddCommand>(2), target);
	ASSERT_TRUE(manager.isDirty());
	manager.clear();
	EXPECT_FALSE(manager.isDirty());
	manager.execute(mkUniq<AddCommand>(3), target);
	EXPECT_TRUE(manager.isDirty());
	manager.undo(target);
	EXPECT_FALSE(manager.isDirty());
}

TEST(UndoManager, DirtyAfterRedoPastSavePoint) {
	UndoManager<Counter> manager;
	manager.setMergeEnabled(false);
	Counter target;
	manager.execute(mkUniq<AddCommand>(1), target);
	manager.execute(mkUniq<AddCommand>(2), target);
	manager.undo(target);
	manager.markSaved();
	manager.redo(target);
	EXPECT_TRUE(manager.isDirty());
	manager.undo(target);
	EXPECT_FALSE(manager.isDirty());
	manager.undo(target);
	EXPECT_TRUE(manager.isDirty());
}
