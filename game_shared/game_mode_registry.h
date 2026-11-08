#pragma once

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
    static const Descriptor deathmatch{
        BrigMapCatalog::GameMode::Deathmatch, "deathmatch", false, "BRIG_Mode_Deathmatch_NextStage"};
    static const Descriptor teamDeathmatch{
        BrigMapCatalog::GameMode::TeamDeathmatch, "team_deathmatch", false, "BRIG_Mode_TeamDeathmatch_NextStage"};

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
} // namespace BrigGameModeRegistry
