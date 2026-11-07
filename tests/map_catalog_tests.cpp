#include <cassert>
#include <string>
#include <vector>
#include "map_catalog.h"

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
    return 0;
}
