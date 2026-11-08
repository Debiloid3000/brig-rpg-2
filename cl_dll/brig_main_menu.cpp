#include "brig_main_menu.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

#include <VGUI_TextPanel.h>
#include <VGUI_App.h>
#include <VGUI_KeyCode.h>
#include <VGUI_Scheme.h>

#include "cl_util.h"
#include "campaign_catalog.h"
#include "filesystem_utils.h"
#include "game_mode_registry.h"
#include "session_config.h"
#include "hud.h"
#include "keydefs.h"
#include "vgui_int.h"
#include "net_api.h"
#include <cstdint>
#include <cstdlib>

namespace
{
struct LanBrowserServer
{
    std::string address;
    std::string hostname;
    std::string map;
    int players = 0;
    int maxPlayers = 0;
    int pingMilliseconds = -1;
};

struct LanBrowserState
{
    std::vector<LanBrowserServer> servers;
    int requestContext = 0;
    int nextRequestContext = 7000;
    bool querying = false;
    std::string status = "Press Refresh LAN to search your local network.";
};

LanBrowserState g_lanBrowser;
CBrigMainMenuPanel* g_pBrigMainMenuPanel = nullptr;

unsigned short HostToNetworkPort(unsigned short port)
{
    const std::uint16_t endianProbe = 1;
    const bool isLittleEndian =
        *reinterpret_cast<const unsigned char*>(&endianProbe) == 1;
    if (!isLittleEndian)
        return port;

    return static_cast<unsigned short>((port << 8) | (port >> 8));
}

std::string BrowserValue(const char* response, const char* key, const char* fallback)
{
    if (!response || !key || !gEngfuncs.pNetAPI ||
        !gEngfuncs.pNetAPI->ValueForKey)
        return fallback ? fallback : "";

    const char* value = gEngfuncs.pNetAPI->ValueForKey(response, key);
    return value && *value ? value : (fallback ? fallback : "");
}

std::string TruncateBrowserLabel(const std::string& value, std::size_t maxLength)
{
    if (value.size() <= maxLength)
        return value;

    return value.substr(0, maxLength > 3 ? maxLength - 3 : maxLength) + "...";
}

void NotifyLanBrowserChanged()
{
    if (g_pBrigMainMenuPanel)
        g_pBrigMainMenuPanel->ServerBrowserDataChanged();
}

void LanServerResponse(net_response_s* response)
{
    if (!response || response->context != g_lanBrowser.requestContext)
        return;

    if (response->error != NET_SUCCESS)
    {
        g_lanBrowser.querying = false;
        if (g_lanBrowser.servers.empty())
        {
            g_lanBrowser.status = (response->error & NET_ERROR_TIMEOUT) != 0
                ? "No LAN servers found."
                : "LAN search finished without a server response.";
        }
        else
        {
            g_lanBrowser.status = "Found " +
                std::to_string(g_lanBrowser.servers.size()) + " LAN server(s).";
        }
        NotifyLanBrowserChanged();
        return;
    }

    if (response->type != NETAPI_REQUEST_DETAILS || !response->response)
        return;

    // NETAPI response memory is owned by the engine and is released as soon
    // as this callback returns. Copy every field before leaving this function.
    const char* responseText = static_cast<const char*>(response->response);
    char* addressText = gEngfuncs.pNetAPI && gEngfuncs.pNetAPI->AdrToString
        ? gEngfuncs.pNetAPI->AdrToString(&response->remote_address)
        : nullptr;

    if (!addressText || !*addressText)
        return;

    LanBrowserServer server;
    server.address = addressText;
    server.hostname = BrowserValue(responseText, "hostname", server.address.c_str());
    server.map = BrowserValue(responseText, "map", "Unknown map");
    server.players = std::max(0, std::atoi(BrowserValue(responseText, "players", "0").c_str()));
    server.maxPlayers = std::max(0, std::atoi(BrowserValue(responseText, "max", "0").c_str()));
    server.pingMilliseconds = static_cast<int>(response->ping * 1000.0);

    const auto existing = std::find_if(
        g_lanBrowser.servers.begin(), g_lanBrowser.servers.end(),
        [&](const LanBrowserServer& item) { return item.address == server.address; });

    if (existing == g_lanBrowser.servers.end())
        g_lanBrowser.servers.push_back(server);
    else
        *existing = server;

    std::sort(g_lanBrowser.servers.begin(), g_lanBrowser.servers.end(),
        [](const LanBrowserServer& left, const LanBrowserServer& right)
        {
            if (left.pingMilliseconds != right.pingMilliseconds)
                return left.pingMilliseconds < right.pingMilliseconds;
            return left.hostname < right.hostname;
        });

    g_lanBrowser.status = "Searching LAN... found " +
        std::to_string(g_lanBrowser.servers.size()) + " server(s).";
    NotifyLanBrowserChanged();
}

bool StartLanServerSearch()
{
    net_api_t* api = gEngfuncs.pNetAPI;
    if (!api || !api->InitNetworking || !api->SendRequest)
    {
        g_lanBrowser.querying = false;
        g_lanBrowser.status = "This engine does not expose the Legacy server-query API.";
        NotifyLanBrowserChanged();
        return false;
    }

    // Change the context before cancelling so a synchronous or late
    // callback from the previous request cannot overwrite this search state.
    const int previousContext = g_lanBrowser.requestContext;
    const bool hadActiveQuery = g_lanBrowser.querying;
    g_lanBrowser.querying = false;
    g_lanBrowser.requestContext = g_lanBrowser.nextRequestContext++;
    if (g_lanBrowser.nextRequestContext < 7000)
        g_lanBrowser.nextRequestContext = 7000;

    if (hadActiveQuery && api->CancelRequest)
        api->CancelRequest(previousContext);

    g_lanBrowser.servers.clear();
    g_lanBrowser.status = "Searching local network for GoldSrc servers...";
    g_lanBrowser.querying = true;

    api->InitNetworking();

    netadr_t broadcastAddress{};
    broadcastAddress.type = NA_BROADCAST;
    broadcastAddress.port = HostToNetworkPort(27015);

    api->SendRequest(
        g_lanBrowser.requestContext,
        NETAPI_REQUEST_DETAILS,
        FNETAPI_MULTIPLE_RESPONSE,
        5.0,
        &broadcastAddress,
        LanServerResponse);

    NotifyLanBrowserChanged();
    return true;
}

bool GoldSrcKeyToVGuiKey(int keynum, KeyCode& code, bool& printable)
{
    printable = false;

    if (keynum >= '0' && keynum <= '9')
    {
        code = static_cast<KeyCode>(KEY_0 + keynum - '0');
        printable = true;
        return true;
    }

    if (keynum >= 'a' && keynum <= 'z')
    {
        code = static_cast<KeyCode>(KEY_A + keynum - 'a');
        printable = true;
        return true;
    }

    if (keynum >= 'A' && keynum <= 'Z')
    {
        code = static_cast<KeyCode>(KEY_A + keynum - 'A');
        printable = true;
        return true;
    }

    switch (keynum)
    {
    case 32: code = KEY_SPACE; printable = true; return true;
    case '-': code = KEY_MINUS; printable = true; return true;
    case '=': code = KEY_EQUAL; printable = true; return true;
    case '[': code = KEY_LBRACKET; printable = true; return true;
    case ']': code = KEY_RBRACKET; printable = true; return true;
    case ';': code = KEY_SEMICOLON; printable = true; return true;
    case 39: code = KEY_APOSTROPHE; printable = true; return true;
    case 96: code = KEY_BACKQUOTE; printable = true; return true;
    case ',': code = KEY_COMMA; printable = true; return true;
    case '.': code = KEY_PERIOD; printable = true; return true;
    case '/': code = KEY_SLASH; printable = true; return true;
    case 92: code = KEY_BACKSLASH; printable = true; return true;
    case K_TAB: code = KEY_TAB; return true;
    case K_ENTER: code = KEY_ENTER; return true;
    case K_KP_ENTER: code = KEY_PAD_ENTER; return true;
    case K_ESCAPE: code = KEY_ESCAPE; return true;
    case K_BACKSPACE: code = KEY_BACKSPACE; return true;
    case K_UPARROW: code = KEY_UP; return true;
    case K_DOWNARROW: code = KEY_DOWN; return true;
    case K_LEFTARROW: code = KEY_LEFT; return true;
    case K_RIGHTARROW: code = KEY_RIGHT; return true;
    case K_SHIFT: code = KEY_LSHIFT; return true;
    case K_CTRL: code = KEY_LCONTROL; return true;
    case K_ALT: code = KEY_LALT; return true;
    case K_F1: code = KEY_F1; return true;
    case K_F2: code = KEY_F2; return true;
    case K_F3: code = KEY_F3; return true;
    case K_F4: code = KEY_F4; return true;
    case K_F5: code = KEY_F5; return true;
    case K_F6: code = KEY_F6; return true;
    case K_F7: code = KEY_F7; return true;
    case K_F8: code = KEY_F8; return true;
    case K_F9: code = KEY_F9; return true;
    case K_F10: code = KEY_F10; return true;
    case K_F11: code = KEY_F11; return true;
    case K_F12: code = KEY_F12; return true;
    case K_INS: code = KEY_INSERT; return true;
    case K_DEL: code = KEY_DELETE; return true;
    case K_PGUP: code = KEY_PAGEUP; return true;
    case K_PGDN: code = KEY_PAGEDOWN; return true;
    case K_HOME: code = KEY_HOME; return true;
    case K_END: code = KEY_END; return true;
    case K_KP_MINUS: code = KEY_PAD_MINUS; printable = true; return true;
    case K_KP_PLUS: code = KEY_PAD_PLUS; printable = true; return true;
    case K_KP_MUL: code = KEY_PAD_MULTIPLY; printable = true; return true;
    case K_KP_SLASH: code = KEY_PAD_DIVIDE; printable = true; return true;
    case K_KP_DEL: code = KEY_PAD_DECIMAL; printable = true; return true;
    case K_KP_INS: code = KEY_PAD_0; printable = true; return true;
    case K_KP_END: code = KEY_PAD_1; printable = true; return true;
    case K_KP_DOWNARROW: code = KEY_PAD_2; printable = true; return true;
    case K_KP_PGDN: code = KEY_PAD_3; printable = true; return true;
    case K_KP_LEFTARROW: code = KEY_PAD_4; printable = true; return true;
    case K_KP_5: code = KEY_PAD_5; printable = true; return true;
    case K_KP_RIGHTARROW: code = KEY_PAD_6; printable = true; return true;
    case K_KP_HOME: code = KEY_PAD_7; printable = true; return true;
    case K_KP_UPARROW: code = KEY_PAD_8; printable = true; return true;
    case K_KP_PGUP: code = KEY_PAD_9; printable = true; return true;
    case K_CAPSLOCK: code = KEY_CAPSLOCK; return true;
    default:
        return false;
    }
}

class CBrigMenuButtonSignal final : public ActionSignal
{
public:
    CBrigMenuButtonSignal(CBrigMainMenuPanel* menu, int slot)
        : m_pMenu(menu), m_iSlot(slot)
    {
    }

