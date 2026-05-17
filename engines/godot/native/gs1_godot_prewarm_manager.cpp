#include "gs1_godot_prewarm_manager.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <utility>

using namespace godot;

namespace
{
std::string path_key(const String& path)
{
    return path.utf8().get_data();
}

int retain_policy_rank(Gs1GodotPrewarmRetainPolicy policy) noexcept
{
    switch (policy)
    {
    case Gs1GodotPrewarmRetainPolicy::Manual:
        return 3;
    case Gs1GodotPrewarmRetainPolicy::UntilScreenExit:
        return 2;
    case Gs1GodotPrewarmRetainPolicy::UntilTransitionConsumed:
        return 1;
    case Gs1GodotPrewarmRetainPolicy::Inherit:
    default:
        return 0;
    }
}
}

void Gs1GodotPrewarmManager::set_current_screen(Gs1GodotDirectorScreenKind screen_kind)
{
    if (current_screen_kind_ == screen_kind)
    {
        return;
    }

    current_screen_kind_ = screen_kind;
    rebuild_active_requests();
}

void Gs1GodotPrewarmManager::poll()
{
    ResourceLoader* resource_loader = ResourceLoader::get_singleton();
    if (resource_loader == nullptr)
    {
        return;
    }

    for (auto& [key, target] : cached_targets_)
    {
        if (!target.threaded_request_started || target.load_failed || !target.retained_resource.is_null())
        {
            continue;
        }

        const ResourceLoader::ThreadLoadStatus status = resource_loader->load_threaded_get_status(target.path);
        if (status == ResourceLoader::THREAD_LOAD_IN_PROGRESS)
        {
            continue;
        }

        target.threaded_request_started = false;
        if (status == ResourceLoader::THREAD_LOAD_LOADED)
        {
            target.retained_resource = resource_loader->load_threaded_get(target.path);
            continue;
        }

        target.load_failed = true;
        UtilityFunctions::push_warning(vformat(
            "GS1 Godot prewarm failed for %s with status %d",
            target.path,
            static_cast<int>(status)));
    }
}

void Gs1GodotPrewarmManager::clear() noexcept
{
    current_screen_kind_ = GS1_GODOT_SCREEN_KIND_NONE;
    cached_targets_.clear();
}

Ref<PackedScene> Gs1GodotPrewarmManager::find_retained_packed_scene(const String& path) const
{
    const auto found = cached_targets_.find(path_key(path));
    if (found == cached_targets_.end())
    {
        return Ref<PackedScene> {};
    }

    Ref<PackedScene> packed_scene = found->second.retained_resource;
    return packed_scene;
}

void Gs1GodotPrewarmManager::rebuild_active_requests()
{
    std::unordered_map<std::string, CachedTarget> desired_targets {};
    if (current_screen_kind_ != GS1_GODOT_SCREEN_KIND_NONE &&
        current_screen_kind_ != GS1_GODOT_SCREEN_KIND_LOADING)
    {
        const auto& transitions = Gs1GodotPrewarmConfig::instance().transitions_from(current_screen_kind_);
        for (const Gs1GodotPrewarmTransitionDef& transition : transitions)
        {
            for (const Gs1GodotPrewarmTargetDef& target_def : transition.targets)
            {
                if (target_def.path.empty())
                {
                    continue;
                }

                const String path = String::utf8(target_def.path.c_str());
                CachedTarget& desired = desired_targets[path_key(path)];
                if (desired.path.is_empty())
                {
                    desired.kind = target_def.kind;
                    desired.retain_policy = target_def.retain_policy;
                    desired.path = path;
                    desired.resource_type = String::utf8(target_def.resource_type.c_str());
                    desired.manual_retained = target_def.retain_policy == Gs1GodotPrewarmRetainPolicy::Manual;
                }
                else
                {
                    if (retain_policy_rank(target_def.retain_policy) > retain_policy_rank(desired.retain_policy))
                    {
                        desired.retain_policy = target_def.retain_policy;
                    }
                    desired.manual_retained = desired.manual_retained ||
                        target_def.retain_policy == Gs1GodotPrewarmRetainPolicy::Manual;
                    if (desired.resource_type.is_empty() && !target_def.resource_type.empty())
                    {
                        desired.resource_type = String::utf8(target_def.resource_type.c_str());
                    }
                }
            }
        }
    }

    for (auto it = cached_targets_.begin(); it != cached_targets_.end();)
    {
        const auto desired = desired_targets.find(it->first);
        if (desired != desired_targets.end())
        {
            desired->second.retained_resource = it->second.retained_resource;
            desired->second.threaded_request_started = it->second.threaded_request_started;
            desired->second.load_failed = it->second.load_failed;
            ++it;
            continue;
        }

        if (it->second.manual_retained)
        {
            desired_targets.insert_or_assign(it->first, it->second);
            ++it;
            continue;
        }

        it = cached_targets_.erase(it);
    }

    cached_targets_ = std::move(desired_targets);

    for (auto& [key, target] : cached_targets_)
    {
        if (!target.retained_resource.is_null() || target.threaded_request_started || target.load_failed)
        {
            continue;
        }

        start_threaded_request(target);
    }
}

void Gs1GodotPrewarmManager::start_threaded_request(CachedTarget& target)
{
    ResourceLoader* resource_loader = ResourceLoader::get_singleton();
    if (resource_loader == nullptr || target.path.is_empty())
    {
        return;
    }

    const Error status = resource_loader->load_threaded_request(
        target.path,
        target.resource_type,
        true,
        ResourceLoader::CACHE_MODE_REUSE);
    if (status == OK || status == ERR_BUSY)
    {
        target.threaded_request_started = true;
        target.load_failed = false;
        return;
    }

    target.load_failed = true;
    UtilityFunctions::push_warning(vformat(
        "GS1 Godot prewarm could not queue %s",
        target.path));
}
