/**
 * @file Document.cpp
 * @author Silmaen
 * @date 18/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Document.h"

namespace owl::nest {
// Out-of-line anchor for the Document vtable (avoids -Wweak-vtables).
// Empty on purpose: the default destructor lives here, not inline in the header.
// NOLINTNEXTLINE(modernize-use-equals-default)
Document::~Document() {}

}// namespace owl::nest
