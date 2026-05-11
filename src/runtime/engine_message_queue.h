#pragma once

#include "gs1/types.h"

#include <deque>

namespace gs1
{
using EngineMessageQueue = std::deque<Gs1RuntimeMessage>;
}  // namespace gs1