    void actionPerformed(Panel*) override
    {
        if (m_pMenu)
            m_pMenu->ActivateButton(m_iSlot);
    }

private:
    CBrigMainMenuPanel* m_pMenu;
    int m_iSlot;
};

bool ReadBoundedTextFile(const char* path, std::size_t maximumBytes, std::string& output)
{
    output.clear();

    if (!g_pFileSystem || !path)
        return false;

    FSFile file(path, "rb");
    if (!file || file.Size() == 0 || file.Size() > maximumBytes)
        return false;

    const std::size_t size = file.Size();
    output.resize(size);
    return file.Read(&output[0], static_cast<int>(size)) == static_cast<int>(size);
}

std::string DecodeQuotedValue(const std::string& value)
{
    std::string decoded;
    decoded.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '\\' && i + 1 < value.size())
        {
            const char next = value[++i];
            if (next == 'n')
                decoded.push_back('\n');
            else if (next == 't')
                decoded.push_back('\t');
            else
                decoded.push_back(next);
        }
        else
        {
            decoded.push_back(value[i]);
        }
    }

    return decoded;
}

bool IsRussianClientLanguage()
{
    const char* candidates[] = {
        CVAR_GET_STRING("lang"),
        CVAR_GET_STRING("language"),
        CVAR_GET_STRING("cl_language")
    };

    for (const char* candidate : candidates)
    {
        if (!candidate || !*candidate)
            continue;

        std::string language(candidate);
        std::transform(language.begin(), language.end(), language.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (language == "russian" || language == "ru")
            return true;
        if (language == "english" || language == "en")
            return false;
    }

    return false;
}

const std::unordered_map<std::string, std::string>& MenuTokens()
{
    static const std::unordered_map<std::string, std::string> tokens = []()
    {
        std::unordered_map<std::string, std::string> result;
        std::string contents;
        const char* path = IsRussianClientLanguage()
            ? "resource/gameui_russian.txt"
            : "resource/gameui_english.txt";

        if (!ReadBoundedTextFile(path, 64 * 1024, contents))
            return result;

        std::istringstream input(contents);
        std::string line;

        while (std::getline(input, line))
        {
            const std::size_t keyStart = line.find('"');
            if (keyStart == std::string::npos)
                continue;

            const std::size_t keyEnd = line.find('"', keyStart + 1);
            if (keyEnd == std::string::npos)
                continue;

            const std::size_t valueStart = line.find('"', keyEnd + 1);
            if (valueStart == std::string::npos)
                continue;

            const std::size_t valueEnd = line.find('"', valueStart + 1);
            if (valueEnd == std::string::npos)
                continue;

            const std::string key = line.substr(keyStart + 1, keyEnd - keyStart - 1);
            if (key.empty() || key[0] == '[')
                continue;

            result[key] = DecodeQuotedValue(
                line.substr(valueStart + 1, valueEnd - valueStart - 1));
        }

        return result;
    }();

    return tokens;
}

std::string MenuText(const char* token, const char* fallback)
{
    const auto& tokens = MenuTokens();
    const auto it = tokens.find(token ? token : "");
    return it == tokens.end() ? std::string(fallback ? fallback : "") : it->second;
}

std::vector<BrigMapCatalog::MapMetadata> LoadClientMapCatalog()
{
    using namespace BrigMapCatalog;

    std::vector<MapMetadata> maps;
    std::string manifest;
    if (!ReadBoundedTextFile("maps/brig_maplist.txt", MaxMapListBytes, manifest))
    {
        gEngfuncs.Con_DPrintf("BRIG menu: maps/brig_maplist.txt is unavailable; no campaign list will be shown.\n");
        return maps;
    }

    std::vector<std::string> ids;
    std::vector<ParseIssue> issues;
    if (!ParseMapList(manifest, ids, &issues))
    {
        gEngfuncs.Con_DPrintf("BRIG menu: map manifest could not be parsed.\n");
        return maps;
    }

    for (const auto& id : ids)
    {
        const std::string bspPath = "maps/" + id + ".bsp";
        FSFile bsp(bspPath.c_str(), "rb");
        if (!bsp || bsp.Size() < 4)
            continue;

        unsigned char header[4] = {};
        if (bsp.Read(header, sizeof(header)) != static_cast<int>(sizeof(header)) ||
            !IsGoldSrcBspHeader(header, sizeof(header)))
        {
            continue;
        }

        MapMetadata metadata = MakeDefaultMetadata(id);
        const std::string cfgPath = "maps/" + id + ".cfg";
        std::string cfg;

        if (ReadBoundedTextFile(cfgPath.c_str(), MaxConfigBytes, cfg))
        {
            std::vector<ParseIssue> configIssues;
            ParseMapConfig(cfg, metadata, &configIssues);
        }

        maps.push_back(std::move(metadata));
    }

    return maps;
}

const char* ModeToken(BrigMapCatalog::GameMode mode)
{
    switch (mode)
    {
    case BrigMapCatalog::GameMode::CoOp:
        return "BRIG_Mode_Coop";
    case BrigMapCatalog::GameMode::Deathmatch:
        return "BRIG_Mode_Deathmatch";
    case BrigMapCatalog::GameMode::TeamDeathmatch:
        return "BRIG_Mode_TeamDeathmatch";
    case BrigMapCatalog::GameMode::SinglePlayer:
    default:
        return "BRIG_Mode_Singleplayer";
    }
}

const char* ModeFallback(BrigMapCatalog::GameMode mode)
{
    switch (mode)
    {
    case BrigMapCatalog::GameMode::CoOp:
        return "Co-op";
    case BrigMapCatalog::GameMode::Deathmatch:
        return "DeathMatch";
    case BrigMapCatalog::GameMode::TeamDeathmatch:
        return "Team DeathMatch";
    case BrigMapCatalog::GameMode::SinglePlayer:
    default:
        return "Singleplayer";
    }
}

const char* ConnectionTypeFallback(BrigSession::ConnectionType type)
{
    switch (type)
    {
    case BrigSession::ConnectionType::ListenServer:
        return "Listen server";
    case BrigSession::ConnectionType::DedicatedServer:
        return "Dedicated server";
    case BrigSession::ConnectionType::Local:
    default:
        return "Local";
    }
}

const char* CampaignCategoryFallback(BrigCampaignCatalog::Category category)
{
    switch (category)
    {
    case BrigCampaignCatalog::Category::HalfLife1: return "Half-Life 1";
    case BrigCampaignCatalog::Category::BlueShift: return "Blue Shift (not implemented)";
    case BrigCampaignCatalog::Category::OpposingForce: return "Opposing Force (not implemented)";
    case BrigCampaignCatalog::Category::Custom: return "Custom BRIG campaigns";
    case BrigCampaignCatalog::Category::All:
    default: return "All campaigns";
    }
}

int CycleServerLimit(int current, const int* values, std::size_t count)
{
    if (!values || count == 0)
        return current;

    for (std::size_t i = 0; i < count; ++i)
    {
        if (values[i] == current)
            return values[(i + 1) % count];
    }

    return values[0];
}

void QueueIntegerCvar(const char* name, int value)
{
    if (!gEngfuncs.pfnClientCmd || !name || !*name)
        return;

    char command[96];
    std::snprintf(command, sizeof(command), "%s %d\n", name, value);
    gEngfuncs.pfnClientCmd(command);
}

void QueueMultiplayerSessionSettings(const BrigSession::SessionConfiguration& session)
{
    QueueIntegerCvar("mp_timelimit", std::max(0, std::min(session.timeLimitMinutes, 120)));
    QueueIntegerCvar("mp_fraglimit", std::max(0, std::min(session.fragLimit, 1000)));
    QueueIntegerCvar("mp_friendlyfire", session.friendlyFire ? 1 : 0);
    QueueIntegerCvar("mp_falldamage", session.fallDamage ? 1 : 0);
    QueueIntegerCvar("mp_weaponstay", session.weaponStay ? 1 : 0);
    QueueIntegerCvar("mp_flashlight", session.flashlight ? 1 : 0);
    QueueIntegerCvar("mp_allowmonsters", session.allowMonsters ? 1 : 0);
    QueueIntegerCvar("allow_spectators", session.allowSpectators ? 1 : 0);
}

BrigCampaignCatalog::Category NextCampaignCategory(BrigCampaignCatalog::Category category)
{
    using BrigCampaignCatalog::Category;
    switch (category)
    {
    case Category::All: return Category::HalfLife1;
    case Category::HalfLife1: return Category::BlueShift;
    case Category::BlueShift: return Category::OpposingForce;
    case Category::OpposingForce: return Category::Custom;
    case Category::Custom:
    default: return Category::All;
    }
}

bool IsExpansionCategory(const std::string& category)
{
    const std::string normalized = BrigMapCatalog::ToLowerAscii(category);
    return normalized == "blue_shift" || normalized == "opposing_force";
}

std::vector<BrigCampaignCatalog::Campaign> GetSelectableCampaigns(
    const std::vector<BrigMapCatalog::MapMetadata>& maps,
    BrigCampaignCatalog::Category category)
{
    auto campaigns = BrigCampaignCatalog::BuildCampaigns(
        maps, BrigMapCatalog::GameMode::SinglePlayer, category);

    if (category == BrigCampaignCatalog::Category::All)
    {
        campaigns.erase(
            std::remove_if(campaigns.begin(), campaigns.end(),
                [](const BrigCampaignCatalog::Campaign& campaign)
                {
                    return IsExpansionCategory(campaign.category) ||
                        BrigMapCatalog::ToLowerAscii(campaign.id) == "blue_shift" ||
                        BrigMapCatalog::ToLowerAscii(campaign.id) == "opposing_force";
                }),
            campaigns.end());
    }

    return campaigns;
}

bool ConnectDirectToServer(const std::string& address)
{
    if (!gEngfuncs.pfnClientCmd || !BrigSession::IsValidServerAddress(address))
        return false;

    const std::string command = "connect " + address + "\n";
    gEngfuncs.pfnClientCmd(command.c_str());
    return true;
}

bool IsSafeConsoleValue(const std::string& value, std::size_t maxLength, bool allowEmpty)
{
    if ((!allowEmpty && value.empty()) || value.size() > maxLength)
        return false;

    for (unsigned char c : value)
    {
        // Values are inserted into a quoted GoldSrc console command. Reject
        // delimiters and escapes rather than trying to reinterpret user text.
        if (c < 0x20 || c == 0x7f || c == '"' || c == '\\' || c == ';')
            return false;
    }
    return true;
}

bool LaunchSingleplayerMap(const std::string& mapId)
{
    if (!gEngfuncs.pfnClientCmd)
        return false;

    const auto maps = LoadClientMapCatalog();
    if (!BrigGameModeRegistry::CanLaunchMap(
            maps, mapId, BrigMapCatalog::GameMode::SinglePlayer))
    {
        gEngfuncs.Con_DPrintf("BRIG menu: refusing to launch unregistered or unsupported map '%s'.\n",
            mapId.c_str());
        return false;
    }

    auto& session = BrigSession::GetActiveSessionConfiguration();
    session.mode = BrigMapCatalog::GameMode::SinglePlayer;
    session.connectionType = BrigSession::ConnectionType::Local;
    session.mapId = mapId;
    session.maxPlayers = 1;
    session.difficulty = BrigSession::NormalizeDifficulty(session.difficulty);

    // Clear stale multiplayer state before loading a Singleplayer map.
    gEngfuncs.pfnClientCmd("deathmatch 0\n");
    gEngfuncs.pfnClientCmd("coop 0\n");
    gEngfuncs.pfnClientCmd("mp_teamplay 0\n");
    gEngfuncs.pfnClientCmd("maxplayers 1\n");

    char skillCommand[32];
    std::snprintf(skillCommand, sizeof(skillCommand), "skill %d\n", session.difficulty);
    gEngfuncs.pfnClientCmd(skillCommand);
    gEngfuncs.pfnClientCmd(session.allowCheats ? "sv_cheats 1\n" : "sv_cheats 0\n");

    const std::string mapCommand = "map " + mapId + "\n";
    gEngfuncs.pfnClientCmd(mapCommand.c_str());
    return true;
}

bool LaunchConfiguredSession()
{
    if (!gEngfuncs.pfnClientCmd)
        return false;

    auto& session = BrigSession::GetActiveSessionConfiguration();
    if (session.mode == BrigMapCatalog::GameMode::SinglePlayer)
        return LaunchSingleplayerMap(session.mapId);

    const bool coop = session.mode == BrigMapCatalog::GameMode::CoOp;
    if (!coop &&
        session.mode != BrigMapCatalog::GameMode::Deathmatch &&
        session.mode != BrigMapCatalog::GameMode::TeamDeathmatch)
        return false;

    const auto maps = LoadClientMapCatalog();
    if (!BrigGameModeRegistry::CanLaunchMap(maps, session.mapId, session.mode))
    {
        gEngfuncs.Con_DPrintf("BRIG menu: refusing unsupported mode/map '%s'.\n",
            session.mapId.c_str());
        return false;
    }

    if (!IsSafeConsoleValue(session.serverName, 48, false) ||
        !IsSafeConsoleValue(session.password, 32, true))
    {
        gEngfuncs.Con_DPrintf("BRIG menu: server name or password contains unsafe console characters.\n");
        return false;
    }

    session.connectionType = BrigSession::ConnectionType::ListenServer;
    session.maxPlayers = std::max(2, std::min(session.maxPlayers, 32));
    session.difficulty = BrigSession::NormalizeDifficulty(session.difficulty);

    char command[96];
    std::snprintf(command, sizeof(command), "maxplayers %d\n", session.maxPlayers);
    gEngfuncs.pfnClientCmd(command);
    gEngfuncs.pfnClientCmd(coop ? "deathmatch 0\n" : "deathmatch 1\n");
    gEngfuncs.pfnClientCmd(coop ? "coop 1\n" : "coop 0\n");
    gEngfuncs.pfnClientCmd(session.mode == BrigMapCatalog::GameMode::TeamDeathmatch
        ? "mp_teamplay 1\n" : "mp_teamplay 0\n");

    std::snprintf(command, sizeof(command), "skill %d\n", session.difficulty);
    gEngfuncs.pfnClientCmd(command);
    gEngfuncs.pfnClientCmd(session.allowCheats ? "sv_cheats 1\n" : "sv_cheats 0\n");

    const std::string hostnameCommand = "hostname \"" + session.serverName + "\"\n";
    const std::string passwordCommand = "sv_password \"" + session.password + "\"\n";
    gEngfuncs.pfnClientCmd(hostnameCommand.c_str());
    gEngfuncs.pfnClientCmd(passwordCommand.c_str());
    gEngfuncs.pfnClientCmd(session.lan ? "sv_lan 1\n" : "sv_lan 0\n");
    QueueMultiplayerSessionSettings(session);

    const std::string mapCommand = "map " + session.mapId + "\n";
    gEngfuncs.pfnClientCmd(mapCommand.c_str());

    // server.cfg/listenserver.cfg may load defaults during startup. Re-apply
    // all session-owned rules once the map has finished loading.
    gEngfuncs.pfnClientCmd(hostnameCommand.c_str());
    gEngfuncs.pfnClientCmd(passwordCommand.c_str());
    gEngfuncs.pfnClientCmd(session.lan ? "sv_lan 1\n" : "sv_lan 0\n");
    QueueMultiplayerSessionSettings(session);
    return true;
}
} // namespace

