#include "brig_main_menu.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

#include <VGUI_TextPanel.h>

#include "cl_util.h"
#include "campaign_catalog.h"
#include "filesystem_utils.h"
#include "game_mode_registry.h"
#include "session_config.h"
#include "hud.h"
#include "keydefs.h"
#include "vgui_int.h"

namespace
{
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
} // namespace

CBrigMainMenuPanel::CBrigMainMenuPanel(int x, int y, int wide, int tall)
    : CMenuPanel(0, false, x, y, wide, tall)
{
    SetMenuID(9001);

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
        m_pButtons[i]->setVisible(false);
}

void CBrigMainMenuPanel::SetButton(
    int slot, BrigMenuNavigation::Action action, const std::string& label)
{
    if (slot < 0 || slot >= ButtonCapacity)
        return;

    m_buttonActions[slot] = action;
    m_pButtons[slot]->setText(label.c_str());
    m_pButtons[slot]->setVisible(true);
    m_iButtonCount = std::max(m_iButtonCount, slot + 1);
}

void CBrigMainMenuPanel::RenderPage()
{
    using BrigMenuNavigation::Action;
    using BrigMenuNavigation::Page;

    HideButtons();

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
        m_pTitle->setText(MenuText("BRIG_Menu_SingleplayerTitle", "SINGLEPLAYER").c_str());
        m_pBody->setText(BuildCampaignListText().c_str());
        break;

    case Page::CreateGame:
        m_pTitle->setText(MenuText("BRIG_Menu_CreateGameTitle", "CREATE GAME").c_str());
        m_pBody->setText(BuildCreateGameText().c_str());
        break;

    case Page::GameSettings:
        m_pTitle->setText(MenuText("BRIG_Menu_GameTitle", "GAME").c_str());
        m_pBody->setText(BuildSessionText().c_str());
        break;

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
        m_pTitle->setText(MenuText("BRIG_Menu_ServerBrowserTitle", "SERVER BROWSER").c_str());
        m_pBody->setText(BuildUnavailableText("BRIG_Menu_ServerBrowserPending",
            "The server browser subsystem will be connected in a later stage.").c_str());
        break;

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
        m_pTitle->setText(MenuText("BRIG_Menu_ConnectServerTitle", "CONNECT TO SERVER").c_str());
        m_pBody->setText(BuildUnavailableText("BRIG_Menu_ConnectPending",
            "Server connection UI will be connected in a later stage.").c_str());
        break;
    }

    if (m_navigation.CanGoBack())
    {
        SetButton(m_iButtonCount, Action::CreateGame, MenuText("BRIG_Menu_Back", "Back"));
        m_iBackButtonSlot = m_iButtonCount - 1;
    }

    const std::string footnote = MenuText("BRIG_Menu_Footnote", "UP / DOWN - Select    ENTER - Open    ESC - Back");
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
    using BrigMapCatalog::GameMode;

    const GameMode modes[] = {
        GameMode::SinglePlayer,
        GameMode::CoOp,
        GameMode::Deathmatch,
        GameMode::TeamDeathmatch
    };

    std::ostringstream text;
    text << MenuText("BRIG_Menu_ModeStatusIntro",
        "Mode availability in this build:") << "\n";

    for (const GameMode mode : modes)
    {
        const auto& descriptor = BrigGameModeRegistry::GetDescriptor(mode);
        text << MenuText(ModeToken(mode), ModeFallback(mode)) << " - "
             << MenuText(descriptor.statusToken,
                 descriptor.gameplayReady ? "Available" : "Not implemented in this stage")
             << "\n";
    }

    text << "\n" << MenuText("BRIG_Menu_CreateGameNextStage",
        "Mode, map and connection selection will be added in the next stage.");
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
        if (m_navigation.Back())
            RenderPage();
        else if (gViewPort)
            gViewPort->HideTopMenu();
    }
}

void CBrigMainMenuPanel::ActivateButton(int slot)
{
    if (slot < 0 || slot >= m_iButtonCount)
        return;

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

void CBrigMainMenuPanel::ActivateAction(BrigMenuNavigation::Action action)
{
    if (m_navigation.Navigate(action))
        RenderPage();
}
