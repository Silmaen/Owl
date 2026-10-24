/**
 * @file MarkdownPreview.cpp
 * @author Silmaen
 * @date 28/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "MarkdownPreview.h"

#include "LanguageDefinitions.h"
#include "external/imgui_text_edit.h"
#include "external/lunasvg_wrapper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <core/Macros.h>
#include <gui/IconFont.h>
#include <gui/Theme.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <platform/openExternalUrl.h>
#include <renderer/gpu/Texture.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace owl::nest::codeEditor {

namespace {
constexpr uint32_t kImageMaxSide = 2048;
constexpr std::string_view kBadgeScheme = "badge:";
constexpr float kBadgePadX = 7.0f;
constexpr float kBadgeTextScale = 0.85f;
constexpr float kZoomViewportRatio = 0.9f;
// Font scale, space above (in body lines) and rule thickness (0: none) of a heading level.
struct HeadingStyle {
	float scale;
	float spaceAbove;
	float rule;
};
constexpr float kInlineCodePadX = 3.0f;
constexpr float kImageHorizontalPadding = 16.0f;

// lunasvg draws `<text>` only with a registered font face (the mermaid diagrams are text-heavy SVG).
void registerSvgFonts() {
	static bool done = false;
	if (done || !app::Application::instanced())
		return;
	done = true;
	const auto find = [](const char* iRelative) -> std::string {
		for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
			if (const auto full = assetsPath / iRelative; exists(full))
				return full.string();
		}
		return {};
	};
	if (const auto regular = find("fonts/Inter-Regular.ttf"); !regular.empty()) {
		lunasvg_add_font_face_from_file("Inter", false, false, regular.c_str());
		lunasvg_add_font_face_from_file("", false, false, regular.c_str());
	}
	if (const auto bold = find("fonts/Inter-SemiBold.ttf"); !bold.empty())
		lunasvg_add_font_face_from_file("Inter", true, false, bold.c_str());
}

[[nodiscard]] auto isExternalUrl(const std::string& iHref) -> bool {
	return iHref.starts_with("http://") || iHref.starts_with("https://") || iHref.starts_with("mailto:");
}

[[nodiscard]] auto fenceToLanguage(std::string iLang) -> Language {
	std::ranges::transform(iLang, iLang.begin(),
						   [](const unsigned char iCh) -> char { return static_cast<char>(std::tolower(iCh)); });
	if (iLang == "lua")
		return Language::Lua;
	if (iLang == "python" || iLang == "py")
		return Language::Python;
	if (iLang == "c")
		return Language::C;
	if (iLang == "cpp" || iLang == "c++" || iLang == "cc" || iLang == "cxx")
		return Language::Cpp;
	if (iLang == "yaml" || iLang == "yml")
		return Language::Yaml;
	if (iLang == "json")
		return Language::Json;
	if (iLang == "xml" || iLang == "svg" || iLang == "html")
		return Language::Xml;
	if (iLang == "markdown" || iLang == "md")
		return Language::Markdown;
	if (iLang == "bash" || iLang == "sh" || iLang == "shell" || iLang == "zsh")
		return Language::Bash;
	return Language::PlainText;
}

[[nodiscard]] auto headingStyle(const uint8_t iLevel) -> HeadingStyle {
	switch (iLevel) {
		case 1:
			return {.scale = 1.75f, .spaceAbove = 1.1f, .rule = 2.0f};
		case 2:
			return {.scale = 1.4f, .spaceAbove = 1.0f, .rule = 1.0f};
		case 3:
			return {.scale = 1.2f, .spaceAbove = 0.75f, .rule = 0.0f};
		default:
			return {.scale = 1.05f, .spaceAbove = 0.5f, .rule = 0.0f};
	}
}

[[nodiscard]] auto headingColour(const gui::Theme& iTheme, const uint8_t iLevel) -> const math::vec4& {
	if (iLevel == 3)
		return iTheme.accent;
	if (iLevel <= 4)
		return iTheme.text;
	return iTheme.textMuted;
}

[[nodiscard]] auto linkColour(const gui::Theme& iTheme, const bool iExternal, const bool iHovered) -> ImVec4 {
	const auto& base = iExternal ? iTheme.info : iTheme.accent;
	return gui::vec(iHovered ? gui::Theme::mix(base, iTheme.text, 0.35f) : base);
}

// Colour of a `rrggbb` / `rgb` hex string; the muted text colour when malformed.
[[nodiscard]] auto badgeColour(const std::string& iHex) -> ImVec4 {
	std::string hex = iHex;
	if (hex.size() == 3)
		hex = {hex[0], hex[0], hex[1], hex[1], hex[2], hex[2]};
	uint32_t value = 0;
	if (const auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), value, 16);
		hex.size() != 6 || ec != std::errc{} || ptr != hex.data() + hex.size())
		return gui::vec(gui::UiLayer::getTheme().textMuted);
	const auto channel = [value](const int iShift) -> float {
		return static_cast<float>((value >> iShift) & 0xffu) / 255.0f;
	};
	return {channel(16), channel(8), channel(0), 1.0f};
}

// True when a table cell holds images and nothing but blanks.
[[nodiscard]] auto isImageCell(const std::vector<MdInline>& iSpans) -> bool {
	bool image = false;
	for (const auto& span: iSpans) {
		if (span.kind == InlineKind::ImageInline && !span.text.starts_with(kBadgeScheme))
			image = true;
		else if (span.kind != InlineKind::Text || !std::ranges::all_of(span.text, [](const unsigned char iCh) -> bool {
					 return std::isspace(iCh) != 0;
				 }))
			return false;
	}
	return image;
}

[[nodiscard]] auto countLines(const std::string& iText) -> int {
	if (iText.empty())
		return 1;
	int n = 1;
	for (const char c: iText)
		if (c == '\n')
			++n;
	return n;
}

// NOLINTBEGIN(misc-no-recursion)
void collectCodeBlocks(const std::vector<MdBlock>& iBlocks, size_t& ioCount) {
	for (const auto& block: iBlocks) {
		std::visit(
				[&]<typename T>(const T& iVal) -> void {
					if constexpr (std::is_same_v<T, MdCodeBlock>)
						++ioCount;
					else if constexpr (std::is_same_v<T, MdList>) {
						for (const auto& item: iVal.items) collectCodeBlocks(item, ioCount);
					} else if constexpr (std::is_same_v<T, MdBlockQuote>)
						collectCodeBlocks(iVal.body, ioCount);
				},
				block.data);
	}
}
// NOLINTEND(misc-no-recursion)

}// namespace

MarkdownPreview::MarkdownPreview() = default;

MarkdownPreview::~MarkdownPreview() = default;

void MarkdownPreview::rebuildCaches() {
	m_document.parse(m_renderedText);
	// One TextEditor per code block, in document traversal order.
	size_t codeCount = 0;
	collectCodeBlocks(m_document.blocks(), codeCount);
	m_codeEditors.clear();
	m_codeEditors.reserve(codeCount);
	// Walk again to populate each editor's content + language.
	std::vector<const MdCodeBlock*> codeBlocks;
	codeBlocks.reserve(codeCount);
	// NOLINTBEGIN(misc-no-recursion) — block tree recurses via lists/quotes.
	const auto collect = [&](const auto& iSelf, const std::vector<MdBlock>& iBlocks) -> void {
		for (const auto& block: iBlocks) {
			std::visit(
					[&]<typename T>(const T& iVal) -> void {
						if constexpr (std::is_same_v<T, MdCodeBlock>)
							codeBlocks.push_back(&iVal);
						else if constexpr (std::is_same_v<T, MdList>) {
							for (const auto& item: iVal.items) iSelf(iSelf, item);
						} else if constexpr (std::is_same_v<T, MdBlockQuote>)
							iSelf(iSelf, iVal.body);
					},
					block.data);
		}
	};
	collect(collect, m_document.blocks());
	// NOLINTEND(misc-no-recursion)
	for (const auto* cb: codeBlocks) {
		auto editor = mkUniq<TextEditor>();
		editor->SetText(cb->text);
		editor->SetReadOnlyEnabled(true);
		applyLanguage(*editor, fenceToLanguage(cb->lang));
		m_codeEditors.push_back(CodeEditorEntry{.editor = std::move(editor)});
	}
}

void MarkdownPreview::render(const ImVec2& iSize) {
	if (m_needsRebuild) {
		rebuildCaches();
		m_anchorY.clear();
		m_hoveredLink = -1;
		m_needsRebuild = false;
	}
	// Applied for a few frames: the scroll is clamped to the content size of the previous frame.
	if (m_pendingScrollY >= 0.0f) {
		ImGui::SetNextWindowScroll(ImVec2{-1.0f, m_pendingScrollY});
		m_scrollFrames = std::max(0, m_scrollFrames - 1);
		if (m_scrollFrames == 0)
			m_pendingScrollY = -1.0f;
	}
	ImGui::BeginChild("##md_preview", iSize, ImGuiChildFlags_Borders);
	m_linkCount = 0;
	m_nextHoveredLink = -1;
	m_firstBlock = true;
	if (!m_pendingAnchor.empty())
		m_anchorY.clear();
	if (m_renderedText.empty()) {
		ImGui::TextDisabled("(empty)");
	} else {
		size_t codeIndex = 0;
		for (const auto& block: m_document.blocks()) renderBlock(block, codeIndex, /*iCompact=*/false);
	}
	// A text set by a link of this frame has no anchors yet; they settle once its tables are sized, frames later.
	if (!m_pendingAnchor.empty() && !m_needsRebuild) {
		if (const auto found = m_anchorY.find(m_pendingAnchor); found != m_anchorY.end()) {
			ImGui::SetScrollY(std::max(0.0f, found->second - ImGui::GetStyle().WindowPadding.y));
		} else {
			OWL_CORE_WARN("MarkdownPreview: No heading with the anchor '{}'.", m_pendingAnchor)
			m_anchorFrames = 0;
		}
		m_anchorFrames = std::max(0, m_anchorFrames - 1);
		if (m_anchorFrames == 0)
			m_pendingAnchor.clear();
	}
	m_scrollY = ImGui::GetScrollY();
	m_hoveredLink = m_nextHoveredLink;
	ImGui::EndChild();
	if (m_zoomRequested) {
		ImGui::OpenPopup("##md_zoom");
		m_zoomRequested = false;
	}
	renderZoom();
}

