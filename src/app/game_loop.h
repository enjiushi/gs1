#pragma once

namespace gs1
{
class GameRuntime;

class GameLoop final
{
public:
    explicit GameLoop(GameRuntime& runtime) noexcept
        : runtime_(runtime)
    {
    }

private:
    GameRuntime& runtime_;
};
}  // namespace gs1
