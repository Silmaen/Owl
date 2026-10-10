/**
 * @file IconFont.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

/**
 * @brief
 *  UTF-8 glyphs of the Lucide icon font (0.460.0, ISC licence), merged into every UI font by `UiLayer`.
 *
 * Use them inside any ImGui label, e.g. `ImGui::Button(icon::save)` or `std::format("{} Save", icon::save)`; the
 * glyph takes the size and colour of the text around it. Font in `engine_assets/fonts/lucide/`, code points from the
 * `info.json` of the `lucide-static` package.
 */
namespace owl::gui::icon {
constexpr auto arrowDown = "\xee\x81\x86";///< Lucide `arrow-down`.
constexpr auto arrowLeft = "\xee\x81\x8c";///< Lucide `arrow-left`.
constexpr auto arrowRight = "\xee\x81\x8d";///< Lucide `arrow-right`.
constexpr auto arrowUp = "\xee\x81\x8e";///< Lucide `arrow-up`.
constexpr auto atom = "\xee\x8f\x9b";///< Lucide `atom`.
constexpr auto bell = "\xee\x81\x9d";///< Lucide `bell`.
constexpr auto bookOpen = "\xee\x81\xa3";///< Lucide `book-open`.
constexpr auto bookmarkPlus = "\xee\x88\xbd";///< Lucide `bookmark-plus`.
constexpr auto box = "\xee\x81\xa5";///< Lucide `box`.
constexpr auto boxes = "\xee\x8b\x90";///< Lucide `boxes`.
constexpr auto camera = "\xee\x81\xa8";///< Lucide `camera`.
constexpr auto check = "\xee\x81\xb0";///< Lucide `check`.
constexpr auto chevronDown = "\xee\x81\xb1";///< Lucide `chevron-down`.
constexpr auto chevronLeft = "\xee\x81\xb2";///< Lucide `chevron-left`.
constexpr auto chevronRight = "\xee\x81\xb3";///< Lucide `chevron-right`.
constexpr auto chevronUp = "\xee\x81\xb4";///< Lucide `chevron-up`.
constexpr auto circleCheck = "\xee\x88\xa6";///< Lucide `circle-check`.
constexpr auto circleHelp = "\xee\x82\x82";///< Lucide `circle-help`.
constexpr auto circleX = "\xee\x82\x88";///< Lucide `circle-x`.
constexpr auto clapperboard = "\xee\x8a\x9b";///< Lucide `clapperboard`.
constexpr auto clipboardCopy = "\xee\x88\xa5";///< Lucide `clipboard-copy`.
constexpr auto clipboardPaste = "\xee\x8f\xac";///< Lucide `clipboard-paste`.
constexpr auto code = "\xee\x82\x97";///< Lucide `code`.
constexpr auto copy = "\xee\x82\xa2";///< Lucide `copy`.
constexpr auto download = "\xee\x82\xb6";///< Lucide `download`.
constexpr auto ellipsis = "\xee\x82\xba";///< Lucide `ellipsis`.
constexpr auto ellipsisVertical = "\xee\x82\xbb";///< Lucide `ellipsis-vertical`.
constexpr auto eraser = "\xee\x8a\x8f";///< Lucide `eraser`.
constexpr auto externalLink = "\xee\x82\xbd";///< Lucide `external-link`.
constexpr auto eye = "\xee\x82\xbe";///< Lucide `eye`.
constexpr auto eyeOff = "\xee\x82\xbf";///< Lucide `eye-off`.
constexpr auto file = "\xee\x83\x84";///< Lucide `file`.
constexpr auto fileCode2 = "\xee\x91\xa3";///< Lucide `file-code-2`.
constexpr auto filePlus = "\xee\x83\x8d";///< Lucide `file-plus`.
constexpr auto film = "\xee\x83\x94";///< Lucide `film`.
constexpr auto filter = "\xee\x83\x95";///< Lucide `filter`.
constexpr auto focus = "\xee\x8a\x9e";///< Lucide `focus`.
constexpr auto folder = "\xee\x83\x9c";///< Lucide `folder`.
constexpr auto folderOpen = "\xee\x89\x87";///< Lucide `folder-open`.
constexpr auto folderPlus = "\xee\x83\x9e";///< Lucide `folder-plus`.
constexpr auto folderTree = "\xee\x8d\x80";///< Lucide `folder-tree`.
constexpr auto gauge = "\xee\x86\xbf";///< Lucide `gauge`.
constexpr auto globe = "\xee\x83\xab";///< Lucide `globe`.
constexpr auto grid3x3 = "\xee\x83\xac";///< Lucide `grid-3x3`.
constexpr auto hash = "\xee\x83\xb2";///< Lucide `hash`.
constexpr auto history = "\xee\x87\xb5";///< Lucide `history`.
constexpr auto house = "\xee\x83\xb8";///< Lucide `house`.
constexpr auto image = "\xee\x83\xb9";///< Lucide `image`.
constexpr auto info = "\xee\x83\xbe";///< Lucide `info`.
constexpr auto keyboard = "\xee\x8a\x84";///< Lucide `keyboard`.
constexpr auto layers = "\xee\x84\x84";///< Lucide `layers`.
constexpr auto layoutDashboard = "\xee\x87\x81";///< Lucide `layout-dashboard`.
constexpr auto layoutGrid = "\xee\x84\x85";///< Lucide `layout-grid`.
constexpr auto link = "\xee\x84\x88";///< Lucide `link`.
constexpr auto list = "\xee\x84\x8c";///< Lucide `list`.
constexpr auto listFilter = "\xee\x91\xa5";///< Lucide `list-filter`.
constexpr auto listTree = "\xee\x90\x8d";///< Lucide `list-tree`.
constexpr auto lock = "\xee\x84\x8f";///< Lucide `lock`.
constexpr auto lockOpen = "\xee\x84\x90";///< Lucide `lock-open`.
constexpr auto logOut = "\xee\x84\x92";///< Lucide `log-out`.
constexpr auto magnet = "\xee\x8a\xb5";///< Lucide `magnet`.
constexpr auto maximize2 = "\xee\x84\x97";///< Lucide `maximize-2`.
constexpr auto minimize2 = "\xee\x84\x9f";///< Lucide `minimize-2`.
constexpr auto minus = "\xee\x84\xa0";///< Lucide `minus`.
constexpr auto mousePointer2 = "\xee\x87\x83";///< Lucide `mouse-pointer-2`.
constexpr auto move = "\xee\x84\xa5";///< Lucide `move`.
constexpr auto move3d = "\xee\x8b\xa5";///< Lucide `move-3d`.
constexpr auto music = "\xee\x84\xa6";///< Lucide `music`.
constexpr auto package = "\xee\x84\xad";///< Lucide `package`.
constexpr auto packageCheck = "\xee\x89\xa6";///< Lucide `package-check`.
constexpr auto packagePlus = "\xee\x89\xa8";///< Lucide `package-plus`.
constexpr auto paintBucket = "\xee\x8b\xa6";///< Lucide `paint-bucket`.
constexpr auto paintbrush = "\xee\x8b\xa7";///< Lucide `paintbrush`.
constexpr auto panelLeft = "\xee\x84\xae";///< Lucide `panel-left`.
constexpr auto panelRight = "\xee\x90\xb6";///< Lucide `panel-right`.
constexpr auto pause = "\xee\x84\xb2";///< Lucide `pause`.
constexpr auto pencil = "\xee\x87\xb9";///< Lucide `pencil`.
constexpr auto pipette = "\xee\x84\xbf";///< Lucide `pipette`.
constexpr auto play = "\xee\x85\x80";///< Lucide `play`.
constexpr auto plus = "\xee\x85\x81";///< Lucide `plus`.
constexpr auto redo2 = "\xee\x8a\xa0";///< Lucide `redo-2`.
constexpr auto refreshCw = "\xee\x85\x89";///< Lucide `refresh-cw`.
constexpr auto rocket = "\xee\x8a\x86";///< Lucide `rocket`.
constexpr auto rotate3d = "\xee\x8b\xaa";///< Lucide `rotate-3d`.
constexpr auto rotateCcw = "\xee\x85\x8c";///< Lucide `rotate-ccw`.
constexpr auto save = "\xee\x85\x91";///< Lucide `save`.
constexpr auto saveAll = "\xee\x90\x94";///< Lucide `save-all`.
constexpr auto scale3d = "\xee\x8b\xab";///< Lucide `scale-3d`.
constexpr auto scan = "\xee\x89\x97";///< Lucide `scan`.
constexpr auto scissors = "\xee\x85\x92";///< Lucide `scissors`.
constexpr auto scrollText = "\xee\x91\xa4";///< Lucide `scroll-text`.
constexpr auto search = "\xee\x85\x95";///< Lucide `search`.
constexpr auto settings = "\xee\x85\x98";///< Lucide `settings`.
constexpr auto settings2 = "\xee\x89\x85";///< Lucide `settings-2`.
constexpr auto skipForward = "\xee\x85\xa4";///< Lucide `skip-forward`.
constexpr auto slidersHorizontal = "\xee\x8a\x9a";///< Lucide `sliders-horizontal`.
constexpr auto square = "\xee\x85\xab";///< Lucide `square`.
constexpr auto squareDashed = "\xee\x87\x8b";///< Lucide `square-dashed`.
constexpr auto tag = "\xee\x86\x83";///< Lucide `tag`.
constexpr auto terminal = "\xee\x86\x85";///< Lucide `terminal`.
constexpr auto trash2 = "\xee\x86\x8e";///< Lucide `trash-2`.
constexpr auto triangleAlert = "\xee\x86\x93";///< Lucide `triangle-alert`.
constexpr auto type = "\xee\x86\x98";///< Lucide `type`.
constexpr auto undo2 = "\xee\x8a\xa1";///< Lucide `undo-2`.
constexpr auto unlink = "\xee\x86\x9c";///< Lucide `unlink`.
constexpr auto upload = "\xee\x86\x9e";///< Lucide `upload`.
constexpr auto user = "\xee\x86\x9f";///< Lucide `user`.
constexpr auto video = "\xee\x86\xa5";///< Lucide `video`.
constexpr auto volume2 = "\xee\x86\xab";///< Lucide `volume-2`.
constexpr auto workflow = "\xee\x90\xaa";///< Lucide `workflow`.
constexpr auto x = "\xee\x86\xb2";///< Lucide `x`.
constexpr auto zap = "\xee\x86\xb4";///< Lucide `zap`.
}// namespace owl::gui::icon
