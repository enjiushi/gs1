#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <vector>

namespace gs1
{
enum class PhoneListingKind : std::uint32_t
{
    BuyItem = 0,
    SellItem = 1,
    HireContractor = 2,
    PurchaseUnlockable = 3
};

struct PhoneListingState final
{
    PhoneListingKind kind {PhoneListingKind::BuyItem};
    ItemId item_id {};
    std::uint32_t listing_id {0};
    std::int32_t price {0};
    std::uint32_t quantity {0};
    bool occupied {false};
};

struct EconomyState final
{
    std::int32_t money {0};
    std::vector<std::uint32_t> revealed_site_unlockable_ids {};
    std::vector<std::uint32_t> direct_purchase_unlockable_ids {};
    std::vector<PhoneListingState> available_phone_listings {};
};
}  // namespace gs1
