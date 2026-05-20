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

#include <core/Application.h>
#include <core/Log.h>
#include <core/Macros.h>
#include <core/utils/openExternalUrl.h>
#include <gui/UiLayer.h>
#include <renderer/gpu/Texture.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>

namespace owl::nest::codeEditor {

namespace {
constexpr float kHeading1Scale = 1.60f;
constexpr float kHeading2Scale = 1.30f;
constexpr float kHeading3Scale = 1.15f;
constexpr uint32_t kImageMaxSide = 1024;
constexpr float kImageHorizontalPadding = 16.0f;

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

[[nodiscard]] auto headingScale(const uint8_t iLevel) -> float {
	switch (iLevel) {
		case 1:
			return kHeading1Scale;
		case 2:
			return kHeading2Scale;
		case 3:
			return kHeading3Scale;
		default:
			return 1.0f;
	}
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
		m_needsRebuild = false;
	}
	ImGui::BeginChild("##md_preview", iSize, ImGuiChildFlags_Borders);
	if (m_renderedText.empty()) {
		ImGui::TextDisabled("(empty)");
	} else {
		size_t codeIndex = 0;
		for (const auto& block: m_document.blocks()) renderBlock(block, codeIndex, /*iCompact=*/false);
	}
	ImGui::EndChild();
}

// NOLINTBEGIN(misc-no-recursion) — block tree recurses via lists/quotes.
void MarkdownPreview::renderBlock(const MdBlock& iBlock, size_t& ioCodeIndex, const bool iCompact) {
	std::visit(
			[&]<typename T>(const T& iVal) -> void {
				if constexpr (std::is_same_v<T, MdHeading>) {
					ImFont* const baseFont = ImGui::GetFont();
					const float baseSize = ImGui::GetFontSize();
					ImGui::PushFont(baseFont, baseSize * headingScale(iVal.level));
					renderInlineSpans(iVal.spans);
					ImGui::PopFont();
					if (iVal.level <= 2)
						ImGui::Separator();
					else if (!iCompact)
						ImGui::Spacing();
				} else if constexpr (std::is_same_v<T, MdParagraph>) {
					// A paragraph that contains only a single block-level image is rendered as an Image.
					if (iVal.spans.size() == 1 && iVal.spans.front().kind == InlineKind::ImageInline) {
						const auto& img = iVal.spans.front();
						renderImage(img.text, img.alt);
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
					renderImage(iVal.src, iVal.alt);
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
}
// NOLINTEND(misc-no-recursion)

void MarkdownPreview::renderInlineSpans(const std::vector<MdInline>& iSpans) {
	int strongDepth = 0;
	int emDepth = 0;
	int strikeDepth = 0;
	bool linkOpen = false;
	std::string linkHref;
	bool firstOnLine = true;
	const auto pushStyle = [&]() -> void {
		if (strongDepth > 0)

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
		else if (emDepth > 0)

			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		else if (linkOpen)

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.40f, 0.70f, 1.00f, 1.00f});
	};
	const auto popStyle = [&]() -> void {
		if (strongDepth > 0 || emDepth > 0 || linkOpen)

			ImGui::PopStyleColor();
	};

	const auto emitWord = [&](const std::string_view iWord, const bool iIsCode) -> void {
		if (iWord.empty())
			return;
		const float spaceW = ImGui::CalcTextSize(" ").x;
		const ImVec2 wordSize = ImGui::CalcTextSize(iWord.data(), iWord.data() + iWord.size());
		if (!firstOnLine) {
			const float prevRight = ImGui::GetItemRectMax().x;
			const float panelRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
			const float remaining = panelRight - (prevRight + spaceW);
			if (remaining >= wordSize.x)

				ImGui::SameLine(0.0f, spaceW);
		}

		pushStyle();
		if (iIsCode) {
			// Inline code: render inside a slightly tinted background.
			const auto bg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);

			ImGui::PushStyleColor(ImGuiCol_Button, bg);

			ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg);

			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg);

			ImGui::SmallButton(std::string{iWord}.c_str());

			ImGui::PopStyleColor(3);
		} else {
			ImGui::TextUnformatted(iWord.data(), iWord.data() + iWord.size());
		}

		popStyle();
		if (linkOpen && ImGui::IsItemClicked())

			handleLinkClick(linkHref);
		firstOnLine = false;
	};

	const auto emitTextRun = [&](std::string_view iText) -> void {
		// Split on whitespace for word-level wrapping.
		while (!iText.empty()) {
			while (!iText.empty() && std::isspace(static_cast<unsigned char>(iText.front())) != 0)
				iText.remove_prefix(1);
			if (iText.empty())
				break;
			size_t end = 0;
			while (end < iText.size() && std::isspace(static_cast<unsigned char>(iText[end])) == 0) ++end;

			emitWord(iText.substr(0, end), false);
			iText.remove_prefix(end);
		}
	};

