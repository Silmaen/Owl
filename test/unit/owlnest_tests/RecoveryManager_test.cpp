/**
 * @file RecoveryManager_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "RecoveryManager.h"
#include "document/DocumentManager.h"
#include "testHelper.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>

using namespace owl;
using namespace owl::nest;

namespace {

// Document whose content is a plain string.
class FakeDocument final : public Document {
public:
	FakeDocument(std::filesystem::path iPath, std::string iContent, const bool iDirty)
		: m_path{std::move(iPath)}, m_content{std::move(iContent)}, m_dirty{iDirty} {}

	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Code; }
	[[nodiscard]] auto title() const -> std::string override { return m_path.filename().string(); }
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }
	[[nodiscard]] auto isDirty() const -> bool override { return m_dirty; }
	void onAttach(EditorLayer* /*iEditor*/) override {}
	void onDetach() override {}
	void onUpdate(const core::Timestep& /*iTimeStep*/) override {}
	void onEvent(event::Event& /*ioEvent*/) override {}
	void onImGuiRender() override {}
	auto save() -> bool override { return true; }
	auto saveAs(const std::filesystem::path& /*iPath*/) -> bool override { return true; }
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_undo; }
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_undo; }
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override { return m_content; }
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override {
		m_content = iSnapshot;
		m_dirty = true;
		return true;
	}

	void setDirty(const bool iDirty) { m_dirty = iDirty; }
	void setContent(const std::string& iContent) { m_content = iContent; }

private:
	std::filesystem::path m_path;
	std::string m_content;
	bool m_dirty;
	SceneUndoManager m_undo;
};

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class RecoveryManagerTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_recovery_manager_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root / "project");
		m_recovery.setDirectory(RecoveryManager::directoryFor(m_root, m_root / "project"));
	}

	void TearDown() override {
		m_documents.clear();
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	auto addDocument(const std::string& iName, const std::string& iContent, const bool iDirty) -> FakeDocument* {
		return static_cast<FakeDocument*>(
				m_documents.add(mkUniq<FakeDocument>(m_root / "project" / iName, iContent, iDirty)));
	}

	std::filesystem::path m_root;
	DocumentManager m_documents;
	RecoveryManager m_recovery;
};
OWL_DIAG_POP

}// namespace

TEST_F(RecoveryManagerTest, AutosaveKeepsOnlyDirtyDocuments) {
	addDocument("a.lua", "print('a')", true);
	addDocument("b.lua", "print('b')", false);
	EXPECT_EQ(m_recovery.autosave(m_documents), 1u);
	const auto pending = m_recovery.getPendingEntries();
	ASSERT_EQ(pending.size(), 1u);
	EXPECT_EQ(pending.front().type, DocumentType::Code);
	EXPECT_EQ(pending.front().title, "a.lua");
	EXPECT_EQ(pending.front().originalPath, m_root / "project" / "a.lua");
	EXPECT_FALSE(pending.front().savedAt.empty());
	EXPECT_EQ(m_recovery.readSnapshot(pending.front()), "print('a')");
}

TEST_F(RecoveryManagerTest, UnchangedSnapshotIsNotRewritten) {
	auto* doc = addDocument("a.lua", "v1", true);
	EXPECT_EQ(m_recovery.autosave(m_documents), 1u);
	EXPECT_EQ(m_recovery.autosave(m_documents), 0u);
	doc->setContent("v2");
	EXPECT_EQ(m_recovery.autosave(m_documents), 1u);
	EXPECT_EQ(m_recovery.readSnapshot(m_recovery.getPendingEntries().front()), "v2");
}

TEST_F(RecoveryManagerTest, SavedDocumentLeavesNothingToRecover) {
	auto* doc = addDocument("a.lua", "v1", true);
	m_recovery.autosave(m_documents);
	doc->setDirty(false);
	m_recovery.autosave(m_documents);
	EXPECT_TRUE(m_recovery.getPendingEntries().empty());
	EXPECT_FALSE(exists(m_recovery.getDirectory() / RecoveryManager::g_manifestName));
}

TEST_F(RecoveryManagerTest, SnapshotOlderThanItsFileIsSkipped) {
	const auto original = m_root / "project" / "a.lua";
	std::ofstream(original) << "on disk";
	addDocument("a.lua", "autosaved", true);
	m_recovery.autosave(m_documents);
	ASSERT_EQ(m_recovery.getPendingEntries().size(), 1u);
	std::filesystem::last_write_time(original, std::filesystem::file_time_type::clock::now() + std::chrono::hours{1});
	EXPECT_TRUE(m_recovery.getPendingEntries().empty());
}

TEST_F(RecoveryManagerTest, NextSessionFindsTheEntriesAndRestoresThem) {
	addDocument("a.lua", "lost work", true);
	m_recovery.autosave(m_documents);
	RecoveryManager next;
	next.setDirectory(RecoveryManager::directoryFor(m_root, m_root / "project"));
	const auto pending = next.getPendingEntries();
	ASSERT_EQ(pending.size(), 1u);
	FakeDocument reopened(pending.front().originalPath, "on disk", false);
	const auto snapshot = next.readSnapshot(pending.front());
	ASSERT_TRUE(snapshot.has_value());
	EXPECT_TRUE(reopened.restoreRecoverySnapshot(*snapshot));
	EXPECT_EQ(reopened.recoverySnapshot(), "lost work");
	EXPECT_TRUE(reopened.isDirty());
	next.clear();
	EXPECT_TRUE(next.getPendingEntries().empty());
}

TEST_F(RecoveryManagerTest, KeepAsideMovesTheSnapshots) {
	addDocument("a.lua", "v1", true);
	m_recovery.autosave(m_documents);
	const auto kept = m_recovery.keepAside();
	EXPECT_FALSE(kept.empty());
	EXPECT_TRUE(exists(kept / RecoveryManager::g_manifestName));
	EXPECT_TRUE(m_recovery.getPendingEntries().empty());
}

TEST_F(RecoveryManagerTest, ClockHonoursIntervalSuspensionAndDirectory) {
	m_recovery.setInterval(10.f);
	EXPECT_FALSE(m_recovery.onUpdate(6.f));
	EXPECT_TRUE(m_recovery.onUpdate(6.f));
	EXPECT_FALSE(m_recovery.onUpdate(6.f));
	m_recovery.setSuspended(true);
	EXPECT_TRUE(m_recovery.isSuspended());
	EXPECT_FALSE(m_recovery.onUpdate(60.f));
	addDocument("a.lua", "v1", true);
	EXPECT_EQ(m_recovery.autosave(m_documents), 0u);
	m_recovery.setSuspended(false);
	m_recovery.setInterval(0.f);
	EXPECT_FALSE(m_recovery.onUpdate(60.f));
	m_recovery.setDirectory({});
	m_recovery.setInterval(1.f);
	EXPECT_FALSE(m_recovery.onUpdate(60.f));
	EXPECT_EQ(m_recovery.autosave(m_documents), 0u);
}

TEST(RecoveryManagerDirectory, IsStablePerProjectAndDistinct) {
	const std::filesystem::path root{"/tmp/owl"};
	const auto first = RecoveryManager::directoryFor(root, "/home/me/games/demo");
	EXPECT_EQ(first, RecoveryManager::directoryFor(root, "/home/me/games/demo/"));
	EXPECT_NE(first, RecoveryManager::directoryFor(root, "/home/me/other/demo"));
	EXPECT_EQ(first.parent_path(), root / "OwlNest_recovery");
	EXPECT_TRUE(first.filename().string().starts_with("demo-"));
	EXPECT_EQ(RecoveryManager::format().name, "Recovery");
}
