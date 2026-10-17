/**
 * @file SlangSyntax.h
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::codeEditor {

/**
 * @brief
 *  Class of a word of a Slang shader, as the editor colours it.
 */
enum struct SlangWordKind : uint8_t {
	None,///< An ordinary identifier.
	Keyword,///< A reserved word or a storage / parameter qualifier (`struct`, `import`, `uniform`...).
	Type,///< A scalar, vector, matrix, resource or sampler type (`float4`, `Texture2D`...).
	Semantic,///< A system-value or vertex semantic (`SV_Position`, `TEXCOORD0`...).
	Builtin,///< An intrinsic function (`mul`, `saturate`, `Sample`...).
};

/**
 * @brief
 *  Tell the class of a word.
 * @param[in] iWord The word, case-sensitive.
 * @return The class, `None` for any other word.
 */
[[nodiscard]] auto classifySlangWord(std::string_view iWord) -> SlangWordKind;

/**
 * @brief
 *  List the words of a class, the numbered semantics (`TEXCOORD0` to `TEXCOORD7`...) written out.
 * @param[in] iKind The class (`None` gives nothing).
 * @return The words.
 */
[[nodiscard]] auto slangWords(SlangWordKind iKind) -> std::vector<std::string>;

}// namespace owl::nest::codeEditor
