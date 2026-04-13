/**
 * @file UndoManager.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "UndoManager.h"

namespace owl::nest {

void UndoManager::push(uniq<UndoCommand> iCommand) {
	// Try to merge with the top command
	if (m_mergeEnabled && !m_undoStack.empty()) {
		auto& top = m_undoStack.back();
		if (top->typeId() != 0 && top->typeId() == iCommand->typeId()) {
			const auto elapsed = iCommand->m_timestamp - top->m_timestamp;
			if (elapsed <= s_mergeTimeoutMs && top->mergeWith(*iCommand))
				return;// Merged: discard the new command
		}
	}
	// Clear redo stack on new command
	m_redoStack.clear();
	// Enforce max depth
	if (m_undoStack.size() >= m_maxDepth) {
		m_undoStack.erase(m_undoStack.begin());
		--m_savedIndex;
	}
	m_undoStack.push_back(std::move(iCommand));
}

void UndoManager::execute(uniq<UndoCommand> iCommand, scene::Scene& ioScene) {
	iCommand->redo(ioScene);
	m_lastSelectionHint = iCommand->m_selectAfterRedo;
	push(std::move(iCommand));
}

void UndoManager::undo(scene::Scene& ioScene) {
	if (m_undoStack.empty())
		return;
	auto command = std::move(m_undoStack.back());
	m_undoStack.pop_back();
	command->undo(ioScene);
	m_lastSelectionHint = command->m_selectAfterUndo;
	m_redoStack.push_back(std::move(command));
}

void UndoManager::redo(scene::Scene& ioScene) {
	if (m_redoStack.empty())
		return;
	auto command = std::move(m_redoStack.back());
	m_redoStack.pop_back();
	command->redo(ioScene);
	m_lastSelectionHint = command->m_selectAfterRedo;
	m_undoStack.push_back(std::move(command));
}

auto UndoManager::canUndo() const -> bool { return !m_undoStack.empty(); }

auto UndoManager::canRedo() const -> bool { return !m_redoStack.empty(); }

auto UndoManager::undoDescription() const -> std::string {
	if (m_undoStack.empty())
		return {};
	return m_undoStack.back()->description();
}

auto UndoManager::redoDescription() const -> std::string {
	if (m_redoStack.empty())
		return {};
	return m_redoStack.back()->description();
}

void UndoManager::clear() {
	m_undoStack.clear();
	m_redoStack.clear();
	m_savedIndex = 0;
	m_lastSelectionHint = core::UUID{0};
}

void UndoManager::markSaved() { m_savedIndex = static_cast<int64_t>(m_undoStack.size()); }

auto UndoManager::isDirty() const -> bool {
	return static_cast<int64_t>(m_undoStack.size()) != m_savedIndex;
}

}// namespace owl::nest