// NOLINTBEGIN(misc-no-recursion) — block tree recurses via lists/quotes.
void MarkdownPreview::renderBlock(const MdBlock& iBlock, size_t& ioCodeIndex, const bool iCompact) {
	std::visit(
			[&]<typename T>(const T& iVal) -> void {
				if constexpr (std::is_same_v<T, MdHeading>) {
					renderHeading(iVal);
				} else if constexpr (std::is_same_v<T, MdParagraph>) {
					// A paragraph that contains only a single block-level image is rendered as an Image.
					if (iVal.spans.size() == 1 && iVal.spans.front().kind == InlineKind::ImageInline) {
						const auto& img = iVal.spans.front();
						renderImage(img.text, img.alt, img.width);
					} else {
						renderInlineSpans(iVal.spans);
					}
					if (!iCompact)
						ImGui::Spacing();
				} else if constexpr (std::is_same_v<T, MdCodeBlock>) {
					renderCodeBlock(iVal, ioCodeIndex);
					if (!iCompact)
						ImGui::Spacing();
				} else if constexpr (std::is_same_v<T, MdImage>) {
					renderImage(iVal.src, iVal.alt, 0);
					if (!iCompact)
						ImGui::Spacing();
				} else if constexpr (std::is_same_v<T, MdHRule>) {
					ImGui::Separator();
				} else if constexpr (std::is_same_v<T, MdTable>) {
					renderTable(iVal);
					if (!iCompact)
						ImGui::Spacing();
				} else if constexpr (std::is_same_v<T, MdList>) {
					renderList(iVal, ioCodeIndex);
				} else if constexpr (std::is_same_v<T, MdBlockQuote>) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
					ImGui::Indent();
					for (const auto& sub: iVal.body) renderBlock(sub, ioCodeIndex, iCompact);
					ImGui::Unindent();
					ImGui::PopStyleColor();
				}
			},
			iBlock.data);
	m_firstBlock = false;
}
// NOLINTEND(misc-no-recursion)

