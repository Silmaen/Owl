/**
 * @file stb_image_implem.cpp
 * @author Silmaen
 * @date 10/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

// GCC 15 miscompiles the flat HDR scanline loop of stb_image at -O2 and above (later rows read as black).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("no-expensive-optimizations")
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
