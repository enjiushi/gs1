#pragma once

namespace gs1
{
class GameRuntime;

class GameApp final
{
public:
    explicit GameApp(GameRuntime& runtime) noexcept
        : runtime_(runtime)
    {
    }

private:
    GameRuntime& runtime_;
};
}  // namespace gs1