void MarkdownPreview::renderHeading(const MdHeading& iHeading) {
	const auto& theme = gui::UiLayer::getTheme();
	const auto style = headingStyle(iHeading.level);
	const float base = ImGui::GetStyle().FontSizeBase;
	if (!m_firstBlock)
		ImGui::Dummy(ImVec2{0.0f, ImGui::GetTextLineHeight() * style.spaceAbove});
	if (!iHeading.anchor.empty())
		m_anchorY.try_emplace(iHeading.anchor, ImGui::GetCursorPosY());
	gui::UiLayer::pushTextStyle(gui::TextStyle::Strong);
	ImGui::PushFont(nullptr, base * style.scale);
	ImGui::PushStyleColor(ImGuiCol_Text, gui::vec(headingColour(theme, iHeading.level)));
	renderInlineSpans(iHeading.spans);
	ImGui::PopStyleColor();
	ImGui::PopFont();
	gui::UiLayer::popTextStyle();
	if (style.rule > 0.0f) {
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const float right = origin.x + ImGui::GetContentRegionAvail().x;
		const float y = origin.y + 2.0f;
		const auto ruleColour = iHeading.level == 1 ? theme.accent : theme.line;
		ImGui::GetWindowDrawList()->AddLine(ImVec2{origin.x, y}, ImVec2{right, y},
											ImGui::GetColorU32(gui::vec(ruleColour)), style.rule);
		ImGui::Dummy(ImVec2{0.0f, 4.0f + style.rule});
	}
	ImGui::Spacing();
}

