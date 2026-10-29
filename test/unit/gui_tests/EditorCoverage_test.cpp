/**
 * @file EditorCoverage_test.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

// The editor reach rule, checked on the sources: every authored field of a component has a widget in its inspector, and
// every key a render layer reads has a field in the layer overrides of Scene Settings. The test reads the headers of
// the components, the inspector (`gui/component/render.cpp`), the layers and `SceneSettings.cpp`; a new field or key
// with no widget fails it, naming the field, until a widget is added (or, for a field that has no business in an
// inspector, it is listed below with the reason).

#include "testHelper.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

auto readFile(const std::filesystem::path& iPath) -> std::string {
	const std::ifstream file{iPath, std::ios::binary};
	std::ostringstream text;
	text << file.rdbuf();
	return text.str();
}

auto sourceRoot() -> std::filesystem::path { return owl::test::getRootPath() / "source" / "owl"; }

// The text of the braces opened at the first `{` found from a position, nested braces included.
auto bracedBlock(const std::string& iText, const size_t iFrom) -> std::string {
	const auto open = iText.find('{', iFrom);
	if (open == std::string::npos)
		return {};
	int depth = 0;
	for (size_t index = open; index < iText.size(); ++index) {
		if (iText[index] == '{')
			++depth;
		else if (iText[index] == '}')
			--depth;
		if (depth == 0)
			return iText.substr(open + 1, index - open - 1);
	}
	return {};
}

// Public data members of `struct OWL_API <Name>`: the declarations at the first level of its braces.
auto fieldsOf(const std::string& iHeader, const std::string& iName) -> std::vector<std::string> {
	const auto start = iHeader.find("struct OWL_API " + iName + " {");
	if (start == std::string::npos)
		return {};
	auto body = bracedBlock(iHeader, start);
	body = std::regex_replace(body, std::regex{R"re(/\*[\s\S]*?\*/)re"}, "");
	body = std::regex_replace(body, std::regex{R"re(//[^\n]*)re"}, "");
	std::vector<std::string> fields;
	int depth = 0;
	std::string statement;
	for (const char character: body) {
		if (character == '{')
			++depth;
		if (depth == 0)
			statement += character;
		if (character == '}')
			--depth;
		if (depth == 0 && character == ';') {
			static const std::regex field{R"re(^\s*[\w:<>,\*&\s]+?\s+(\w+)\s*(?:=\s*[^;]+)?;\s*$)re"};
			std::string cleaned = statement;
			cleaned = std::regex_replace(cleaned, std::regex{R"re(\{[^{}]*\})re"}, "");
			const auto first = cleaned.find_first_not_of(" \t\n");
			const std::string head = first == std::string::npos ? std::string{} : cleaned.substr(first, 12);
			const bool declaration =
					std::ranges::none_of(std::vector<std::string>{"static", "using", "enum", "struct", "class",
																  "friend", "typedef", "constexpr", "template"},
										 [&head](const std::string& iWord) -> bool { return head.starts_with(iWord); });
			if (std::smatch match;
				declaration && cleaned.find('(') == std::string::npos && std::regex_search(cleaned, match, field)) {
				const auto name = match[1].str();
				if (!name.starts_with("m_") && !name.starts_with("mp_"))
					fields.push_back(name);
			}
			statement.clear();
		}
	}
	return fields;
}

// Members the inspector reads in every function taking the component by reference (`<param>.<field>`).
auto fieldsDrawn(const std::string& iRender, const std::string& iName) -> std::set<std::string> {
	std::set<std::string> drawn;
	const std::regex signature{"[^\\w]" + iName + "& (\\w+)\\s*[,)]"};
	for (auto it = std::sregex_iterator(iRender.begin(), iRender.end(), signature); it != std::sregex_iterator();
		 ++it) {
		const auto body = bracedBlock(iRender, static_cast<size_t>(it->position()));
		const std::regex use{"\\b" + (*it)[1].str() + "\\.(\\w+)"};
		for (auto field = std::sregex_iterator(body.begin(), body.end(), use); field != std::sregex_iterator(); ++field)
			drawn.insert((*field)[1].str());
	}
	return drawn;
}

// Fields that are not authored in the inspector, by component, with the reason.
const std::map<std::string, std::set<std::string>> g_notDrawn{
		{"SoundListener", {"runtimeLastPosition", "runtimeHasLastPosition"}},// runtime: the Doppler velocity
		{"AdaptiveMusic", {"runtimePlayer"}},// runtime: the music playing
		{"Camera", {"camera"}},// a struct drawn field by field (`ioComponent.camera.*` in the camera section)
		{"UiButton", {"state"}},// runtime: hover and press state
		{"UiList",
		 {"rows", "hoverIndex", "revealSelected", "iconCache"}},// runtime: the rows filled by Lua and the draw state
		{"EntityLink", {"linkedEntity", "wasUnresolvedReported"}},// runtime: resolved from the linked name and id
		{"RaycastDoor", {"bodyId"}},// runtime: the physics body
		{"RaycastPushWall", {"bodyId"}},// runtime: the physics body
		{"LuaScript", {"instance"}},// runtime: the Lua state
		{"Animator", {"pose", "boneMatrices", "skinMatrices", "source"}},// runtime: the pose computed by the system
		{"VoxelPlayer",
		 {"velocityY", "yaw", "pitch", "grounded", "initialized", "flyMode", "superSpeed", "lastMouse", "mouseValid",
		  "doubleTapTimer", "hasTarget", "targetBlock"}},// runtime: the controller state of the player
};