CBrigMainMenuPanel::CBrigMainMenuPanel(int x, int y, int wide, int tall)
    : CMenuPanel(0, false, x, y, wide, tall)
{
    SetMenuID(9001);
    g_pBrigMainMenuPanel = this;

    m_pTitle = new Label("", 0, 0, 480, 32);
    m_pTitle->setParent(this);
    m_pTitle->setFont(Scheme::sf_primary1);
    m_pTitle->setFgColor(230, 153, 52, 255);
    m_pTitle->setContentAlignment(Label::a_west);

    m_pBody = new TextPanel("", 0, 0, 480, 100);
    m_pBody->setParent(this);
    m_pBody->setFont(Scheme::sf_primary2);
    m_pBody->setFgColor(210, 210, 196, 255);
    m_pBody->setBgColor(0, 0, 0, 0);

    m_pServerNameLabel = new Label("", 0, 0, 90, 24);
    m_pServerNameLabel->setParent(this);
    m_pServerNameLabel->setFont(Scheme::sf_primary2);
    m_pServerNameLabel->setFgColor(210, 210, 196, 255);
    m_pServerNameLabel->setContentAlignment(Label::a_west);
    m_pServerNameLabel->setVisible(false);

    Font* textEntryFont = nullptr;
    if (App::getInstance() && App::getInstance()->getScheme())
        textEntryFont = App::getInstance()->getScheme()->getFont(Scheme::sf_primary2);

    m_pServerNameEntry = new TextEntry("", 0, 0, 100, 26);
    m_pServerNameEntry->setParent(this);
    if (textEntryFont)
            m_pServerNameEntry->setFont(textEntryFont);
    m_pServerNameEntry->setFgColor(230, 230, 220, 255);
    m_pServerNameEntry->setBgColor(25, 25, 25, 255);
    m_pServerNameEntry->setVisible(false);

    m_pPasswordLabel = new Label("", 0, 0, 90, 24);
    m_pPasswordLabel->setParent(this);
    m_pPasswordLabel->setFont(Scheme::sf_primary2);
    m_pPasswordLabel->setFgColor(210, 210, 196, 255);
    m_pPasswordLabel->setContentAlignment(Label::a_west);
    m_pPasswordLabel->setVisible(false);

    m_pPasswordEntry = new TextEntry("", 0, 0, 100, 26);
    m_pPasswordEntry->setParent(this);
    if (textEntryFont)
            m_pPasswordEntry->setFont(textEntryFont);
    m_pPasswordEntry->setFgColor(230, 230, 220, 255);
    m_pPasswordEntry->setBgColor(25, 25, 25, 255);
    m_pPasswordEntry->setTextHidden(true);
    m_pPasswordEntry->setVisible(false);

    m_pServerAddressLabel = new Label("", 0, 0, 120, 24);
    m_pServerAddressLabel->setParent(this);
    m_pServerAddressLabel->setFont(Scheme::sf_primary2);
    m_pServerAddressLabel->setFgColor(210, 210, 196, 255);
    m_pServerAddressLabel->setContentAlignment(Label::a_west);
    m_pServerAddressLabel->setVisible(false);

    m_pServerAddressEntry = new TextEntry("", 0, 0, 100, 26);
    m_pServerAddressEntry->setParent(this);
    if (textEntryFont)
            m_pServerAddressEntry->setFont(textEntryFont);
    m_pServerAddressEntry->setFgColor(230, 230, 220, 255);
    m_pServerAddressEntry->setBgColor(25, 25, 25, 255);
    m_pServerAddressEntry->setVisible(false);

    m_pFootnote = new Label("", 0, 0, 480, 20);
    m_pFootnote->setParent(this);
    m_pFootnote->setFont(Scheme::sf_primary2);
    m_pFootnote->setFgColor(145, 145, 135, 255);
    m_pFootnote->setContentAlignment(Label::a_west);

    for (int i = 0; i < ButtonCapacity; ++i)
    {
        m_pButtons[i] = new CommandButton("", 0, 0, 480, 30, false, true);
        m_pButtons[i]->setParent(this);
        m_pButtons[i]->addActionSignal(new CBrigMenuButtonSignal(this, i));
        m_pButtons[i]->setVisible(false);
    }

    m_navigation.Start(BrigMenuNavigation::Page::PlayHub);
    setVisible(false);
    LayoutControls();
}