void MarkdownPreview::renderInlineSpans(const std::vector<MdInline>& iSpans) {
	const auto& theme = gui::UiLayer::getTheme();
	int strongDepth = 0;
	int emDepth = 0;
	int strikeDepth = 0;
	bool linkOpen = false;
	bool linkExternal = false;
	int linkId = -1;
	std::string linkHref;
	bool firstOnLine = true;
	// Whitespace between the previous span and the next one: a span glued to the previous one (`(`, `.`) keeps no gap.
	bool spaceBefore = false;
	int previousLinkId = -1;
	const auto pushStyle = [&]() -> void {
		if (linkOpen)
			ImGui::PushStyleColor(ImGuiCol_Text, linkColour(theme, linkExternal, linkId == m_hoveredLink));
		else if (strongDepth > 0)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
		else if (emDepth > 0)
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	};
	const auto popStyle = [&]() -> void {
		if (strongDepth > 0 || emDepth > 0 || linkOpen)
			ImGui::PopStyleColor();
	};

	const auto emitWord = [&](const std::string_view iWord, const bool iIsCode, const float iGap) -> void {
		if (iWord.empty())
			return;
		const float gap = iGap;
		ImVec2 wordSize = ImGui::CalcTextSize(iWord.data(), iWord.data() + iWord.size());
		if (iIsCode)
			wordSize.x += 2.0f * kInlineCodePadX;
		bool sameLine = false;
		float gapLeft = 0.0f;
		if (!firstOnLine) {
			const float prevRight = ImGui::GetItemRectMax().x;
			const float panelRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
			if (panelRight - (prevRight + gap) >= wordSize.x) {
				ImGui::SameLine(0.0f, gap);
				sameLine = true;
				gapLeft = prevRight;
			}
		}

		pushStyle();
		auto* drawList = ImGui::GetWindowDrawList();
		if (iIsCode) {
			// Inline code: a flat tinted span, not a widget, so it carries no click affordance.
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			const ImU32 background = ImGui::GetColorU32(ImGuiCol_FrameBg);
			const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
			drawList->AddRectFilled(origin, ImVec2{origin.x + wordSize.x, origin.y + wordSize.y}, background, 3.0f);
			drawList->AddText(ImVec2{origin.x + kInlineCodePadX, origin.y}, textColor, iWord.data(),
							  iWord.data() + iWord.size());
			ImGui::Dummy(wordSize);
		} else {
			ImGui::TextUnformatted(iWord.data(), iWord.data() + iWord.size());
		}
		const ImVec2 itemMin = ImGui::GetItemRectMin();
		const ImVec2 itemMax = ImGui::GetItemRectMax();
		if (strikeDepth > 0) {
			const float y = (itemMin.y + itemMax.y) * 0.5f;
			drawList->AddLine(ImVec2{itemMin.x, y}, ImVec2{itemMax.x, y}, ImGui::GetColorU32(ImGuiCol_Text), 1.0f);
		}
		popStyle();
		if (linkOpen)
			decorateLinkItem(linkId, linkExternal, linkHref,
							 sameLine && previousLinkId == linkId ? gapLeft : ImGui::GetItemRectMin().x);
		previousLinkId = linkOpen ? linkId : -1;
		firstOnLine = false;
	};

	const auto spaceWidth = [&spaceBefore]() -> float { return spaceBefore ? ImGui::CalcTextSize(" ").x : 0.0f; };
	const auto emitTextRun = [&](std::string_view iText) -> void {
		// Split on whitespace for word-level wrapping.
		while (!iText.empty()) {
			while (!iText.empty() && std::isspace(static_cast<unsigned char>(iText.front())) != 0) {
				iText.remove_prefix(1);
				spaceBefore = true;
			}
			if (iText.empty())
				break;
			size_t end = 0;
			while (end < iText.size() && std::isspace(static_cast<unsigned char>(iText[end])) == 0) ++end;

			emitWord(iText.substr(0, end), false, spaceWidth());
			spaceBefore = false;
			iText.remove_prefix(end);
		}
	};

	for (const auto& span: iSpans) {
		switch (span.kind) {
			case InlineKind::Text:

				emitTextRun(span.text);
				break;
			case InlineKind::Code:

				emitWord(span.text, true, spaceWidth());
				spaceBefore = false;
				break;
			case InlineKind::StrongStart:
				++strongDepth;
				break;
			case InlineKind::StrongEnd:
				if (strongDepth > 0)
					--strongDepth;
				break;
			case InlineKind::EmphasisStart:
				++emDepth;
				break;
			case InlineKind::EmphasisEnd:
				if (emDepth > 0)
					--emDepth;
				break;
			case InlineKind::StrikeStart:
				++strikeDepth;
				break;
			case InlineKind::StrikeEnd:
				if (strikeDepth > 0)
					--strikeDepth;
				break;
			case InlineKind::LinkStart:
				linkOpen = true;
				linkHref = span.text;
				linkExternal = isExternalUrl(linkHref);
				linkId = m_linkCount++;
				break;
			case InlineKind::LinkEnd:
				if (linkOpen && linkExternal && !firstOnLine)
					emitWord(gui::icon::externalLink, false, ImGui::CalcTextSize(" ").x * 0.4f);
				linkOpen = false;
				linkExternal = false;
				linkId = -1;
				linkHref.clear();
				break;
			case InlineKind::ImageInline:

				renderInlineImage(span.text, span.alt, firstOnLine, linkOpen, linkHref);
				previousLinkId = -1;
				spaceBefore = false;
				break;
			case InlineKind::LineBreak:

				ImGui::TextUnformatted("");
				firstOnLine = true;
				spaceBefore = false;
				break;
		}
	}
}