// Components without an inspector section: C++ only, never authored in a scene.
const std::set<std::string> g_noInspector{"NativeScript"};

}// namespace

TEST(EditorCoverage, EveryAuthoredFieldOfAComponentHasAWidgetInItsInspector) {
	const auto render = readFile(sourceRoot() / "private" / "gui" / "component" / "render.cpp");
	ASSERT_FALSE(render.empty());
	std::vector<std::string> missing;
	size_t checked = 0;
	for (const auto& entry: std::filesystem::directory_iterator{sourceRoot() / "public" / "scene" / "component"}) {
		const auto name = entry.path().stem().string();
		if (name == "components" || name == "ID" || name == "Tag" || name == "Hierarchy")
			continue;
		const auto header = readFile(entry.path());
		const auto fields = fieldsOf(header, name);
		const auto drawn = fieldsDrawn(render, name);
		if (g_noInspector.contains(name))
			continue;
		EXPECT_FALSE(drawn.empty()) << name << " has no inspector function taking `" << name << "& ioComponent`";
		for (const auto& field: fields) {
			++checked;
			if (drawn.contains(field) || (g_notDrawn.contains(name) && g_notDrawn.at(name).contains(field)))
				continue;
			missing.push_back(std::string{name}.append("::").append(field));
		}
	}
	EXPECT_GT(checked, 100u);
	std::string report;
	for (const auto& field: missing) report += " " + field;
	EXPECT_TRUE(missing.empty()) << "fields with no widget in the inspector:" << report;
}

TEST(EditorCoverage, EveryKeyARenderLayerReadsHasAFieldInTheSceneSettingsOverrides) {
	const auto settings =
			readFile(owl::test::getRootPath() / "source" / "owlnest" / "sources" / "panel" / "SceneSettings.cpp");
	ASSERT_FALSE(settings.empty());
	std::vector<std::string> missing;
	size_t checked = 0;
	for (const auto& entry: std::filesystem::directory_iterator{sourceRoot() / "private" / "renderer"}) {
		const auto file = entry.path().filename().string();
		if (!file.starts_with("Renderer") || !file.ends_with("Layer.cpp"))
			continue;
		const auto code = readFile(entry.path());
		const auto start = code.find("::applyConfig(");
		if (start == std::string::npos)
			continue;
		const auto body = bracedBlock(code, start);
		const std::regex key{R"re(config\["(\w+)"\])re"};
		for (auto it = std::sregex_iterator(body.begin(), body.end(), key); it != std::sregex_iterator(); ++it) {
			++checked;
			const auto name = (*it)[1].str();
			if (settings.find(std::string{"\""}.append(name).append("\"")) == std::string::npos)
				missing.push_back(std::string{file}.append(":").append(name));
		}
	}
	EXPECT_GT(checked, 15u);
	std::string report;
	for (const auto& key: missing) report += " " + key;
	EXPECT_TRUE(missing.empty()) << "layer keys with no override field in Scene Settings:" << report;
}

TEST(EditorCoverage, TheCheckSeesAFieldWithoutAWidget) {
	const std::string header = "struct OWL_API Lamp {\n"
							   "	/// Colour.\n"
							   "	math::vec3 color{1.f, 1.f, 1.f};\n"
							   "	float intensity = 2.f;\n"
							   "	std::vector<float> flicker;\n"
							   "	float m_runtime = 0.f;\n"
							   "	enum struct Mode : uint8_t { A, B };\n"
							   "	Mode mode = Mode::A;\n"
							   "	static auto key() -> const char* { return \"Lamp\"; }\n"
							   "};\n";
	const auto fields = fieldsOf(header, "Lamp");
	EXPECT_EQ(fields, (std::vector<std::string>{"color", "intensity", "flicker", "mode"}));
	const std::string render = "void renderProps(Lamp& ioComponent) {\n"
							   "	prop::ColorEdit3(\"Color\", &ioComponent.color);\n"
							   "	prop::DragFloat(\"Intensity\", &ioComponent.intensity);\n"
							   "}\n";
	const auto drawn = fieldsDrawn(render, "Lamp");
	EXPECT_TRUE(drawn.contains("color"));
	EXPECT_TRUE(drawn.contains("intensity"));
	EXPECT_FALSE(drawn.contains("flicker"));
	EXPECT_FALSE(drawn.contains("mode"));
}