CBrigMainMenuPanel::~CBrigMainMenuPanel()
{
    if (g_pBrigMainMenuPanel != this)
        return;

    // Invalidate callbacks before cancelling the engine-owned request. A
    // response that races destruction will then be ignored by its old context.
    g_pBrigMainMenuPanel = nullptr;
    if (g_lanBrowser.querying)
    {
        const int previousContext = g_lanBrowser.requestContext;
        g_lanBrowser.querying = false;
        g_lanBrowser.requestContext = g_lanBrowser.nextRequestContext++;
        if (g_lanBrowser.nextRequestContext < 7000)
            g_lanBrowser.nextRequestContext = 7000;

        net_api_t* api = gEngfuncs.pNetAPI;
        if (api && api->CancelRequest)
            api->CancelRequest(previousContext);
    }
}

void CBrigMainMenuPanel::ShowPlay()
{
    m_navigation.Start(BrigMenuNavigation::Page::PlayHub);
    RenderPage();
}

void CBrigMainMenuPanel::ShowMultiplayer()
{
    m_navigation.Start(BrigMenuNavigation::Page::MultiplayerHub);
    RenderPage();
}

void CBrigMainMenuPanel::ServerBrowserDataChanged()
{
    if (m_navigation.CurrentPage() == BrigMenuNavigation::Page::ServerBrowser)
    {
        const int maxOffset = g_lanBrowser.servers.empty()
            ? 0
            : ((static_cast<int>(g_lanBrowser.servers.size()) - 1) / 2) * 2;
        m_iServerBrowserOffset = std::min(m_iServerBrowserOffset, maxOffset);
        RenderPage();
    }
}

void CBrigMainMenuPanel::Open()
{
    CMenuPanel::Open();
    LayoutControls();
}

void CBrigMainMenuPanel::paintBackground()
{
    int wide = getWide();
    int tall = getTall();

    if (getParent())
    {
        int parentWide = 0;
        int parentTall = 0;
        getParent()->getSize(parentWide, parentTall);

        if (parentWide > 0 && parentTall > 0 &&
            (parentWide != wide || parentTall != tall))
        {
            setSize(parentWide, parentTall);
            wide = parentWide;
            tall = parentTall;
            LayoutControls();
        }
    }

    drawSetColor(0, 0, 0, 150);
    drawFilledRect(0, 0, wide, tall);

    const int panelWidth = std::max(220, std::min(static_cast<int>(XRES(540)), wide - static_cast<int>(XRES(24))));
    const int panelHeight = std::max(220, std::min(static_cast<int>(YRES(420)), tall - static_cast<int>(YRES(24))));
    const int panelX = (wide - panelWidth) / 2;
    const int panelY = (tall - panelHeight) / 2;

    drawSetColor(9, 11, 10, 238);
    drawFilledRect(panelX, panelY, panelX + panelWidth, panelY + panelHeight);
    drawSetColor(150, 72, 24, 255);
    drawOutlinedRect(panelX, panelY, panelX + panelWidth, panelY + panelHeight);
    drawSetColor(76, 55, 36, 255);
    drawFilledRect(panelX + XRES(22), panelY + YRES(58),
        panelX + panelWidth - XRES(22), panelY + YRES(60));
}

void CBrigMainMenuPanel::LayoutControls()
{
    const int wide = getWide();
    const int tall = getTall();
    if (wide <= 0 || tall <= 0)
        return;

    const int panelWidth = std::max(220, std::min(static_cast<int>(XRES(540)), wide - static_cast<int>(XRES(24))));
    const int panelHeight = std::max(220, std::min(static_cast<int>(YRES(420)), tall - static_cast<int>(YRES(24))));
    const int panelX = (wide - panelWidth) / 2;
    const int panelY = (tall - panelHeight) / 2;
    const int inset = XRES(26);
    const int contentWidth = panelWidth - inset * 2;

    m_pTitle->setBounds(panelX + inset, panelY + YRES(18), contentWidth, YRES(30));
    m_pBody->setBounds(panelX + inset, panelY + YRES(72), contentWidth, YRES(82));

    const int labelWidth = XRES(100);
    const int fieldX = panelX + inset + labelWidth;
    const int fieldWidth = std::max(80, contentWidth - labelWidth);
    const int fieldHeight = std::max(22, static_cast<int>(YRES(26)));
    const int firstFieldY = panelY + YRES(76);
    const int secondFieldY = panelY + YRES(114);
    m_pServerNameLabel->setBounds(panelX + inset, firstFieldY, labelWidth - XRES(6), fieldHeight);
    m_pServerNameEntry->setBounds(fieldX, firstFieldY, fieldWidth, fieldHeight);
    m_pPasswordLabel->setBounds(panelX + inset, secondFieldY, labelWidth - XRES(6), fieldHeight);
    m_pPasswordEntry->setBounds(fieldX, secondFieldY, fieldWidth, fieldHeight);
    m_pServerAddressLabel->setBounds(panelX + inset, panelY + YRES(96), labelWidth - XRES(6), fieldHeight);
    m_pServerAddressEntry->setBounds(fieldX, panelY + YRES(96), fieldWidth, fieldHeight);

    const int buttonHeight = std::max(22, static_cast<int>(YRES(29)));
    const int buttonGap = std::max(3, static_cast<int>(YRES(5)));
    const int buttonTop = panelY + YRES(164);

    for (int i = 0; i < ButtonCapacity; ++i)
    {
        m_pButtons[i]->setBounds(panelX + inset, buttonTop + i * (buttonHeight + buttonGap),
            contentWidth, buttonHeight);
    }

    m_pFootnote->setBounds(panelX + inset, panelY + panelHeight - YRES(27), contentWidth, YRES(18));
}

void CBrigMainMenuPanel::HideButtons()
{
    m_iButtonCount = 0;
    m_iSelectedButton = 0;
    m_iBackButtonSlot = -1;

    for (int i = 0; i < ButtonCapacity; ++i)
    {
        m_pButtons[i]->setVisible(false);
        m_buttonActions[i] = ButtonAction{};
    }
}

void CBrigMainMenuPanel::SetButton(
    int slot, BrigMenuNavigation::Action action, const std::string& label)
{
    if (slot < 0 || slot >= ButtonCapacity)
        return;

    m_buttonActions[slot].command = ButtonCommand::Navigate;
    m_buttonActions[slot].navigationAction = action;
    m_buttonActions[slot].target.clear();
    m_pButtons[slot]->setText(label.c_str());
    m_pButtons[slot]->setVisible(true);
    m_iButtonCount = std::max(m_iButtonCount, slot + 1);
}

void CBrigMainMenuPanel::SetCommandButton(
    int slot, ButtonCommand command, const std::string& label, const std::string& target)
{
    if (slot < 0 || slot >= ButtonCapacity)
        return;

    m_buttonActions[slot].command = command;
    m_buttonActions[slot].target = target;
    m_pButtons[slot]->setText(label.c_str());
    m_pButtons[slot]->setVisible(true);
    m_iButtonCount = std::max(m_iButtonCount, slot + 1);
}

void CBrigMainMenuPanel::RenderPagedEntries(const std::vector<MenuEntry>& entries)
{
    if (m_iListOffset < 0)
        m_iListOffset = 0;

    if (m_iListOffset >= static_cast<int>(entries.size()) && !entries.empty())
        m_iListOffset = ((static_cast<int>(entries.size()) - 1) / ListPageSize) * ListPageSize;

    int slot = 0;
    if (m_iListOffset > 0)
        SetCommandButton(slot++, ButtonCommand::PreviousPage, MenuText("BRIG_Menu_Previous", "Previous"));

    const int end = std::min(static_cast<int>(entries.size()), m_iListOffset + ListPageSize);
    for (int index = m_iListOffset; index < end; ++index)
    {
        const MenuEntry& entry = entries[static_cast<std::size_t>(index)];
        SetCommandButton(slot++, entry.command, entry.label, entry.target);
    }

    if (end < static_cast<int>(entries.size()))
        SetCommandButton(slot++, ButtonCommand::NextPage, MenuText("BRIG_Menu_Next", "Next"));

    SetCommandButton(slot, ButtonCommand::Back, MenuText("BRIG_Menu_Back", "Back"));
}