void MarkdownPreview::decorateLinkItem(const int iLinkId, const bool iExternal, const std::string& iHref,
									   const float iUnderlineLeft) {
	if (ImGui::IsItemHovered()) {
		m_nextHoveredLink = iLinkId;
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		if (iExternal)
			ImGui::SetTooltip("%s %s", gui::icon::externalLink, iHref.c_str());
	}
	if (ImGui::IsItemClicked())
		handleLinkClick(iHref);
	if (iLinkId != m_hoveredLink)
		return;
	const ImVec2 itemMax = ImGui::GetItemRectMax();
	ImGui::GetWindowDrawList()->AddLine(ImVec2{iUnderlineLeft, itemMax.y - 1.0f}, ImVec2{itemMax.x, itemMax.y - 1.0f},
										ImGui::GetColorU32(linkColour(gui::UiLayer::getTheme(), iExternal, true)),
										1.0f);
}

void MarkdownPreview::renderCodeBlock(const MdCodeBlock& iCb, size_t& ioCodeIndex) {
	if (ioCodeIndex >= m_codeEditors.size()) {
		ImGui::TextUnformatted(iCb.text.c_str());
		return;
	}
	auto& entry = m_codeEditors[ioCodeIndex++];
	ImFont* codeFont = nullptr;
	if (app::Application::instanced()) {
		if (const auto ui = app::Application::get().getImGuiLayer(); ui != nullptr)
			codeFont = ui->getCodeFont();
	}
	if (codeFont != nullptr)
		ImGui::PushFont(codeFont, 0.0f);
	const float lineH = ImGui::GetTextLineHeightWithSpacing();
	const int lines = std::clamp(countLines(iCb.text), 1, 30);
	const float height = static_cast<float>(lines) * lineH + 16.0f;
	const std::string title = "##md_code_" + std::to_string(ioCodeIndex);
	entry.editor->Render(title.c_str(), ImVec2{0.0f, height}, ImGuiChildFlags_Borders);
	if (codeFont != nullptr)
		ImGui::PopFont();
}

void MarkdownPreview::renderTable(const MdTable& iTable) {
	if (iTable.cols == 0)
		return;
	const auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
					   ImGuiTableFlags_Resizable;
	const std::string id = "##md_table_" + std::to_string(reinterpret_cast<uintptr_t>(&iTable));
	if (!ImGui::BeginTable(id.c_str(), static_cast<int>(iTable.cols), flags))
		return;
	for (uint32_t c = 0; c < iTable.cols; ++c) {
		const ImGuiTableColumnFlags colFlags = ImGuiTableColumnFlags_WidthStretch;
		ImGui::TableSetupColumn("", colFlags);
	}
	const auto renderRow = [&](const std::vector<std::vector<MdInline>>& iRow) -> void {
		for (uint32_t c = 0; c < iTable.cols; ++c) {
			ImGui::TableSetColumnIndex(static_cast<int>(c));
			if (c < iRow.size())
				renderTableCell(iRow[c]);
		}
	};
	// A head row of images (a gallery) has no title to show: it is drawn as a body row.
	const bool galleryHead = !iTable.headRows.empty() && std::ranges::all_of(iTable.headRows.front(), isImageCell);
	if (!iTable.headRows.empty()) {
		if (galleryHead) {
			ImGui::TableNextRow();
		} else {
			ImGui::TableHeadersRow();
		}
		renderRow(iTable.headRows.front());
	}
	for (const auto& row: iTable.bodyRows) {
		ImGui::TableNextRow();
		renderRow(row);
	}
	ImGui::EndTable();
}

void MarkdownPreview::renderTableCell(const std::vector<MdInline>& iSpans) {
	if (!isImageCell(iSpans)) {
		renderInlineSpans(iSpans);
		return;
	}
	for (const auto& span: iSpans) {
		if (span.kind != InlineKind::ImageInline)
			continue;
		const auto& entry = resolveImage(span.text);
		if (entry.failed || !entry.texture || entry.isRemote) {
			ImGui::TextDisabled("[image: %s]", span.alt.empty() ? span.text.c_str() : span.alt.c_str());
			continue;
		}
		const auto shown = entry.logicalSize.x() > 0 ? entry.logicalSize : entry.size;
		const float natural = span.width > 0 ? static_cast<float>(span.width) : static_cast<float>(shown.x());
		drawImage(span.text, entry, std::min(natural, std::max(1.0f, ImGui::GetContentRegionAvail().x)));
		if (!span.alt.empty()) {
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("%s", span.alt.c_str());
			ImGui::PopTextWrapPos();
		}
	}
}

