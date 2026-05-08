#pragma once

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref.hpp>

#include <algorithm>
#include <vector>

class IGs1GodotInputSubscriber
{
public:
    virtual ~IGs1GodotInputSubscriber() = default;
    virtual void handle_input_event(const godot::Ref<godot::InputEvent>& event) = 0;
};

class Gs1GodotInputDispatcher final
{
public:
    void subscribe(IGs1GodotInputSubscriber& subscriber)
    {
        if (std::find(subscribers_.begin(), subscribers_.end(), &subscriber) == subscribers_.end())
        {
            subscribers_.push_back(&subscriber);
        }
    }

    void unsubscribe(IGs1GodotInputSubscriber& subscriber)
    {
        subscribers_.erase(
            std::remove(subscribers_.begin(), subscribers_.end(), &subscriber),
            subscribers_.end());
    }

    void dispatch(const godot::Ref<godot::InputEvent>& event) const
    {
        const auto subscribers = subscribers_;
        for (IGs1GodotInputSubscriber* subscriber : subscribers)
        {
            if (subscriber != nullptr)
            {
                subscriber->handle_input_event(event);
            }
        }
    }

private:
    std::vector<IGs1GodotInputSubscriber*> subscribers_ {};
};