void CBrigMainMenuPanel::RenderPage()
{
    using BrigMenuNavigation::Action;
    using BrigMenuNavigation::Page;

    // If a text field owned keyboard focus on the previous page, transfer
    // focus back to the menu before hiding it. Otherwise hidden inputs would
    // continue consuming navigation keys when the page changes/reopens.
    App* app = App::getInstance();
    if (app)
    {
        Panel* focused = app->getFocus();
        if (focused == m_pServerNameEntry ||
            focused == m_pPasswordEntry ||
            focused == m_pServerAddressEntry)
        {
            app->requestFocus(m_pTitle);
        }
    }

    HideButtons();
    m_pBody->setVisible(true);
    m_pServerNameLabel->setVisible(false);
    m_pServerNameEntry->setVisible(false);
    m_pPasswordLabel->setVisible(false);
    m_pPasswordEntry->setVisible(false);
    m_pServerAddressLabel->setVisible(false);
    m_pServerAddressEntry->setVisible(false);
    bool pageHasOwnBackButton = false;

    switch (m_navigation.CurrentPage())
    {
    case Page::PlayHub:
        m_pTitle->setText(MenuText("BRIG_Menu_PlayTitle", "PLAY").c_str());
        m_pBody->setText(MenuText("BRIG_Menu_PlayDescription",
            "Choose how you want to play BRIG RPG 2.").c_str());
        SetButton(0, Action::Singleplayer, MenuText("BRIG_Menu_Singleplayer", "Singleplayer"));
        SetButton(1, Action::CreateGame, MenuText("BRIG_Menu_CreateGame", "Create Game"));
        SetButton(2, Action::GameSettings, MenuText("BRIG_Menu_Game", "Game"));
        break;

    case Page::Singleplayer:
    {
        m_pTitle->setText(MenuText("BRIG_Menu_SingleplayerTitle", "SINGLEPLAYER").c_str());
        const auto maps = LoadClientMapCatalog();
        const auto campaigns = GetSelectableCampaigns(maps, m_campaignCategory);
        std::ostringstream body;
        body << MenuText("BRIG_Menu_Category", "Category") << ": "
             << CampaignCategoryFallback(m_campaignCategory) << "\n";
        body << campaigns.size() << " "
             << MenuText("BRIG_Menu_CampaignsAvailable", "playable campaigns registered");
        m_pBody->setText(body.str().c_str());

        SetCommandButton(0, ButtonCommand::CycleCategory,
            std::string(MenuText("BRIG_Menu_Category", "Category")) + ": " +
                CampaignCategoryFallback(m_campaignCategory));
        SetCommandButton(1, ButtonCommand::OpenCampaignList,
            MenuText("BRIG_Menu_BrowseCampaigns", "Browse campaigns"));
        SetCommandButton(2, ButtonCommand::OpenTrainingCourses,
            MenuText("BRIG_Menu_Training", "Training"));
        SetCommandButton(3, ButtonCommand::OpenGameSettings,
            MenuText("BRIG_Menu_GameSettings", "Game settings"));
        break;
    }

    case Page::CampaignList:
    {
        pageHasOwnBackButton = true;
        m_pTitle->setText(MenuText("BRIG_Menu_CampaignsTitle", "CAMPAIGNS").c_str());
        if (m_campaignCategory == BrigCampaignCatalog::Category::BlueShift ||
            m_campaignCategory == BrigCampaignCatalog::Category::OpposingForce)
        {
            m_pBody->setText(MenuText("BRIG_Menu_ExpansionUnavailable",
                "This expansion is not implemented for BRIG RPG 2 yet.").c_str());
            RenderPagedEntries({});
            break;
        }

        const auto maps = LoadClientMapCatalog();
        const auto campaigns = GetSelectableCampaigns(maps, m_campaignCategory);
        std::vector<MenuEntry> entries;
        for (const auto& campaign : campaigns)
        {
            std::ostringstream label;
            label << campaign.title << " (" << campaign.ChapterCount() << " "
                  << MenuText("BRIG_Menu_Chapters", "chapters") << ")";
            entries.push_back({ButtonCommand::OpenCampaign, label.str(), campaign.id});
        }

        m_pBody->setText(entries.empty()
            ? MenuText("BRIG_Menu_NoCampaigns", "No playable campaigns are registered in this category.").c_str()
            : MenuText("BRIG_Menu_SelectCampaign", "Select a campaign.").c_str());
        RenderPagedEntries(entries);
        break;
    }

    case Page::CampaignChapters:
    {
        pageHasOwnBackButton = true;
        m_pTitle->setText(MenuText("BRIG_Menu_ChaptersTitle", "CAMPAIGN").c_str());
        const auto maps = LoadClientMapCatalog();
        const auto campaigns = GetSelectableCampaigns(maps, m_campaignCategory);
        const auto campaign = std::find_if(campaigns.begin(), campaigns.end(),
            [&](const BrigCampaignCatalog::Campaign& item) { return item.id == m_selectedCampaignId; });

        if (campaign == campaigns.end())
        {
            m_pBody->setText(MenuText("BRIG_Menu_SelectionUnavailable",
                "The selected campaign is no longer available.").c_str());
            RenderPagedEntries({});
            break;
        }

        m_pBody->setText(campaign->title.c_str());
        std::vector<MenuEntry> entries;
        if (const auto* startMap = campaign->StartMap())
        {
            if (BrigGameModeRegistry::CanLaunchMap(
                    maps, startMap->name, BrigMapCatalog::GameMode::SinglePlayer))
            {
                entries.push_back({ButtonCommand::LaunchMap,
                    std::string(MenuText("BRIG_Menu_StartCampaign", "Start campaign")) +
                        " - " + startMap->title,
                    startMap->name});
            }
        }
        entries.push_back({ButtonCommand::OpenCampaignMaps,
            MenuText("BRIG_Menu_AllCampaignMaps", "All campaign maps"), campaign->id});

        for (const auto& chapter : campaign->chapters)
        {
            std::ostringstream label;
            label << chapter.title << " (" << chapter.MapCount() << " "
                  << MenuText("BRIG_Menu_Maps", "maps") << ")";
            entries.push_back({ButtonCommand::OpenChapter, label.str(), chapter.id});
        }

        RenderPagedEntries(entries);
        break;
    }

    case Page::ChapterMaps:
    case Page::CampaignMaps:
    case Page::TrainingMaps:
    {
        pageHasOwnBackButton = true;
        const bool chapterPage = m_navigation.CurrentPage() == Page::ChapterMaps;
        const bool campaignPage = m_navigation.CurrentPage() == Page::CampaignMaps;
        m_pTitle->setText(chapterPage
            ? MenuText("BRIG_Menu_ChapterMapsTitle", "CHAPTER MAPS").c_str()
            : (campaignPage
                ? MenuText("BRIG_Menu_CampaignMapsTitle", "CAMPAIGN MAPS").c_str()
                : MenuText("BRIG_Menu_TrainingMapsTitle", "TRAINING MAPS").c_str()));

        const auto maps = LoadClientMapCatalog();
        std::vector<BrigMapCatalog::MapMetadata> selectedMaps;
        std::string heading;

        if (chapterPage || campaignPage)
        {
            const auto campaigns = GetSelectableCampaigns(maps, m_campaignCategory);
            const auto campaign = std::find_if(campaigns.begin(), campaigns.end(),
                [&](const BrigCampaignCatalog::Campaign& item) { return item.id == m_selectedCampaignId; });
            if (campaign != campaigns.end())
            {
                if (chapterPage)
                {
                    const auto chapter = std::find_if(campaign->chapters.begin(), campaign->chapters.end(),
                        [&](const BrigCampaignCatalog::Chapter& item) { return item.id == m_selectedChapterId; });
                    if (chapter != campaign->chapters.end())
                    {
                        selectedMaps = chapter->maps;
                        heading = chapter->title;
                    }
                }
                else
                {
                    selectedMaps = campaign->maps;
                    heading = campaign->title;
                }
            }
        }
        else
        {
            const auto courses = BrigCampaignCatalog::BuildTrainingCourses(
                maps, BrigMapCatalog::GameMode::SinglePlayer);
            const auto course = std::find_if(courses.begin(), courses.end(),
                [&](const BrigCampaignCatalog::Campaign& item) { return item.id == m_selectedTrainingCourseId; });
            if (course != courses.end())
            {
                selectedMaps = course->maps;
                heading = course->title;
            }
        }

        m_pBody->setText(heading.empty()
            ? MenuText("BRIG_Menu_SelectionUnavailable",
                "The selected map group is no longer available.").c_str()
            : heading.c_str());

        std::vector<MenuEntry> entries;
        for (const auto& map : selectedMaps)
        {
            if (!BrigGameModeRegistry::CanLaunchMap(
                    maps, map.name, BrigMapCatalog::GameMode::SinglePlayer))
                continue;

            std::ostringstream label;
            label << map.title << " [" << map.name << "]";
            entries.push_back({ButtonCommand::LaunchMap, label.str(), map.name});
        }
        if (entries.empty() && !heading.empty())
            m_pBody->setText(MenuText("BRIG_Menu_NoLaunchableMaps",
                "No launchable maps are available in this selection.").c_str());
        RenderPagedEntries(entries);
        break;
    }

    case Page::TrainingCourses:
    {
        pageHasOwnBackButton = true;
        m_pTitle->setText(MenuText("BRIG_Menu_Training", "TRAINING").c_str());
        const auto maps = LoadClientMapCatalog();
        const auto courses = BrigCampaignCatalog::BuildTrainingCourses(
            maps, BrigMapCatalog::GameMode::SinglePlayer);
        std::vector<MenuEntry> entries;
        for (const auto& course : courses)
        {
            std::ostringstream label;
            label << course.title << " (" << course.MapCount() << " "
                  << MenuText("BRIG_Menu_Maps", "maps") << ")";
            entries.push_back({ButtonCommand::OpenTrainingCourse, label.str(), course.id});
        }
        m_pBody->setText(entries.empty()
            ? MenuText("BRIG_Menu_NoTraining", "No training courses are registered.").c_str()
            : MenuText("BRIG_Menu_SelectTraining", "Select a training course.").c_str());
        RenderPagedEntries(entries);
        break;
    }

    case Page::CreateGame:
    {
        m_pTitle->setText(MenuText("BRIG_Menu_CreateGameTitle", "CREATE GAME").c_str());
        m_pBody->setVisible(false);
        m_pServerNameLabel->setText(MenuText("BRIG_Menu_ServerName", "Server name").c_str());
        m_pPasswordLabel->setText(MenuText("BRIG_Menu_Password", "Password").c_str());
        m_pServerNameLabel->setVisible(true);
        m_pServerNameEntry->setVisible(true);
        m_pPasswordLabel->setVisible(true);
        m_pPasswordEntry->setVisible(true);

        auto& session = BrigSession::GetActiveSessionConfiguration();
        if (!m_bServerEntriesInitialized)
        {
            const std::string initialName = session.serverName.substr(0, 48);
            const std::string initialPassword = session.password.substr(0, 32);
            m_pServerNameEntry->setText(initialName.c_str(), static_cast<int>(initialName.size()));
            m_pPasswordEntry->setText(initialPassword.c_str(), static_cast<int>(initialPassword.size()));
            m_bServerEntriesInitialized = true;
        }
        SetCommandButton(0, ButtonCommand::CycleGameMode,
            std::string(MenuText("BRIG_Menu_Mode", "Mode")) + ": " +
                MenuText(ModeToken(session.mode), ModeFallback(session.mode)));
        SetCommandButton(1, ButtonCommand::OpenCreateGameMaps,
            std::string(MenuText("BRIG_Menu_Map", "Map")) + ": " +
                (session.mapId.empty() ? MenuText("BRIG_Menu_NotSelected", "Not selected") : session.mapId));
        if (session.mode != BrigMapCatalog::GameMode::SinglePlayer)
        {
            SetCommandButton(2, ButtonCommand::CycleMaxPlayers,
                std::string(MenuText("BRIG_Menu_MaxPlayers", "Max players")) + ": " +
                    std::to_string(session.maxPlayers));
            SetCommandButton(3, ButtonCommand::ToggleLan,
                std::string(MenuText("BRIG_Menu_LAN", "LAN")) + ": " +
                    MenuText(session.lan ? "BRIG_Menu_Enabled" : "BRIG_Menu_Disabled",
                        session.lan ? "Enabled" : "Disabled"));
            SetCommandButton(4, ButtonCommand::StartConfiguredGame,
                MenuText("BRIG_Menu_StartGame", "Start game"));
        }
        else
        {
            SetCommandButton(2, ButtonCommand::StartConfiguredGame,
                MenuText("BRIG_Menu_StartGame", "Start game"));
        }
        break;
    }

    case Page::CreateGameMaps:
    {
        pageHasOwnBackButton = true;
        m_pTitle->setText(MenuText("BRIG_Menu_SelectMapTitle", "SELECT MAP").c_str());
        const auto maps = LoadClientMapCatalog();
        const auto mode = BrigSession::GetActiveSessionConfiguration().mode;
        std::vector<MenuEntry> entries;
        for (const auto& map : maps)
        {
            if (!BrigGameModeRegistry::CanLaunchMap(maps, map.name, mode))
                continue;
            const std::string label = map.title + " [" + map.name + "]";
            entries.push_back({ButtonCommand::SelectSessionMap, label, map.name});
        }
        m_pBody->setText(entries.empty()
            ? MenuText("BRIG_Menu_NoLaunchableMaps", "No launchable maps are available for this mode.").c_str()
            : MenuText("BRIG_Menu_SelectMap", "Select a map for the session.").c_str());
        RenderPagedEntries(entries);
        break;
    }

    case Page::GameSettings:
    {
        m_pTitle->setText(MenuText("BRIG_Menu_GameTitle", "GAME SETTINGS").c_str());
        m_pBody->setText(BuildSessionText().c_str());
        const auto& session = BrigSession::GetActiveSessionConfiguration();
        SetCommandButton(0, ButtonCommand::ToggleDifficulty,
            std::string(MenuText("BRIG_Menu_Difficulty", "Difficulty")) + ": " +
                BrigSession::DifficultyName(session.difficulty));
        SetCommandButton(1, ButtonCommand::ToggleCheats,
            std::string(MenuText("BRIG_Menu_Cheats", "Cheats")) + ": " +
                MenuText(session.allowCheats ? "BRIG_Menu_Enabled" : "BRIG_Menu_Disabled",
                    session.allowCheats ? "Enabled" : "Disabled"));
        SetCommandButton(2, ButtonCommand::OpenServerSettings,
            MenuText("BRIG_Menu_ServerSettings", "Server settings"));
        break;
    }

    case Page::ServerSettings:
    {
        pageHasOwnBackButton = true;
        m_pTitle->setText(MenuText("BRIG_Menu_ServerSettings", "SERVER SETTINGS").c_str());
        const auto& session = BrigSession::GetActiveSessionConfiguration();
        m_pBody->setText(MenuText("BRIG_Menu_ServerSettingsDescription",
            "These values are applied to Deathmatch and Team Deathmatch listen servers.").c_str());

        const auto toggleLabel = [](const char* token, const char* fallback, bool value)
        {
            return std::string(MenuText(token, fallback)) + ": " +
                MenuText(value ? "BRIG_Menu_Enabled" : "BRIG_Menu_Disabled",
                    value ? "Enabled" : "Disabled");
        };
        const auto limitLabel = [](const char* token, const char* fallback, int value)
        {
            return std::string(MenuText(token, fallback)) + ": " +
                (value == 0 ? std::string(MenuText("BRIG_Menu_Off", "Off")) : std::to_string(value));
        };

        std::vector<MenuEntry> entries;
        entries.push_back({ButtonCommand::CycleTimeLimit,
            limitLabel("BRIG_Menu_TimeLimit", "Time limit (minutes)", session.timeLimitMinutes), {}});
        entries.push_back({ButtonCommand::CycleFragLimit,
            limitLabel("BRIG_Menu_FragLimit", "Frag limit", session.fragLimit), {}});
        entries.push_back({ButtonCommand::ToggleFriendlyFire,
            toggleLabel("BRIG_Menu_FriendlyFire", "Friendly fire", session.friendlyFire), {}});
        entries.push_back({ButtonCommand::ToggleFallDamage,
            toggleLabel("BRIG_Menu_FallDamage", "Fall damage", session.fallDamage), {}});
        entries.push_back({ButtonCommand::ToggleWeaponStay,
            toggleLabel("BRIG_Menu_WeaponStay", "Weapon stay", session.weaponStay), {}});
        entries.push_back({ButtonCommand::ToggleFlashlight,
            toggleLabel("BRIG_Menu_Flashlight", "Flashlight", session.flashlight), {}});
        entries.push_back({ButtonCommand::ToggleAllowMonsters,
            toggleLabel("BRIG_Menu_AllowMonsters", "Allow monsters", session.allowMonsters), {}});
        entries.push_back({ButtonCommand::ToggleSpectators,
            toggleLabel("BRIG_Menu_Spectators", "Allow spectators", session.allowSpectators), {}});
        RenderPagedEntries(entries);
        break;
    }

    case Page::MultiplayerHub:
        m_pTitle->setText(MenuText("BRIG_Menu_MultiplayerTitle", "MULTIPLAYER").c_str());
        m_pBody->setText(MenuText("BRIG_Menu_MultiplayerDescription",
            "Choose a multiplayer section.").c_str());
        SetButton(0, Action::ServerBrowser, MenuText("BRIG_Menu_ServerBrowser", "Server Browser"));
        SetButton(1, Action::Lobby, MenuText("BRIG_Menu_Lobby", "Lobby"));
        SetButton(2, Action::CreateLobby, MenuText("BRIG_Menu_CreateLobby", "Create Lobby"));
        SetButton(3, Action::ConnectServer, MenuText("BRIG_Menu_ConnectServer", "Connect to Server"));
        break;

    case Page::ServerBrowser:
    {
        pageHasOwnBackButton = true;
        m_pTitle->setText(MenuText("BRIG_Menu_ServerBrowserTitle", "SERVER BROWSER").c_str());
        m_pBody->setText(g_lanBrowser.status.c_str());
        SetCommandButton(0, ButtonCommand::RefreshLanServers,
            MenuText("BRIG_Menu_RefreshLan", "Refresh LAN"));

        const int start = std::max(0, m_iServerBrowserOffset);
        const int end = std::min(
            static_cast<int>(g_lanBrowser.servers.size()), start + 2);
        int slot = 1;

        for (int index = start; index < end; ++index)
        {
            const LanBrowserServer& server =
                g_lanBrowser.servers[static_cast<std::size_t>(index)];

            std::string label = TruncateBrowserLabel(server.hostname, 22);
            label += " | " + TruncateBrowserLabel(server.map, 12);
            label += " | " + std::to_string(server.players) + "/" +
                std::to_string(server.maxPlayers);
            if (server.pingMilliseconds >= 0)
                label += " | " + std::to_string(server.pingMilliseconds) + "ms";

            SetCommandButton(slot++, ButtonCommand::JoinBrowserServer,
                label, server.address);
        }

        if (m_iServerBrowserOffset > 0)
        {
            SetCommandButton(3, ButtonCommand::PreviousServerPage,
                MenuText("BRIG_Menu_Previous", "Previous"));
        }
        if (end < static_cast<int>(g_lanBrowser.servers.size()))
        {
            SetCommandButton(4, ButtonCommand::NextServerPage,
                MenuText("BRIG_Menu_Next", "Next"));
        }

        SetCommandButton(5, ButtonCommand::Back,
            MenuText("BRIG_Menu_Back", "Back"));
        break;
    }

    case Page::Lobby:
        m_pTitle->setText(MenuText("BRIG_Menu_LobbyTitle", "LOBBY").c_str());
        m_pBody->setText(BuildUnavailableText("BRIG_Menu_LobbyPending",
            "The lobby subsystem will be connected in a later stage.").c_str());
        break;

    case Page::CreateLobby:
        m_pTitle->setText(MenuText("BRIG_Menu_CreateLobbyTitle", "CREATE LOBBY").c_str());
        m_pBody->setText(BuildUnavailableText("BRIG_Menu_CreateLobbyPending",
            "Lobby creation will be connected in a later stage.").c_str());
        break;

    case Page::ConnectServer:
    {
        m_pTitle->setText(MenuText("BRIG_Menu_ConnectServerTitle", "CONNECT TO SERVER").c_str());
        m_pBody->setVisible(false);
        m_pServerAddressLabel->setText(MenuText("BRIG_Menu_ServerAddress", "Server address").c_str());
        m_pServerAddressLabel->setVisible(true);
        m_pServerAddressEntry->setVisible(true);
        auto& session = BrigSession::GetActiveSessionConfiguration();
        if (!m_bServerAddressInitialized)
        {
            m_pServerAddressEntry->setText(
                session.serverAddress.c_str(), static_cast<int>(session.serverAddress.size()));
            m_bServerAddressInitialized = true;
        }
        SetCommandButton(0, ButtonCommand::ConnectToServer,
            MenuText("BRIG_Menu_Connect", "Connect"));
        break;
    }

    }

    if (m_navigation.CanGoBack() && !pageHasOwnBackButton)
    {
        SetButton(m_iButtonCount, Action::CreateGame, MenuText("BRIG_Menu_Back", "Back"));
        m_iBackButtonSlot = m_iButtonCount - 1;
    }

    const std::string footnote = MenuText("BRIG_Menu_Footnote",
        "UP / DOWN - Select    ENTER - Open    ESC - Back");
    m_pFootnote->setText(footnote.c_str());

    if (m_iButtonCount > 0)
        m_pButtons[0]->setArmed(true);

    LayoutControls();
}

