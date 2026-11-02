/**
 * @file ContentBrowserStarter_test.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "panel/ContentBrowser.h"

#include <script/ScriptEngine.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace owl;

TEST(ContentBrowserStarter, theLuaScriptDeclaresItsPropertyAndHooks) {
	const auto text = nest::panel::ContentBrowser::starterLuaScript();
	const std::vector<uint8_t> bytes(text.begin(), text.end());
	const auto properties = script::ScriptEngine::extractPropertiesFromBuffer(bytes, "new_script.lua");
	ASSERT_EQ(properties.size(), 1u);
	EXPECT_EQ(properties.front().name, "speed");
	const auto functions = script::ScriptEngine::extractFunctionsFromBuffer(bytes, "new_script.lua");
	const auto has = [&functions](const std::string& iName) -> bool {
		return std::ranges::any_of(functions,
								   [&iName](const auto& iFunction) -> bool { return iFunction.name == iName; });
	};
	EXPECT_TRUE(has("on_create"));
	EXPECT_TRUE(has("on_update"));
}
