#pragma once

#include <string>

#include "map_catalog.h"

namespace BrigSession
{
enum class ConnectionType
{
    Local,
    ListenServer,
    DedicatedServer
};

// The single authoritative set of settings for a future or active session.
// UI pages read and update this object; they must not keep independent copies.
struct SessionConfiguration
{
    BrigMapCatalog::GameMode mode = BrigMapCatalog::GameMode::SinglePlayer;
    ConnectionType connectionType = ConnectionType::Local;
    std::string mapId;
    std::string serverName = "BRIG RPG 2";
    std::string password;
    int maxPlayers = 1;
    bool lan = true;
    bool allowCheats = false;
};

inline SessionConfiguration& GetActiveSessionConfiguration()
{
    static SessionConfiguration configuration;
    return configuration;
}

inline const char* ConnectionTypeId(ConnectionType type)
{
    switch (type)
    {
    case ConnectionType::ListenServer:
        return "listen";
    case ConnectionType::DedicatedServer:
        return "dedicated";
    case ConnectionType::Local:
    default:
        return "local";
    }
}
} // namespace BrigSession