std::string CBrigMainMenuPanel::BuildCampaignListText() const
{
    const auto maps = LoadClientMapCatalog();
    const auto campaigns = BrigCampaignCatalog::BuildCampaigns(
        maps, BrigMapCatalog::GameMode::SinglePlayer);
    const auto trainingCourses = BrigCampaignCatalog::BuildTrainingCourses(
        maps, BrigMapCatalog::GameMode::SinglePlayer);

    std::ostringstream text;
    if (campaigns.empty() && trainingCourses.empty())
    {
        text << MenuText("BRIG_Menu_NoCampaigns",
            "No Singleplayer campaigns are available in the map catalog.");
        return text.str();
    }

    text << MenuText("BRIG_Menu_CampaignIntro",
        "Campaigns registered in the shared map catalog:") << "\n";

    for (const auto& campaign : campaigns)
    {
        text << campaign.title << "\n";

        if (campaign.chapters.empty())
        {
            for (const auto& map : campaign.maps)
                text << "  " << map.title << " [" << map.name << "]\n";
            continue;
        }

        for (const auto& chapter : campaign.chapters)
        {
            const auto* startMap = chapter.StartMap();
            text << "  " << chapter.title;

            if (startMap)
                text << " [" << startMap->title << " / " << startMap->name << "]";

            text << "\n";
        }
    }

    if (!trainingCourses.empty())
    {
        text << "\n" << MenuText("BRIG_Menu_Training", "Training") << ":\n";
        for (const auto& course : trainingCourses)
        {
            const auto* startMap = course.StartMap();
            text << "  " << course.title;
            if (startMap)
                text << " [" << startMap->name << "]";
            text << "\n";
        }
    }

    text << "\n" << MenuText("BRIG_Menu_CampaignNextStage",
        "Chapter selection and campaign launch will be connected in the next stage.");
    return text.str();
}