	for (const auto& span: iSpans) {
		switch (span.kind) {
			case InlineKind::Text:

				emitTextRun(span.text);
				break;
			case InlineKind::Code:

				emitWord(span.text, true);
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
				break;
			case InlineKind::LinkEnd:
				linkOpen = false;
				linkHref.clear();
				break;
			case InlineKind::ImageInline:

				renderInlineImage(span.text, span.alt, firstOnLine, linkOpen, linkHref);
				break;
			case InlineKind::LineBreak:

				ImGui::TextUnformatted("");
				firstOnLine = true;
				break;
		}
	}
}

void MarkdownPreview::renderCodeBlock(const MdCodeBlock& iCb, size_t& ioCodeIndex) {
	if (ioCodeIndex >= m_codeEditors.size()) {
		ImGui::TextUnformatted(iCb.text.c_str());
		return;
	}
	auto& entry = m_codeEditors[ioCodeIndex++];
	ImFont* codeFont = nullptr;
	if (core::Application::instanced()) {
		if (const auto ui = core::Application::get().getImGuiLayer(); ui != nullptr)
			codeFont = ui->getCodeFont();
	}
	if (codeFont != nullptr)
		ImGui::PushFont(codeFont, 0.0f);
	const float lineH = ImGui::GetTextLineHeightWithSpacing();
	const int lines = std::clamp(countLines(iCb.text), 1, 30);
	const float height = static_cast<float>(lines) * lineH + 16.0f;
	const std::string title = "##md_code_" + std::to_string(ioCodeIndex);
	entry.editor->Render(title.c_str(), ImVec2{0.0f, height}, /*border=*/true);
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
	// Header.
	if (!iTable.headRows.empty()) {
		ImGui::TableHeadersRow();
		for (uint32_t c = 0; c < iTable.cols; ++c) {
			ImGui::TableSetColumnIndex(static_cast<int>(c));
			if (c < iTable.headRows.front().size())
				renderInlineSpans(iTable.headRows.front()[c]);
		}
	}
	// Body.
	for (const auto& row: iTable.bodyRows) {
		ImGui::TableNextRow();
		for (uint32_t c = 0; c < iTable.cols; ++c) {
			ImGui::TableSetColumnIndex(static_cast<int>(c));
			if (c < row.size())
				renderInlineSpans(row[c]);
		}
	}
	ImGui::EndTable();
}

// NOLINTBEGIN(misc-no-recursion) — list items contain blocks (incl. nested lists).
void MarkdownPreview::renderList(const MdList& iList, size_t& ioCodeIndex) {
	const float spaceW = ImGui::CalcTextSize(" ").x;
	int idx = 1;
	for (const auto& item: iList.items) {
		ImGui::BeginGroup();
		if (iList.ordered) {
			const std::string num = std::to_string(idx++) + ".";
			ImGui::TextUnformatted(num.c_str());
		} else {
			ImGui::TextUnformatted("\xe2\x80\xa2");// U+2022 BULLET
		}
		ImGui::SameLine(0.0f, spaceW);
		for (const auto& sub: item) renderBlock(sub, ioCodeIndex, false);
		ImGui::EndGroup();
	}
}
// NOLINTEND(misc-no-recursion)

void MarkdownPreview::renderInlineImage(const std::string& iSrc, const std::string& iAlt, bool& ioFirstOnLine,
										const bool iLinkOpen, const std::string& iLinkHref) {
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

void MarkdownPreview::renderImage(const std::string& iSrc, const std::string& iAlt) {
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
	const float aspect = static_cast<float>(entry.size.y()) / static_cast<float>(std::max(1u, entry.size.x()));
	const float width = std::min(availX, static_cast<float>(entry.size.x()));
	const float height = width * aspect;
	const auto cursor = ImGui::GetCursorPos();
	ImGui::SetCursorPosX(cursor.x + (ImGui::GetContentRegionAvail().x - width) * 0.5f);
	const auto im = entry.texture->getRendererId();
	const ImVec2 uv0 = entry.flippedY ? ImVec2{0.0f, 1.0f} : ImVec2{0.0f, 0.0f};
	const ImVec2 uv1 = entry.flippedY ? ImVec2{1.0f, 0.0f} : ImVec2{1.0f, 1.0f};
	ImGui::Image(static_cast<ImTextureID>(im), ImVec2{width, height}, uv0, uv1);
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
		core::utils::openExternalUrl(iHref);
		return;
	}
	if (m_linkCallback)
		m_linkCallback(iHref);
}

}// namespace owl::nest::codeEditor
