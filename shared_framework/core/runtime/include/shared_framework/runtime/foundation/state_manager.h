#pragma once

#include "shared_framework/runtime/foundation/state_catalog.h"
#include "shared_framework/runtime/foundation/system_contract_validation.h"

namespace shared_framework::runtime
{
template <typename SystemPack>
class state_manager
{
public:
    using validator = validate_system_contracts<SystemPack>;
    using catalog = runtime_state_catalog<SystemPack>;
    using storage_type = state_storage<typename catalog::descriptors>;

    static consteval void validate_contracts()
    {
        validator::validate();
    }

    template <typename Key>
    [[nodiscard]] auto& state() noexcept
    {
        return storage_.template get<Key>();
    }

    template <typename Key>
    [[nodiscard]] const auto& query() const noexcept
    {
        return storage_.template get<Key>();
    }

    [[nodiscard]] storage_type& storage() noexcept { return storage_; }
    [[nodiscard]] const storage_type& storage() const noexcept { return storage_; }

private:
    storage_type storage_ {};
};
}  // namespace shared_framework::runtime
