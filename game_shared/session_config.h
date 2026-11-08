#pragma once

#include <algorithm>
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
    std::string serverAddress;
    std::string serverName = "BRIG RPG 2";
    std::string password;
    int maxPlayers = 1;
    bool lan = true;
    bool allowCheats = false;
    int difficulty = 2; // GoldSrc skill levels: 1 easy, 2 normal, 3 hard.

    // Legacy multiplayer server rules, applied only to listen-server sessions.
    int timeLimitMinutes = 20;
    int fragLimit = 0;
    bool friendlyFire = false;
    bool fallDamage = false;
    bool weaponStay = false;
    bool flashlight = true;
    bool allowMonsters = false;
    bool allowSpectators = false;
};

// Co-op is campaign-driven: competitive time/frag limits are disabled and
// campaign monsters cannot be switched off through the generic session UI.
inline int EffectiveTimeLimitMinutes(BrigMapCatalog::GameMode mode, int requested)
{
    return mode == BrigMapCatalog::GameMode::CoOp
        ? 0 : std::max(0, std::min(requested, 120));
}

inline int EffectiveFragLimit(BrigMapCatalog::GameMode mode, int requested)
{
    return mode == BrigMapCatalog::GameMode::CoOp
        ? 0 : std::max(0, std::min(requested, 1000));
}

inline bool EffectiveAllowMonsters(BrigMapCatalog::GameMode mode, bool requested)
{
    return mode == BrigMapCatalog::GameMode::CoOp || requested;
}

// Validate a classic GoldSrc hostname or IPv4 address, optionally with a
// decimal port. Delimiters, whitespace and IPv6 literals are rejected because
// this Legacy connect path is intentionally limited to the supported syntax.
inline bool IsValidServerAddress(const std::string& address)
{
    if (address.empty() || address.size() > 64)
        return false;

    const std::size_t colon = address.find(':');
    if (colon != std::string::npos)
    {
        if (colon == 0 || colon + 1 >= address.size() ||
            address.find(':', colon + 1) != std::string::npos)
            return false;

        unsigned int port = 0;
        for (std::size_t i = colon + 1; i < address.size(); ++i)
        {
            const char c = address[i];
            if (c < '0' || c > '9')
                return false;
            port = port * 10u + static_cast<unsigned int>(c - '0');
            if (port > 65535u)
                return false;
        }
        if (port == 0)
            return false;
    }

    const std::size_t hostLength = colon == std::string::npos ? address.size() : colon;
    bool hasAlphaNumeric = false;
    for (std::size_t i = 0; i < hostLength; ++i)
    {
        const unsigned char c = static_cast<unsigned char>(address[i]);
        const bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        const bool digit = c >= '0' && c <= '9';
        if (!alpha && !digit && c != '.' && c != '-')
            return false;
        hasAlphaNumeric = hasAlphaNumeric || alpha || digit;
    }

    return hasAlphaNumeric;
}

inline SessionConfiguration& GetActiveSessionConfiguration()
{
    static SessionConfiguration configuration;
    return configuration;
}

inline int NormalizeDifficulty(int difficulty)
{
    return difficulty < 1 ? 1 : (difficulty > 3 ? 3 : difficulty);
}

inline int NextDifficulty(int difficulty)
{
    return NormalizeDifficulty(difficulty) % 3 + 1;
}

inline const char* DifficultyName(int difficulty)
{
    switch (NormalizeDifficulty(difficulty))
    {
    case 1: return "Easy";
    case 3: return "Hard";
    case 2:
    default: return "Normal";
    }
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