void MarkdownPreview::drawImage(const std::string& iSrc, const ImageEntry& iEntry, const float iWidth) {
	const auto shown = iEntry.logicalSize.x() > 0 ? iEntry.logicalSize : iEntry.size;
	const float aspect = static_cast<float>(shown.y()) / static_cast<float>(std::max(1u, shown.x()));
	const auto im = iEntry.texture->getRendererId();
	const ImVec2 uv0 = iEntry.flippedY ? ImVec2{0.0f, 1.0f} : ImVec2{0.0f, 0.0f};
	const ImVec2 uv1 = iEntry.flippedY ? ImVec2{1.0f, 0.0f} : ImVec2{1.0f, 1.0f};
	ImGui::Image(static_cast<ImTextureID>(im), ImVec2{iWidth, iWidth * aspect}, uv0, uv1);
	if (iWidth >= static_cast<float>(shown.x()) * 0.95f || !ImGui::IsItemHovered())
		return;
	ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	ImGui::SetTooltip("Click to enlarge");
	if (ImGui::IsItemClicked()) {
		m_zoomSrc = iSrc;
		m_zoomRequested = true;
	}
}

void MarkdownPreview::renderZoom() {
	if (m_zoomSrc.empty())
		return;
	const auto found = m_imageCache.find(m_zoomSrc);
	if (found == m_imageCache.end() || !found->second.texture) {
		m_zoomSrc.clear();
		return;
	}
	const auto& entry = found->second;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const auto shown = entry.logicalSize.x() > 0 ? entry.logicalSize : entry.size;
	const float scale =
			std::min({1.0f, viewport->WorkSize.x * kZoomViewportRatio / static_cast<float>(std::max(1u, shown.x())),
					  viewport->WorkSize.y * kZoomViewportRatio / static_cast<float>(std::max(1u, shown.y()))});
	ImGui::SetNextWindowPos(ImVec2{viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
								   viewport->WorkPos.y + viewport->WorkSize.y * 0.5f},
							ImGuiCond_Always, ImVec2{0.5f, 0.5f});
	if (!ImGui::BeginPopup("##md_zoom")) {
		m_zoomSrc.clear();
		return;
	}
	const auto im = entry.texture->getRendererId();
	const ImVec2 uv0 = entry.flippedY ? ImVec2{0.0f, 1.0f} : ImVec2{0.0f, 0.0f};
	const ImVec2 uv1 = entry.flippedY ? ImVec2{1.0f, 0.0f} : ImVec2{1.0f, 1.0f};
	ImGui::Image(static_cast<ImTextureID>(im),
				 ImVec2{static_cast<float>(shown.x()) * scale, static_cast<float>(shown.y()) * scale}, uv0, uv1);
	if (ImGui::IsItemClicked() || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
		ImGui::CloseCurrentPopup();
	ImGui::EndPopup();
}

void MarkdownPreview::renderBadge(const std::string& iLabel, const std::string& iColour, bool& ioFirstOnLine) {
	const float spaceW = ImGui::CalcTextSize(" ").x;
	const float lineH = ImGui::GetTextLineHeight();
	gui::UiLayer::pushTextStyle(gui::TextStyle::Strong);
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kBadgeTextScale);
	const ImVec2 textSize = ImGui::CalcTextSize(iLabel.c_str());
	const ImVec2 size{textSize.x + 2.0f * kBadgePadX, lineH};
	if (!ioFirstOnLine && ImGui::GetContentRegionAvail().x >= size.x + spaceW)
		ImGui::SameLine(0.0f, spaceW);
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	auto* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(origin, ImVec2{origin.x + size.x, origin.y + size.y},
							ImGui::GetColorU32(badgeColour(iColour)), size.y * 0.5f);
	drawList->AddText(ImVec2{origin.x + kBadgePadX, origin.y + (size.y - textSize.y) * 0.5f},
					  ImGui::GetColorU32(ImVec4{1.0f, 1.0f, 1.0f, 1.0f}), iLabel.c_str());
	ImGui::PopFont();
	gui::UiLayer::popTextStyle();
	ImGui::Dummy(size);
	ioFirstOnLine = false;
}

