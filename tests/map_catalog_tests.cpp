#include <cassert>
#include <string>
#include <vector>
#include "map_catalog.h"
#include "campaign_catalog.h"
#include "game_mode_registry.h"
#include "session_config.h"
#include "brig_menu_navigation.h"

using namespace BrigMapCatalog;

int main()
{
    assert(IsValidMapId("c0a0"));
    assert(IsValidMapId("my-custom_map-02"));
    assert(!IsValidMapId("../c0a0"));
    assert(!IsValidMapId("maps/c0a0"));
    assert(!IsValidMapId("c0a0.bsp"));
    assert(!IsValidMapId(std::string(32, 'a')));

    MapMetadata missingConfig = MakeDefaultMetadata("unconfigured_map");
    assert(missingConfig.title == "unconfigured_map");
    assert(missingConfig.category == "uncategorized");
    assert(missingConfig.singleplayer && !missingConfig.coop);
    assert(!missingConfig.deathmatch && !missingConfig.teamDeathmatch);
    assert(!missingConfig.hasConfig);
    assert(!IsValidMapId(""));

    std::vector<std::string> ids;
    std::vector<ParseIssue> listIssues;
    const std::string manifest =
        "# BRIG map catalog\n"
        "c0a0\n"
        "C1A0 // normalized and unique\n"
        "c0a0\n"
        "../outside\n"
        "maps/c2a1.bsp\n";
    assert(ParseMapList(manifest, ids, &listIssues));
    assert(ids.size() == 2 && ids[0] == "c0a0" && ids[1] == "c1a0");
    assert(!listIssues.empty());

    MapMetadata map = MakeDefaultMetadata("c0a0");
    std::vector<ParseIssue> configIssues;
    const std::string config =
        "// map metadata\n"
        "brig_title \"Black Mesa Inbound\"\n"
        "brig_campaign \"half_life\"\n"
        "brig_campaign_title \"Half-Life\"\n"
        "brig_category \"half_life_1\"\n"
        "brig_chapter \"black_mesa_inbound\"\n"
        "brig_chapter_title \"Black Mesa Inbound\"\n"
        "brig_chapter_order 1\n"
        "brig_map_order 1\n"
        "brig_campaign_start 1\n"
        "brig_singleplayer 1\n"
        "brig_coop 1\n"
        "brig_deathmatch 0\n"
        "brig_team_deathmatch 0\n"
        "brig_default_mode \"coop\"\n"
        "forcepmodels \"scientist,barney\"\n"
        "globalsoundlist \"c0a0/soundrep.txt\"\n";
    assert(ParseMapConfig(config, map, &configIssues));
    assert(configIssues.empty());
    assert(map.title == "Black Mesa Inbound");
    assert(map.campaignId == "half_life" && map.campaignTitle == "Half-Life");
    assert(map.chapterOrder == 1 && map.mapOrder == 1 && map.campaignStart);
    assert(map.singleplayer && map.coop && !map.deathmatch && !map.teamDeathmatch);
    assert(map.defaultMode == GameMode::CoOp);
    assert(IsModeAllowed(map, GameMode::SinglePlayer));
    assert(IsModeAllowed(map, GameMode::CoOp));
    assert(!IsModeAllowed(map, GameMode::Deathmatch));
    assert(map.forcePlayerModels == "scientist,barney" && map.hasForcePlayerModelsSetting);
    assert(map.globalSoundList == "c0a0/soundrep.txt" && map.hasGlobalSoundListSetting);

    MapMetadata safe = MakeDefaultMetadata("custom01");
    std::vector<ParseIssue> unsafeIssues;
    const std::string unsafeConfig =
        "brig_title \"Safe title\"\n"
        "forcepmodels \"../../models/evil\"\n"
        "globalsoundlist \"../../cfg/evil.cfg\"\n"
        "exec \"sv_cheats 1\"\n"
        "sv_cheats 1\n"
        "brig_map_order -1\n"
        "brig_coop yes\n"
        "brig_title \"Duplicate title\"\n";
    assert(ParseMapConfig(unsafeConfig, safe, &unsafeIssues));
    assert(safe.title == "Safe title");
    assert(!safe.hasForcePlayerModelsSetting && !safe.hasGlobalSoundListSetting);
    assert(safe.singleplayer && !safe.coop);
    assert(unsafeIssues.size() >= 6);

    MapMetadata coopOnly = MakeDefaultMetadata("coop_only");
    std::vector<ParseIssue> defaultIssues;
    assert(ParseMapConfig("brig_singleplayer 0\nbrig_coop 1\nbrig_default_mode \"singleplayer\"\n",
        coopOnly, &defaultIssues));
    assert(coopOnly.defaultMode == GameMode::CoOp);
    assert(!coopOnly.defaultModeExplicit && !defaultIssues.empty());

    assert(IsSafeSoundListPath("sound/c0a0/soundrep.txt"));
    assert(IsSafeSoundListPath("custom/soundrep.gsr"));
    assert(IsSafeSoundListPath("../../maps/custom/soundrep.txt"));
    assert(IsSafeSoundListPath("../../maps/custom/soundrep.gsr"));
    assert(!IsSafeSoundListPath("../../maps/../evil.txt"));
    assert(!IsSafeSoundListPath("../../cfg/evil.cfg"));
    assert(!IsSafeSoundListPath("C:/soundrep.txt"));
    assert(!IsSafePlayerModelList("../models"));
    assert(IsSafePlayerModelList("scientist,barney"));

    const unsigned char validBsp[] = {30, 0, 0, 0};
    const unsigned char invalidBsp[] = {29, 0, 0, 0};
    assert(IsGoldSrcBspHeader(validBsp, sizeof(validBsp)));
    assert(!IsGoldSrcBspHeader(invalidBsp, sizeof(invalidBsp)));
    assert(IsCatalogMapUsable("c0a0", true));
    assert(!IsCatalogMapUsable("c0a0", false));
    assert(!IsCatalogMapUsable("../c0a0", true));

    // Hazard Course remains a Singleplayer training campaign but is also
    // declared Co-op eligible in the shared map metadata.
    MapMetadata hazard = MakeDefaultMetadata("t0a0");
    std::vector<ParseIssue> hazardIssues;
    assert(ParseMapConfig(
        "brig_campaign \"hazard_course\"\n"
        "brig_campaign_title \"Hazard Course\"\n"
        "brig_category \"training\"\n"
        "brig_chapter \"training\"\n"
        "brig_singleplayer 1\n"
        "brig_coop 1\n"
        "brig_deathmatch 0\n"
        "brig_team_deathmatch 0\n"
        "brig_default_mode \"singleplayer\"\n",
        hazard, &hazardIssues));
    assert(hazardIssues.empty());
    assert(IsModeAllowed(hazard, GameMode::SinglePlayer));
    assert(IsModeAllowed(hazard, GameMode::CoOp));
    assert(!IsModeAllowed(hazard, GameMode::Deathmatch));
    assert(BrigGameModeRegistry::CanLaunchMap(
        {hazard}, "t0a0", GameMode::SinglePlayer));
    assert(!BrigGameModeRegistry::CanLaunchMap(
        {hazard}, "t0a0", GameMode::CoOp));

    MapMetadata mapContinuation = map;
    mapContinuation.name = "c0a0a";
    mapContinuation.title = "Black Mesa Inbound continuation";
    mapContinuation.mapOrder = 2;
    mapContinuation.campaignStart = false;

    MapMetadata secondChapterMap = map;
    secondChapterMap.name = "c1a0";
    secondChapterMap.title = "Anomalous Materials";
    secondChapterMap.chapterId = "anomalous_materials";
    secondChapterMap.chapterTitle = "Anomalous Materials";
    secondChapterMap.chapterOrder = 2;
    secondChapterMap.mapOrder = 1;
    secondChapterMap.campaignStart = false;

    MapMetadata hazardContinuation = hazard;
    hazardContinuation.name = "t0a0a";
    hazardContinuation.title = "Hazard Course continuation";
    hazardContinuation.mapOrder = 2;
    hazardContinuation.campaignStart = false;

    const auto campaigns = BrigCampaignCatalog::BuildCampaigns(
        {mapContinuation, hazardContinuation, secondChapterMap, map},
        GameMode::SinglePlayer);
    assert(campaigns.size() == 1);
    assert(campaigns[0].id == "half_life");
    assert(campaigns[0].MapCount() == 3);
    assert(campaigns[0].ChapterCount() == 2);
    assert(campaigns[0].StartMap() != nullptr);
    assert(campaigns[0].StartMap()->name == "c0a0");
    assert(campaigns[0].chapters[0].id == "black_mesa_inbound");
    assert(campaigns[0].chapters[0].title == "Black Mesa Inbound");
    assert(campaigns[0].chapters[0].MapCount() == 2);
    assert(campaigns[0].chapters[0].StartMap() != nullptr);
    assert(campaigns[0].chapters[0].StartMap()->name == "c0a0");
    assert(campaigns[0].chapters[1].id == "anomalous_materials");
    assert(campaigns[0].chapters[1].StartMap()->name == "c1a0");

    const auto halfLifeOnly = BrigCampaignCatalog::BuildCampaigns(
        {map, hazard}, GameMode::SinglePlayer, BrigCampaignCatalog::Category::HalfLife1);
    assert(halfLifeOnly.size() == 1);
    assert(halfLifeOnly[0].id == "half_life");

    MapMetadata blueShiftMap = map;
    blueShiftMap.name = "ba_canal1";
    blueShiftMap.campaignId = "blue_shift";
    blueShiftMap.campaignTitle = "Blue Shift";
    blueShiftMap.category = "blue_shift";
    const auto blueShiftOnly = BrigCampaignCatalog::BuildCampaigns(
        {blueShiftMap}, GameMode::SinglePlayer, BrigCampaignCatalog::Category::BlueShift);
    assert(blueShiftOnly.size() == 1);
    assert(blueShiftOnly[0].id == "blue_shift");

    const auto trainingCourses = BrigCampaignCatalog::BuildTrainingCourses(
        {hazardContinuation, hazard}, GameMode::SinglePlayer);
    assert(trainingCourses.size() == 1);
    assert(trainingCourses[0].id == "hazard_course");
    assert(trainingCourses[0].title == "Hazard Course");
    assert(trainingCourses[0].MapCount() == 2);
    assert(trainingCourses[0].StartMap() != nullptr);
    assert(trainingCourses[0].StartMap()->name == "t0a0");

    const auto trainingMaps = BrigCampaignCatalog::BuildTrainingMaps(
        {hazardContinuation, hazard}, GameMode::SinglePlayer);
    assert(trainingMaps.size() == 2);
    assert(trainingMaps[0].name == "t0a0");
    assert(trainingMaps[1].name == "t0a0a");

    assert(BrigGameModeRegistry::IsGameplayReady(GameMode::SinglePlayer));
    assert(!BrigGameModeRegistry::IsGameplayReady(GameMode::CoOp));
    assert(!BrigGameModeRegistry::IsGameplayReady(GameMode::Deathmatch));
    assert(!BrigGameModeRegistry::IsGameplayReady(GameMode::TeamDeathmatch));

    auto& session = BrigSession::GetActiveSessionConfiguration();
    session.mode = GameMode::CoOp;
    session.maxPlayers = 4;
    assert(BrigSession::GetActiveSessionConfiguration().mode == GameMode::CoOp);
    assert(BrigSession::GetActiveSessionConfiguration().maxPlayers == 4);
    session = BrigSession::SessionConfiguration{};

    BrigMenuNavigation::State navigation;
    navigation.Start(BrigMenuNavigation::Page::PlayHub);
    assert(navigation.Navigate(BrigMenuNavigation::Action::Singleplayer));
    assert(navigation.CurrentPage() == BrigMenuNavigation::Page::Singleplayer);
    assert(navigation.Back());
    assert(navigation.CurrentPage() == BrigMenuNavigation::Page::PlayHub);
    assert(navigation.Navigate(BrigMenuNavigation::Action::CreateGame));
    assert(navigation.CurrentPage() == BrigMenuNavigation::Page::CreateGame);
    assert(navigation.Back());
    assert(navigation.Navigate(BrigMenuNavigation::Action::GameSettings));
    assert(navigation.Back());
    assert(!navigation.Back());

    navigation.Start(BrigMenuNavigation::Page::MultiplayerHub);
    assert(navigation.Navigate(BrigMenuNavigation::Action::ServerBrowser));
    assert(navigation.Back());
    assert(navigation.Navigate(BrigMenuNavigation::Action::Lobby));
    assert(navigation.Back());
    assert(navigation.Navigate(BrigMenuNavigation::Action::CreateLobby));
    assert(navigation.Back());
    assert(navigation.Navigate(BrigMenuNavigation::Action::ConnectServer));
    assert(navigation.Back());
    assert(!navigation.Navigate(BrigMenuNavigation::Action::Singleplayer));

    return 0;
}
