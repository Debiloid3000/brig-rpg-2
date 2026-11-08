#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "map_catalog.h"

namespace BrigCampaignCatalog
{
struct Campaign
{
    std::string id;
    std::string title;
    std::string category;
    int firstChapterOrder = 9999;
    std::vector<BrigMapCatalog::MapMetadata> maps;
    std::vector<std::string> chapterIds;

    std::size_t MapCount() const { return maps.size(); }
    std::size_t ChapterCount() const { return chapterIds.size(); }
};

inline bool MapLess(const BrigMapCatalog::MapMetadata& lhs, const BrigMapCatalog::MapMetadata& rhs)
{
    if (lhs.chapterOrder != rhs.chapterOrder)
        return lhs.chapterOrder < rhs.chapterOrder;
    if (lhs.mapOrder != rhs.mapOrder)
        return lhs.mapOrder < rhs.mapOrder;
    return lhs.name < rhs.name;
}

// Campaigns are derived from the one map catalog. This function deliberately
// owns no separate list of campaign or map identifiers.
inline std::vector<Campaign> BuildCampaigns(
    const std::vector<BrigMapCatalog::MapMetadata>& maps,
    BrigMapCatalog::GameMode mode)
{
    std::vector<Campaign> campaigns;

    for (const auto& map : maps)
    {
        if (!BrigMapCatalog::IsModeAllowed(map, mode) || map.campaignId.empty())
            continue;

        auto it = std::find_if(campaigns.begin(), campaigns.end(),
            [&](const Campaign& candidate) { return candidate.id == map.campaignId; });

        if (it == campaigns.end())
        {
            Campaign campaign;
            campaign.id = map.campaignId;
            campaign.title = map.campaignTitle.empty() ? map.campaignId : map.campaignTitle;
            campaign.category = map.category;
            campaign.firstChapterOrder = map.chapterOrder;
            campaigns.push_back(std::move(campaign));
            it = campaigns.end() - 1;
        }

        it->firstChapterOrder = std::min(it->firstChapterOrder, map.chapterOrder);
        if (it->category.empty())
            it->category = map.category;
        if (it->title == it->id && !map.campaignTitle.empty())
            it->title = map.campaignTitle;

        if (!map.chapterId.empty() &&
            std::find(it->chapterIds.begin(), it->chapterIds.end(), map.chapterId) == it->chapterIds.end())
        {
            it->chapterIds.push_back(map.chapterId);
        }

        it->maps.push_back(map);
    }

    for (auto& campaign : campaigns)
        std::stable_sort(campaign.maps.begin(), campaign.maps.end(), MapLess);

    std::stable_sort(campaigns.begin(), campaigns.end(),
        [](const Campaign& lhs, const Campaign& rhs)
        {
            if (lhs.firstChapterOrder != rhs.firstChapterOrder)
                return lhs.firstChapterOrder < rhs.firstChapterOrder;
            return lhs.id < rhs.id;
        });

    return campaigns;
}
} // namespace BrigCampaignCatalog
