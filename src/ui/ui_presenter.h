#pragma once

namespace gs1
{
class GameRuntime;
struct HudViewModel;
struct PhoneViewModel;
struct RegionalMapViewModel;

class UiPresenter final
{
public:
    explicit UiPresenter(const GameRuntime& runtime) noexcept
        : runtime_(runtime)
    {
    }

private:
    const GameRuntime& runtime_;
};
}  // namespace gs1
