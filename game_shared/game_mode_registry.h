#pragma once

#include <string>
#include <vector>

#include "map_catalog.h"

namespace BrigGameModeRegistry
{
struct Descriptor
{
    BrigMapCatalog::GameMode mode;
    const char* id;
    bool gameplayReady;
    const char* statusToken;
};

inline const Descriptor& GetDescriptor(BrigMapCatalog::GameMode mode)
{
    static const Descriptor singlePlayer{
        BrigMapCatalog::GameMode::SinglePlayer, "singleplayer", true, "BRIG_Mode_Ready"};
    static const Descriptor coop{
        BrigMapCatalog::GameMode::CoOp, "coop", false, "BRIG_Mode_Coop_Incomplete"};
    // BRIG already has the original Half-Life multiplayer and teamplay rules.
    // Create Game now configures them and validates maps through the shared catalog.
    // Co-op remains gated until its NPC/scripted-map and Legacy runtime audit is done.
    static const Descriptor deathmatch{
        BrigMapCatalog::GameMode::Deathmatch, "deathmatch", true, "BRIG_Mode_Ready"};
    static const Descriptor teamDeathmatch{
        BrigMapCatalog::GameMode::TeamDeathmatch, "team_deathmatch", true, "BRIG_Mode_Ready"};

    switch (mode)
    {
    case BrigMapCatalog::GameMode::CoOp:
        return coop;
    case BrigMapCatalog::GameMode::Deathmatch:
        return deathmatch;
    case BrigMapCatalog::GameMode::TeamDeathmatch:
        return teamDeathmatch;
    case BrigMapCatalog::GameMode::SinglePlayer:
    default:
        return singlePlayer;
    }
}

inline bool IsGameplayReady(BrigMapCatalog::GameMode mode)
{
    return GetDescriptor(mode).gameplayReady;
}

inline bool IsUnsupportedExpansion(const BrigMapCatalog::MapMetadata& map)
{
    const std::string category = BrigMapCatalog::ToLowerAscii(map.category);
    const std::string campaign = BrigMapCatalog::ToLowerAscii(map.campaignId);
    return category == "blue_shift" || category == "opposing_force" ||
        campaign == "blue_shift" || campaign == "opposing_force";
}

inline bool CanLaunchMap(
    const std::vector<BrigMapCatalog::MapMetadata>& maps,
    const std::string& mapId,
    BrigMapCatalog::GameMode mode)
{
    if (!IsGameplayReady(mode) || !BrigMapCatalog::IsValidMapId(mapId))
        return false;

    const std::string normalizedId = BrigMapCatalog::ToLowerAscii(mapId);
    for (const auto& map : maps)
    {
        if (map.name != normalizedId)
            continue;

        if (!BrigMapCatalog::IsModeAllowed(map, mode))
            return false;

        // The expansion category is metadata-only until its campaign assets,
        // entities and runtime behavior have been audited and implemented.
        return mode != BrigMapCatalog::GameMode::SinglePlayer ||
            !IsUnsupportedExpansion(map);
    }

    return false;
}
} // namespace BrigGameModeRegistry