std::string CBrigMainMenuPanel::BuildCreateGameText() const
{
    const auto& session = BrigSession::GetActiveSessionConfiguration();
    const auto maps = LoadClientMapCatalog();
    const auto selected = std::find_if(maps.begin(), maps.end(),
        [&](const BrigMapCatalog::MapMetadata& map) { return map.name == session.mapId; });

    std::ostringstream text;
    text << MenuText("BRIG_Menu_SessionMode", "Mode") << ": "
         << MenuText(ModeToken(session.mode), ModeFallback(session.mode)) << "\n";
    text << MenuText("BRIG_Menu_SessionMap", "Map") << ": "
         << (selected == maps.end() ? MenuText("BRIG_Menu_NotSelected", "Not selected") : selected->title)
         << "\n";
    text << MenuText("BRIG_Menu_SessionConnection", "Connection") << ": "
         << ConnectionTypeFallback(session.connectionType) << "\n";
    text << MenuText("BRIG_Menu_SessionPlayers", "Max players") << ": "
         << session.maxPlayers << "\n";
    text << MenuText("BRIG_Menu_SessionLan", "LAN") << ": "
         << MenuText(session.lan ? "BRIG_Menu_Enabled" : "BRIG_Menu_Disabled",
             session.lan ? "Enabled" : "Disabled") << "\n\n";

    return text.str();
}

std::string CBrigMainMenuPanel::BuildSessionText() const
{
    const auto& session = BrigSession::GetActiveSessionConfiguration();
    std::ostringstream text;

    text << MenuText("BRIG_Menu_SessionSharedConfig",
        "This page reads the shared Session Configuration; settings are not duplicated here.")
         << "\n\n";
    text << MenuText("BRIG_Menu_SessionMode", "Mode") << ": "
         << MenuText(ModeToken(session.mode), ModeFallback(session.mode)) << "\n";
    text << MenuText("BRIG_Menu_SessionConnection", "Connection") << ": "
         << ConnectionTypeFallback(session.connectionType) << "\n";
    text << MenuText("BRIG_Menu_SessionMap", "Map") << ": "
         << (session.mapId.empty() ? MenuText("BRIG_Menu_NotSelected", "Not selected") : session.mapId) << "\n";
    text << MenuText("BRIG_Menu_SessionPlayers", "Max players") << ": "
         << session.maxPlayers << "\n";
    text << MenuText("BRIG_Menu_SessionDifficulty", "Difficulty") << ": "
         << BrigSession::DifficultyName(session.difficulty) << "\n";
    text << MenuText("BRIG_Menu_SessionLan", "LAN") << ": "
         << MenuText(session.lan ? "BRIG_Menu_Enabled" : "BRIG_Menu_Disabled",
             session.lan ? "Enabled" : "Disabled") << "\n";
    text << MenuText("BRIG_Menu_SessionCheats", "SV_CHEATS") << ": "
         << MenuText(session.allowCheats ? "BRIG_Menu_Enabled" : "BRIG_Menu_Disabled",
             session.allowCheats ? "Enabled" : "Disabled") << "\n\n";
    text << MenuText("BRIG_Menu_SessionSettingsNextStage",
        "Editable server and connection settings will be added in the next stage.");
    return text.str();
}

std::string CBrigMainMenuPanel::BuildUnavailableText(
    const char* featureToken, const char* fallback) const
{
    return MenuText(featureToken, fallback);
}

void CBrigMainMenuPanel::MoveSelection(int direction)
{
    if (m_iButtonCount <= 0)
        return;

    m_pButtons[m_iSelectedButton]->setArmed(false);
    m_iSelectedButton += direction;

    if (m_iSelectedButton < 0)
        m_iSelectedButton = m_iButtonCount - 1;
    else if (m_iSelectedButton >= m_iButtonCount)
        m_iSelectedButton = 0;

    m_pButtons[m_iSelectedButton]->setArmed(true);
}

bool CBrigMainMenuPanel::HandleTextEntryKeyInput(bool down, int keynum)
{
    App* app = App::getInstance();
    if (!app)
        return false;

    Panel* focused = app->getFocus();
    if (focused != m_pServerNameEntry &&
        focused != m_pPasswordEntry &&
        focused != m_pServerAddressEntry)
        return false;

    // Escape remains the menu's Back action. Other keys go to the focused
    // TextEntry rather than changing the menu's selected button.
    if (keynum == K_ESCAPE)
        return false;

    KeyCode code;
    bool printable = false;
    if (!GoldSrcKeyToVGuiKey(keynum, code, printable))
        return true;

    SurfaceBase* surface = focused->getSurfaceBase();
    if (!surface)
        return true;

    if (down)
    {
        app->internalKeyPressed(code, surface);
        if (printable)
            app->internalKeyTyped(code, surface);
    }
    else
    {
        app->internalKeyReleased(code, surface);
    }

    return true;
}

void CBrigMainMenuPanel::HandleKeyInput(bool down, int keynum)
{
    if (!down)
        return;

    if (keynum == K_UPARROW)
    {
        MoveSelection(-1);
        return;
    }

    if (keynum == K_DOWNARROW)
    {
        MoveSelection(1);
        return;
    }

    if (keynum == K_ENTER || keynum == K_KP_ENTER || keynum == K_SPACE)
    {
        if (m_iButtonCount > 0)
            m_pButtons[m_iSelectedButton]->fireActionSignal();
        return;
    }

    if (keynum == K_ESCAPE || keynum == K_BACKSPACE)
    {
        CapturePageInputs();
        if (m_navigation.Back())
            RenderPage();
        else if (gViewPort)
            gViewPort->HideTopMenu();
    }
}

void CBrigMainMenuPanel::CapturePageInputs()
{
    auto& session = BrigSession::GetActiveSessionConfiguration();

    if (m_navigation.CurrentPage() == BrigMenuNavigation::Page::CreateGame)
    {
        char serverName[64] = {};
        char password[48] = {};
        m_pServerNameEntry->getText(0, serverName, sizeof(serverName));
        m_pPasswordEntry->getText(0, password, sizeof(password));
        serverName[sizeof(serverName) - 1] = '\0';
        password[sizeof(password) - 1] = '\0';
        session.serverName = serverName;
        session.password = password;
    }
    else if (m_navigation.CurrentPage() == BrigMenuNavigation::Page::ConnectServer)
    {
        char address[80] = {};
        m_pServerAddressEntry->getText(0, address, sizeof(address));
        address[sizeof(address) - 1] = '\0';
        session.serverAddress = address;
    }
}

void CBrigMainMenuPanel::ActivateButton(int slot)
{
    if (slot < 0 || slot >= m_iButtonCount)
        return;

    CapturePageInputs();

    if (slot == m_iBackButtonSlot)
    {
        if (m_navigation.Back())
            RenderPage();
        else if (gViewPort)
            gViewPort->HideTopMenu();
        return;
    }

    ActivateAction(m_buttonActions[slot]);
}