// NOLINTBEGIN(misc-no-recursion) — list items contain blocks (incl. nested lists).
void MarkdownPreview::renderList(const MdList& iList, size_t& ioCodeIndex) {
	const float spaceW = ImGui::CalcTextSize(" ").x;
	const float boxSize = ImGui::GetTextLineHeight() * 0.8f;
	// Widest number of the list: markers are right-aligned in one column so the bodies line up.
	const std::string widest = std::to_string(iList.start + iList.items.size()) + ".";
	uint32_t number = iList.start;
	for (size_t i = 0; i < iList.items.size(); ++i) {
		const auto& item = iList.items[i];
		const TaskState task = i < iList.tasks.size() ? iList.tasks[i] : TaskState::None;
		ImGui::BeginGroup();
		float markerW = 0.0f;
		if (task != TaskState::None) {
			markerW = boxSize + spaceW;
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			const float top = origin.y + (ImGui::GetTextLineHeight() - boxSize) * 0.5f;
			auto* drawList = ImGui::GetWindowDrawList();
			const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
			drawList->AddRect(ImVec2{origin.x, top}, ImVec2{origin.x + boxSize, top + boxSize}, color, 2.0f);
			if (task == TaskState::Done) {
				const ImVec2 a{origin.x + boxSize * 0.22f, top + boxSize * 0.52f};
				const ImVec2 b{origin.x + boxSize * 0.42f, top + boxSize * 0.74f};
				const ImVec2 c{origin.x + boxSize * 0.78f, top + boxSize * 0.28f};
				drawList->AddPolyline(std::array{a, b, c}.data(), 3, color, ImDrawFlags_None, 2.0f);
			}
			ImGui::Dummy(ImVec2{boxSize, ImGui::GetTextLineHeight()});
		} else if (iList.ordered) {
			const std::string num = std::to_string(number++) + ".";
			markerW = ImGui::CalcTextSize(widest.c_str()).x + spaceW;
			const float pad = ImGui::CalcTextSize(widest.c_str()).x - ImGui::CalcTextSize(num.c_str()).x;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
			ImGui::TextUnformatted(num.c_str());
		} else {
			markerW = ImGui::CalcTextSize("\xe2\x80\xa2").x + spaceW;
			ImGui::TextUnformatted("\xe2\x80\xa2");// U+2022 BULLET
		}
		// Hanging indent: the body (and every wrapped line of it) starts after the marker.
		const float startX = ImGui::GetCursorPosX();
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::SetCursorPosX(startX + markerW);
		ImGui::Indent(markerW);
		for (const auto& sub: item) renderBlock(sub, ioCodeIndex, false);
		ImGui::Unindent(markerW);
		ImGui::EndGroup();
	}
}
// NOLINTEND(misc-no-recursion)

