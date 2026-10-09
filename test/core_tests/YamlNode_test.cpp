/**
 * @file YamlNode_test.cpp
 * @author Silmaen
 * @date 09/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/YamlNode.h>
#include <core/external/yaml.h>
#include <math/YamlSerializers.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

using namespace owl;
using namespace owl::core;

TEST(YamlNode, MissingNullAndPresentKeys) {
	const YamlDocument doc{"a: 1\nb:\nc: ~\nd: null\ne: \"\"\nf: 'null'\n"};
	const auto root = doc.getRoot();
	EXPECT_TRUE(root.isMap());
	EXPECT_FALSE(root["missing"]);
	EXPECT_FALSE(root["missing"]["deeper"]);
	EXPECT_TRUE(root["a"].isScalar());
	for (const auto* key: {"b", "c", "d"}) {
		EXPECT_TRUE(root[key]) << key;
		EXPECT_TRUE(root[key].isNull()) << key;
		EXPECT_FALSE(root[key].isScalar()) << key;
		EXPECT_EQ(root[key].as<std::string>(), "null") << key;
	}
	EXPECT_TRUE(root["e"].isScalar());
	EXPECT_EQ(root["e"].as<std::string>(), "");
	EXPECT_FALSE(root["f"].isNull());
	EXPECT_EQ(root["f"].as<std::string>(), "null");
	EXPECT_EQ(root.size(), 6U);
}

TEST(YamlNode, IndexingAScalarThrows) {
	const YamlDocument doc{"Tag: 5\nlist: [1, 2]\n"};
	const auto root = doc.getRoot();
	EXPECT_THROW((void) root["Tag"]["tag"], YamlError);
	EXPECT_THROW((void) root["Tag"][0], YamlError);
	EXPECT_FALSE(root["list"]["key"]);
	EXPECT_FALSE(root["list"][5]);
	EXPECT_EQ(root["list"][1].as<int>(), 2);
	EXPECT_THROW((void) root["missing"].as<int>(), YamlError);
	EXPECT_EQ(root["missing"].as<int>(7), 7);
	EXPECT_EQ(root["list"].as<int>(7), 7);
}

TEST(YamlNode, Booleans) {
	const YamlDocument doc{"[true, True, TRUE, yes, On, y, false, No, OFF, n, tRue, 1]"};
	const auto root = doc.getRoot();
	const std::vector<bool> expected{true, true, true, true, true, true, false, false, false, false};
	for (size_t i = 0; i < expected.size(); ++i) EXPECT_EQ(root[i].as<bool>(), expected[i]) << i;
	EXPECT_THROW((void) root[10].as<bool>(), YamlError);
	EXPECT_THROW((void) root[11].as<bool>(), YamlError);
}

TEST(YamlNode, Integers) {
	const YamlDocument doc{"[42, -7, +5, 0x1F, -0x10, 3000000000, -1, 1.5, 12abc, 18446744073709551615]"};
	const auto root = doc.getRoot();
	EXPECT_EQ(root[0].as<uint32_t>(), 42U);
	EXPECT_EQ(root[1].as<int32_t>(), -7);
	EXPECT_EQ(root[2].as<int>(), 5);
	EXPECT_EQ(root[3].as<uint16_t>(), 31U);
	EXPECT_EQ(root[4].as<int64_t>(), -16);
	EXPECT_THROW((void) root[5].as<int32_t>(), YamlError);
	EXPECT_EQ(root[5].as<uint32_t>(), 3000000000U);
	EXPECT_THROW((void) root[6].as<uint32_t>(), YamlError);
	EXPECT_THROW((void) root[7].as<int>(), YamlError);
	EXPECT_THROW((void) root[8].as<int>(), YamlError);
	EXPECT_EQ(root[9].as<uint64_t>(), std::numeric_limits<uint64_t>::max());
}

TEST(YamlNode, Floats) {
	const YamlDocument doc{"[1.5, -2, +0.25, 1e3, .inf, -.Inf, .NaN, abc, inf]"};
	const auto root = doc.getRoot();
	EXPECT_FLOAT_EQ(root[0].as<float>(), 1.5f);
	EXPECT_FLOAT_EQ(root[1].as<float>(), -2.f);
	EXPECT_DOUBLE_EQ(root[2].as<double>(), 0.25);
	EXPECT_FLOAT_EQ(root[3].as<float>(), 1000.f);
	EXPECT_TRUE(std::isinf(root[4].as<float>()));
	EXPECT_LT(root[5].as<double>(), 0.0);
	EXPECT_TRUE(std::isnan(root[6].as<float>()));
	EXPECT_THROW((void) root[7].as<float>(), YamlError);
	EXPECT_THROW((void) root[8].as<float>(), YamlError);
}

TEST(YamlNode, VectorsAndIteration) {
	const YamlDocument doc{"v3: [1, 2, 3]\nv4: [1, 2, 3, 4]\nlist:\n  - a\n  - b\n"};
	const auto root = doc.getRoot();
	EXPECT_EQ(root["v3"].as<math::vec3>(), (math::vec3{1.f, 2.f, 3.f}));
	EXPECT_EQ(root["v4"].as<math::vec4>(), (math::vec4{1.f, 2.f, 3.f, 4.f}));
	EXPECT_THROW((void) root["v3"].as<math::vec4>(), YamlError);
	std::vector<std::string> items;
	for (const auto item: root["list"]) items.push_back(item.as<std::string>());
	EXPECT_EQ(items, (std::vector<std::string>{"a", "b"}));
	std::vector<std::string_view> keys;
	for (const auto entry: root) keys.push_back(entry.getKey());
	EXPECT_EQ(keys.size(), 3U);
	EXPECT_EQ(keys.front(), "v3");
	EXPECT_EQ(root["v3"].begin(), root["v3"].begin());
	EXPECT_EQ(root["v3"]["x"].begin(), root["v3"]["x"].end());
}

TEST(YamlNode, LineAndErrors) {
	const YamlDocument doc{"a: 1\nb:\n  c: 2\n"};
	EXPECT_EQ(doc.getRoot()["a"].getLine(), 1U);
	EXPECT_EQ(doc.getRoot()["b"]["c"].getLine(), 3U);
	EXPECT_EQ(YamlNode{}.getLine(), 0U);
	EXPECT_THROW(YamlDocument("a: [1, 2\nb: 3\n", "broken.yml"), YamlError);
	EXPECT_THROW(YamlDocument("a: b: c\n"), YamlError);
}

TEST(YamlNode, DocumentsAndAliases) {
	const YamlDocument stream{"---\nfirst: 1\n---\nsecond: 2\n"};
	EXPECT_EQ(stream.getRoot()["first"].as<int>(), 1);
	EXPECT_FALSE(stream.getRoot()["second"]);
	const YamlDocument empty{""};
	EXPECT_FALSE(empty.getRoot().isMap());
	const YamlDocument aliases{"base: &b [1, 2, 3]\ncopy: *b\n"};
	EXPECT_EQ(aliases.getRoot()["copy"].as<math::vec3>(), (math::vec3{1.f, 2.f, 3.f}));
}

TEST(YamlNode, SameAsIgnoresLayout) {
	const YamlDocument flow{"x: {a: [1, 2], b: \"s\", c: ~}"};
	const YamlDocument block{"x:\n  a:\n    - 1\n    - 2\n  b: s\n  c:\n"};
	const YamlDocument other{"x: {a: [1, 3], b: s, c: ~}"};
	const YamlDocument reordered{"x: {b: s, a: [1, 2], c: ~}"};
	EXPECT_TRUE(flow.getRoot()["x"].isSameAs(block.getRoot()["x"]));
	EXPECT_FALSE(flow.getRoot()["x"].isSameAs(other.getRoot()["x"]));
	EXPECT_FALSE(flow.getRoot()["x"].isSameAs(reordered.getRoot()["x"]));
	EXPECT_FALSE(flow.getRoot()["x"].isSameAs(flow.getRoot()["missing"]));
	EXPECT_TRUE(YamlNode{}.isSameAs(YamlNode{}));
}

TEST(YamlNode, EmitIsReadableByYamlCpp) {
	const YamlDocument doc{"Overrides:\n  ratio: 1.5\n  list: [a, b]\n  name: \"x: y\"\n"};
	const auto text = doc.getRoot()["Overrides"].emit();
	const auto reread = YAML::Load(text);
	EXPECT_FLOAT_EQ(reread["ratio"].as<float>(), 1.5f);
	EXPECT_EQ(reread["list"][1].as<std::string>(), "b");
	EXPECT_EQ(reread["name"].as<std::string>(), "x: y");
	EXPECT_EQ(doc.getRoot()["Overrides"]["ratio"].emit(), "1.5");
	EXPECT_TRUE(doc.getRoot()["missing"].emit().empty());
}

TEST(YamlNode, ReadsWhatYamlCppWrites) {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "text" << YAML::Value << "line one\nline two: \"quoted\"";
	out << YAML::Key << "empty" << YAML::Value << "";
	out << YAML::Key << "number" << YAML::Value << 0.1f;
	out << YAML::Key << "vec" << YAML::Value << math::vec3{0.5f, -1.f, 2.25f};
	out << YAML::Key << "flag" << YAML::Value << true;
	out << YAML::Key << "big" << YAML::Value << uint64_t{18446744073709551615ULL};
	out << YAML::EndMap;
	const YamlDocument doc{out.c_str()};
	const auto root = doc.getRoot();
	EXPECT_EQ(root["text"].as<std::string>(), "line one\nline two: \"quoted\"");
	EXPECT_EQ(root["empty"].as<std::string>(), "");
	EXPECT_FLOAT_EQ(root["number"].as<float>(), 0.1f);
	EXPECT_EQ(root["vec"].as<math::vec3>(), (math::vec3{0.5f, -1.f, 2.25f}));
	EXPECT_TRUE(root["flag"].as<bool>());
	EXPECT_EQ(root["big"].as<uint64_t>(), 18446744073709551615ULL);
}
