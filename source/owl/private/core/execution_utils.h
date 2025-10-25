/**
 * @file execution_utils.h
 * @author Silmaen
 * @date 20/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <execution>

namespace owl::core::execution_utils {

#ifdef OWL_ENGINE_USE_TBB
#define PARUNSEQ std::execution::par_unseq
#else
#define PARUNSEQ std::execution::unseq
#endif


#ifdef OWL_ENGINE_USE_TBB
#define PARSEQ std::execution::par;
#else
#define PARSEQ std::execution::seq;
#endif


}// namespace owl::core::execution_utils