void MarkdownPreview::renderInlineImage(const std::string& iSrc, const std::string& iAlt, bool& ioFirstOnLine,
										const bool iLinkOpen, const std::string& iLinkHref) {
	if (iSrc.starts_with(kBadgeScheme)) {
		renderBadge(iAlt, iSrc.substr(kBadgeScheme.size()), ioFirstOnLine);
		return;
	}
	const auto& entry = resolveImage(iSrc);
	const float spaceW = ImGui::CalcTextSize(" ").x;
	if (!entry.failed && entry.texture && !entry.isRemote) {
		const float h = ImGui::GetTextLineHeight() * 1.4f;
		const float aspect =
				entry.size.y() > 0 ? static_cast<float>(entry.size.x()) / static_cast<float>(entry.size.y()) : 1.0f;
		const float w = h * aspect;
		if (!ioFirstOnLine && ImGui::GetContentRegionAvail().x >= w + spaceW)
			ImGui::SameLine(0.0f, spaceW);
		const auto im = entry.texture->getRendererId();
		const ImVec2 uv0 = entry.flippedY ? ImVec2{0.0f, 1.0f} : ImVec2{0.0f, 0.0f};
		const ImVec2 uv1 = entry.flippedY ? ImVec2{1.0f, 0.0f} : ImVec2{1.0f, 1.0f};
		ImGui::Image(static_cast<ImTextureID>(im), ImVec2{w, h}, uv0, uv1);
		if (iLinkOpen && ImGui::IsItemClicked())
			handleLinkClick(iLinkHref);
	} else {
		// Fallback: small button labelled with alt text.
		const std::string label = iAlt.empty() ? iSrc : iAlt;
		const float buttonW = ImGui::CalcTextSize(label.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
		if (!ioFirstOnLine && ImGui::GetContentRegionAvail().x >= buttonW + spaceW)
			ImGui::SameLine(0.0f, spaceW);
		if (ImGui::SmallButton(label.c_str()))
			handleLinkClick(iSrc);
	}
	ioFirstOnLine = false;
}

void MarkdownPreview::renderImage(const std::string& iSrc, const std::string& iAlt, const uint32_t iWidth) {
	const auto& entry = resolveImage(iSrc);
	if (entry.isRemote) {
		const std::string label = (iAlt.empty() ? iSrc : iAlt) + "  \xe2\x86\x97";
		if (ImGui::Button(label.c_str()))
			handleLinkClick(iSrc);
		return;
	}
	if (entry.failed || !entry.texture) {
		ImGui::TextDisabled("[image: %s]", iSrc.c_str());
		return;
	}
	const float availX = std::max(1.0f, ImGui::GetContentRegionAvail().x - kImageHorizontalPadding);
	// A rasterised SVG is shown at its own size, not at its (oversampled) texture size.
	const auto shown = entry.logicalSize.x() > 0 ? entry.logicalSize : entry.size;
	const float wanted = iWidth > 0 ? static_cast<float>(iWidth) : static_cast<float>(shown.x());
	const float width = std::min(availX, wanted);
	const auto cursor = ImGui::GetCursorPos();
	ImGui::SetCursorPosX(cursor.x + (ImGui::GetContentRegionAvail().x - width) * 0.5f);
	drawImage(iSrc, entry, width);
}

auto MarkdownPreview::resolveImage(const std::string& iSrc) -> const ImageEntry& {
	if (auto it = m_imageCache.find(iSrc); it != m_imageCache.end())
		return it->second;
	ImageEntry entry{};
	if (isExternalUrl(iSrc)) {
		entry.isRemote = true;
		const auto [it, _] = m_imageCache.emplace(iSrc, std::move(entry));
		return it->second;
	}
	if (m_baseDir.empty()) {
		entry.failed = true;
		const auto [it, _] = m_imageCache.emplace(iSrc, std::move(entry));
		return it->second;
	}
	const std::filesystem::path full = m_baseDir / iSrc;
	if (!exists(full)) {
		entry.failed = true;
		const auto [it, _] = m_imageCache.emplace(iSrc, std::move(entry));
		return it->second;
	}
	const auto ext = full.extension().string();
	const bool isSvg = ext == ".svg" || ext == ".SVG";
	if (isSvg) {
		registerSvgFonts();
		std::string svgText;
		try {
			const std::ifstream stream(full, std::ios::binary);
			std::stringstream buf;
			buf << stream.rdbuf();
			svgText = buf.str();
		} catch (...) { entry.failed = true; }
		if (!entry.failed) {
			if (auto doc = lunasvg::Document::loadFromData(svgText); doc) {
				const float intrW = std::max(1.0f, doc->width());
				const float intrH = std::max(1.0f, doc->height());
				const float scale = std::min(static_cast<float>(kImageMaxSide) / std::max(intrW, intrH), 4.0f);
				const auto pixW = static_cast<uint32_t>(std::max(1.0f, intrW * scale));
				const auto pixH = static_cast<uint32_t>(std::max(1.0f, intrH * scale));
				if (auto bitmap = doc->renderToBitmap(static_cast<int>(pixW), static_cast<int>(pixH));
					!bitmap.isNull()) {
					const auto pixelCount = static_cast<size_t>(pixW) * pixH;
					std::vector<uint8_t> pixels(pixelCount * 4);
					const auto* src = bitmap.data();
					for (size_t i = 0; i < pixelCount; ++i) {
						const size_t idx = i * 4;
						const uint8_t a = src[idx + 3];
						if (a == 0) {
							pixels[idx + 0] = pixels[idx + 1] = pixels[idx + 2] = pixels[idx + 3] = 0;
						} else {
							pixels[idx + 0] = static_cast<uint8_t>(static_cast<uint16_t>(src[idx + 2]) * 255 / a);
							pixels[idx + 1] = static_cast<uint8_t>(static_cast<uint16_t>(src[idx + 1]) * 255 / a);
							pixels[idx + 2] = static_cast<uint8_t>(static_cast<uint16_t>(src[idx + 0]) * 255 / a);
							pixels[idx + 3] = a;
						}
					}
					const renderer::gpu::Texture::Specification specs{.size = {pixW, pixH},
																	  .format = renderer::gpu::ImageFormat::Rgba8,
																	  .generateMips = false};
					entry.texture = renderer::gpu::Texture2D::create(specs);
					entry.size = {pixW, pixH};
					entry.logicalSize = {static_cast<uint32_t>(intrW), static_cast<uint32_t>(intrH)};
					entry.texture->setData(pixels.data(), static_cast<uint32_t>(pixels.size()));
				} else {
					entry.failed = true;
				}
			} else {
				entry.failed = true;
			}
		}
	} else {
		entry.texture = renderer::gpu::Texture2D::createFromSerialized("pat:" + full.string());
		if (entry.texture) {
			entry.size = entry.texture->getSize();
			entry.flippedY = true;
		} else {
			entry.failed = true;
		}
	}
	const auto [it, _] = m_imageCache.emplace(iSrc, std::move(entry));
	return it->second;
}

void MarkdownPreview::handleLinkClick(const std::string& iHref) const {
	if (iHref.empty())
		return;
	if (isExternalUrl(iHref)) {
		platform::openExternalUrl(iHref);
		return;
	}
	if (m_linkCallback)
		m_linkCallback(iHref);
}

}// namespace owl::nest::codeEditor
