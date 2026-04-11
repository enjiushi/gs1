#pragma once

#include "events/engine_feedback_event.h"

#include <deque>

namespace gs1
{
using EngineFeedbackBuffer = std::deque<EngineFeedbackEvent>;
}  // namespace gs1
