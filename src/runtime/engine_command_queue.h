#pragma once

#include "gs1/types.h"

#include <deque>

namespace gs1
{
using EngineCommandQueue = std::deque<Gs1EngineCommand>;
}  // namespace gs1