void CBrigMainMenuPanel::ActivateAction(const ButtonAction& action)
{
    switch (action.command)
    {
    case ButtonCommand::Navigate:
        if (m_navigation.Navigate(action.navigationAction))
            RenderPage();
        return;
    case ButtonCommand::Back:
        if (m_navigation.Back())
            RenderPage();
        else if (gViewPort)
            gViewPort->HideTopMenu();
        return;
    case ButtonCommand::CycleCategory:
        m_campaignCategory = NextCampaignCategory(m_campaignCategory);
        m_iListOffset = 0;
        RenderPage();
        return;
    case ButtonCommand::OpenCampaignList:
        m_iListOffset = 0;
        if (m_navigation.NavigateTo(BrigMenuNavigation::Page::CampaignList))
            RenderPage();
        return;
    case ButtonCommand::OpenTrainingCourses:
        m_iListOffset = 0;
        if (m_navigation.NavigateTo(BrigMenuNavigation::Page::TrainingCourses))
            RenderPage();
        return;
    case ButtonCommand::OpenGameSettings:
        if (m_navigation.NavigateTo(BrigMenuNavigation::Page::GameSettings))
            RenderPage();
        return;
    case ButtonCommand::OpenServerSettings:
        m_iListOffset = 0;
        if (m_navigation.NavigateTo(BrigMenuNavigation::Page::ServerSettings))
            RenderPage();
        return;
    case ButtonCommand::CycleTimeLimit:
    {
        static const int limits[] = {0, 10, 20, 30, 45, 60};
        auto& session = BrigSession::GetActiveSessionConfiguration();
        session.timeLimitMinutes = CycleServerLimit(
            session.timeLimitMinutes, limits, sizeof(limits) / sizeof(limits[0]));
        RenderPage();
        return;
    }
    case ButtonCommand::CycleFragLimit:
    {
        static const int limits[] = {0, 10, 20, 30, 50, 100};
        auto& session = BrigSession::GetActiveSessionConfiguration();
        session.fragLimit = CycleServerLimit(
            session.fragLimit, limits, sizeof(limits) / sizeof(limits[0]));
        RenderPage();
        return;
    }
    case ButtonCommand::ToggleFriendlyFire:
    {
        auto& session = BrigSession::GetActiveSessionConfiguration();
        session.friendlyFire = !session.friendlyFire;
        RenderPage();
        return;
    }
    case ButtonCommand::ToggleFallDamage:
    {
        auto& session = BrigSession::GetActiveSessionConfiguration();
        session.fallDamage = !session.fallDamage;
        RenderPage();
        return;
    }
    case ButtonCommand::ToggleWeaponStay:
    {
        auto& session = BrigSession::GetActiveSessionConfiguration();
        session.weaponStay = !session.weaponStay;
        RenderPage();
        return;
    }
    case ButtonCommand::ToggleFlashlight:
    {
        auto& session = BrigSession::GetActiveSessionConfiguration();
        session.flashlight = !session.flashlight;
        RenderPage();
        return;
    }
    case ButtonCommand::ToggleAllowMonsters:
    {
        auto& session = BrigSession::GetActiveSessionConfiguration();
        session.allowMonsters = !session.allowMonsters;
        RenderPage();
        return;
    }
    case ButtonCommand::ToggleSpectators:
    {
        auto& session = BrigSession::GetActiveSessionConfiguration();
        session.allowSpectators = !session.allowSpectators;
        RenderPage();
        return;
    }
    case ButtonCommand::ConnectToServer:
    {
        const auto& session = BrigSession::GetActiveSessionConfiguration();
        if (ConnectDirectToServer(session.serverAddress))
        {
            if (gViewPort)
                gViewPort->HideTopMenu();
        }
        else
        {
            m_pFootnote->setText(MenuText("BRIG_Menu_ConnectRejected",
                "Enter a valid server address, optionally followed by :port.").c_str());
        }
        return;
    }
    case ButtonCommand::RefreshLanServers:
        m_iServerBrowserOffset = 0;
        StartLanServerSearch();
        RenderPage();
        return;
    case ButtonCommand::JoinBrowserServer:
    {
        auto& session = BrigSession::GetActiveSessionConfiguration();
        if (!BrigSession::IsValidServerAddress(action.target))
        {
            m_pFootnote->setText("The server returned an invalid address; connection was cancelled.");
            return;
        }

        session.serverAddress = action.target;
        if (m_pServerAddressEntry)
            m_pServerAddressEntry->setText(
                session.serverAddress.c_str(), static_cast<int>(session.serverAddress.size()));

        if (ConnectDirectToServer(session.serverAddress))
        {
            if (gViewPort)
                gViewPort->HideTopMenu();
        }
        else
        {
            m_pFootnote->setText(MenuText("BRIG_Menu_ConnectRejected",
                "Could not connect to the selected server.").c_str());
        }
        return;
    }
    case ButtonCommand::NextServerPage:
        m_iServerBrowserOffset += 2;
        RenderPage();
        return;
    case ButtonCommand::PreviousServerPage:
        m_iServerBrowserOffset = std::max(0, m_iServerBrowserOffset - 2);
        RenderPage();
        return;
    case ButtonCommand::CycleGameMode:
    {
        auto& session = BrigSession::GetActiveSessionConfiguration();
        using BrigMapCatalog::GameMode;
        const GameMode modes[] = {
            GameMode::SinglePlayer, GameMode::CoOp,
            GameMode::Deathmatch, GameMode::TeamDeathmatch};
        int current = 0;
        for (int i = 0; i < 4; ++i)
            if (modes[i] == session.mode) current = i;
        session.mode = modes[(current + 1) % 4];
        session.connectionType = session.mode == GameMode::SinglePlayer
            ? BrigSession::ConnectionType::Local : BrigSession::ConnectionType::ListenServer;
        session.maxPlayers = session.mode == GameMode::SinglePlayer ? 1 : std::max(2, session.maxPlayers);
        const auto maps = LoadClientMapCatalog();
        if (!BrigGameModeRegistry::CanLaunchMap(maps, session.mapId, session.mode))
            session.mapId.clear();
        RenderPage();
        return;
    }
    case ButtonCommand::OpenCreateGameMaps:
        m_iListOffset = 0;
        if (m_navigation.NavigateTo(BrigMenuNavigation::Page::CreateGameMaps))
            RenderPage();
        return;
    case ButtonCommand::SelectSessionMap:
    {
        auto& session = BrigSession::GetActiveSessionConfiguration();
        session.mapId = action.target;
        m_iListOffset = 0;
        if (m_navigation.Back())
            RenderPage();
        return;
    }
    case ButtonCommand::CycleMaxPlayers:
    {
        auto& session = BrigSession::GetActiveSessionConfiguration();
        const int current = std::max(2, std::min(session.maxPlayers, 32));
        session.maxPlayers = current >= 32 ? 2 : current <= 2 ? 4 : current <= 4 ? 8 : current <= 8 ? 16 : 32;
        RenderPage();
        return;
    }
    case ButtonCommand::ToggleLan:
    {
        auto& session = BrigSession::GetActiveSessionConfiguration();
        session.lan = !session.lan;
        RenderPage();
        return;
    }
    case ButtonCommand::StartConfiguredGame:
        if (LaunchConfiguredSession())
        {
            if (gViewPort)
                gViewPort->HideTopMenu();
        }
        else
        {
            m_pFootnote->setText(MenuText("BRIG_Menu_LaunchRejected",
                "Cannot start this session: choose a supported map and check the session settings.").c_str());
            gEngfuncs.Con_DPrintf("BRIG menu: configured session launch was rejected.\n");
        }
        return;
    case ButtonCommand::OpenCampaign:
        m_selectedCampaignId = action.target;
        m_iListOffset = 0;
        if (m_navigation.NavigateTo(BrigMenuNavigation::Page::CampaignChapters))
            RenderPage();
        return;
    case ButtonCommand::OpenChapter:
        m_selectedChapterId = action.target;
        m_iListOffset = 0;
        if (m_navigation.NavigateTo(BrigMenuNavigation::Page::ChapterMaps))
            RenderPage();
        return;
    case ButtonCommand::OpenCampaignMaps:
        m_iListOffset = 0;
        if (m_navigation.NavigateTo(BrigMenuNavigation::Page::CampaignMaps))
            RenderPage();
        return;
    case ButtonCommand::OpenTrainingCourse:
        m_selectedTrainingCourseId = action.target;
        m_iListOffset = 0;
        if (m_navigation.NavigateTo(BrigMenuNavigation::Page::TrainingMaps))
            RenderPage();
        return;
    case ButtonCommand::LaunchMap:
        if (LaunchSingleplayerMap(action.target))
        {
            if (gViewPort)
                gViewPort->HideTopMenu();
        }
        else
        {
            m_pFootnote->setText(MenuText("BRIG_Menu_LaunchRejected",
                "Launch refused: this map is missing, unsupported or disabled for Singleplayer.").c_str());
            gEngfuncs.Con_DPrintf("BRIG menu: Singleplayer launch validation failed.\n");
        }
        return;
    case ButtonCommand::ToggleDifficulty:
    {
        auto& session = BrigSession::GetActiveSessionConfiguration();
        session.difficulty = BrigSession::NextDifficulty(session.difficulty);
        RenderPage();
        return;
    }
    case ButtonCommand::ToggleCheats:
    {
        auto& session = BrigSession::GetActiveSessionConfiguration();
        session.allowCheats = !session.allowCheats;
        RenderPage();
        return;
    }
    case ButtonCommand::PreviousPage:
        m_iListOffset = std::max(0, m_iListOffset - ListPageSize);
        RenderPage();
        return;
    case ButtonCommand::NextPage:
        m_iListOffset += ListPageSize;
        RenderPage();
        return;
    case ButtonCommand::None:
    default:
        return;
    }
}
