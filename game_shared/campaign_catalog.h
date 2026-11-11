#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "map_catalog.h"

namespace BrigCampaignCatalog
{
enum class Category
{
    All,
    HalfLife1,
    BlueShift,
    OpposingForce,
    Custom
};

inline bool IsTrainingMap(const BrigMapCatalog::MapMetadata& map)
{
    return BrigMapCatalog::ToLowerAscii(map.category) == "training" ||
        BrigMapCatalog::ToLowerAscii(map.campaignId) == "hazard_course";
}

inline bool MatchesCategory(
    const BrigMapCatalog::MapMetadata& map,
    Category category)
{
    using BrigMapCatalog::ToLowerAscii;

    switch (category)
    {
    case Category::HalfLife1:
        return ToLowerAscii(map.category) == "half_life_1";
    case Category::BlueShift:
        return ToLowerAscii(map.category) == "blue_shift";
    case Category::OpposingForce:
        return ToLowerAscii(map.category) == "opposing_force";
    case Category::Custom:
        return ToLowerAscii(map.category) == "custom";
    case Category::All:
    default:
        return true;
    }
}

inline bool MapLess(
    const BrigMapCatalog::MapMetadata& lhs,
    const BrigMapCatalog::MapMetadata& rhs)
{
    if (lhs.chapterOrder != rhs.chapterOrder)
        return lhs.chapterOrder < rhs.chapterOrder;
    if (lhs.mapOrder != rhs.mapOrder)
        return lhs.mapOrder < rhs.mapOrder;
    return lhs.name < rhs.name;
}

struct Chapter
{
    std::string id;
    std::string title;
    int order = 9999;
    std::vector<BrigMapCatalog::MapMetadata> maps;

    std::size_t MapCount() const { return maps.size(); }

    // The first map in catalog order is the chapter entry point. This is
    // intentionally independent from campaignStart, which marks only the
    // first map of the entire campaign.
    const BrigMapCatalog::MapMetadata* StartMap() const
    {
        return maps.empty() ? nullptr : &maps.front();
    }
};

struct Campaign
{
    std::string id;
    std::string title;
    std::string category;
    int firstChapterOrder = 9999;
    std::vector<BrigMapCatalog::MapMetadata> maps;
    std::vector<std::string> chapterIds;
    std::vector<Chapter> chapters;

    std::size_t MapCount() const { return maps.size(); }
    std::size_t ChapterCount() const { return chapters.size(); }

    const BrigMapCatalog::MapMetadata* StartMap() const
    {
        for (const auto& map : maps)
        {
            if (map.campaignStart)
                return &map;
        }

        return maps.empty() ? nullptr : &maps.front();
    }
};

inline std::vector<Campaign> BuildGroups(
    const std::vector<BrigMapCatalog::MapMetadata>& maps,
    BrigMapCatalog::GameMode mode,
    Category category,
    bool training)
{
    std::vector<Campaign> campaigns;

    for (const auto& map : maps)
    {
        if (!BrigMapCatalog::IsModeAllowed(map, mode) ||
            map.campaignId.empty() ||
            IsTrainingMap(map) != training ||
            (!training && !MatchesCategory(map, category)))
        {
            continue;
        }

        auto campaignIt = std::find_if(
            campaigns.begin(), campaigns.end(),
            [&](const Campaign& candidate) { return candidate.id == map.campaignId; });

        if (campaignIt == campaigns.end())
        {
            Campaign campaign;
            campaign.id = map.campaignId;
            campaign.title = map.campaignTitle.empty() ? map.campaignId : map.campaignTitle;
            campaign.category = map.category;
            campaign.firstChapterOrder = map.chapterOrder;
            campaigns.push_back(std::move(campaign));
            campaignIt = campaigns.end() - 1;
        }

        Campaign& campaign = *campaignIt;
        campaign.firstChapterOrder = std::min(campaign.firstChapterOrder, map.chapterOrder);

        if (campaign.category.empty())
            campaign.category = map.category;
        if (campaign.title == campaign.id && !map.campaignTitle.empty())
            campaign.title = map.campaignTitle;

        campaign.maps.push_back(map);

        if (map.chapterId.empty())
            continue;

        auto chapterIt = std::find_if(
            campaign.chapters.begin(), campaign.chapters.end(),
            [&](const Chapter& candidate) { return candidate.id == map.chapterId; });

        if (chapterIt == campaign.chapters.end())
        {
            Chapter chapter;
            chapter.id = map.chapterId;
            chapter.title = map.chapterTitle.empty() ? map.chapterId : map.chapterTitle;
            chapter.order = map.chapterOrder;
            campaign.chapters.push_back(std::move(chapter));
            chapterIt = campaign.chapters.end() - 1;

            if (std::find(
                    campaign.chapterIds.begin(), campaign.chapterIds.end(), map.chapterId) ==
                campaign.chapterIds.end())
            {
                campaign.chapterIds.push_back(map.chapterId);
            }
        }

        Chapter& chapter = *chapterIt;
        chapter.order = std::min(chapter.order, map.chapterOrder);
        if (chapter.title == chapter.id && !map.chapterTitle.empty())
            chapter.title = map.chapterTitle;
        chapter.maps.push_back(map);
    }

    for (auto& campaign : campaigns)
    {
        std::stable_sort(campaign.maps.begin(), campaign.maps.end(), MapLess);

        for (auto& chapter : campaign.chapters)
            std::stable_sort(chapter.maps.begin(), chapter.maps.end(), MapLess);

        std::stable_sort(
            campaign.chapters.begin(), campaign.chapters.end(),
            [](const Chapter& lhs, const Chapter& rhs)
            {
                if (lhs.order != rhs.order)
                    return lhs.order < rhs.order;
                return lhs.id < rhs.id;
            });
    }

    std::stable_sort(
        campaigns.begin(), campaigns.end(),
        [](const Campaign& lhs, const Campaign& rhs)
        {
            if (lhs.firstChapterOrder != rhs.firstChapterOrder)
                return lhs.firstChapterOrder < rhs.firstChapterOrder;
            return lhs.id < rhs.id;
        });

    return campaigns;
}

// Normal story campaigns. Training content is deliberately excluded even
// when its map CFG has a campaignId, so Hazard Course never becomes a chapter.
inline std::vector<Campaign> BuildCampaigns(
    const std::vector<BrigMapCatalog::MapMetadata>& maps,
    BrigMapCatalog::GameMode mode,
    Category category = Category::All)
{
    return BuildGroups(maps, mode, category, false);
}

// Standalone training courses share the map catalog but are not included in
// the numbered story-chapter list.
inline std::vector<Campaign> BuildTrainingCourses(
    const std::vector<BrigMapCatalog::MapMetadata>& maps,
    BrigMapCatalog::GameMode mode)
{
    return BuildGroups(maps, mode, Category::All, true);
}

// The complete set of training maps is useful for an explicit "all maps"
// view without maintaining a second list of BSP identifiers.
inline std::vector<BrigMapCatalog::MapMetadata> BuildTrainingMaps(
    const std::vector<BrigMapCatalog::MapMetadata>& maps,
    BrigMapCatalog::GameMode mode)
{
    std::vector<BrigMapCatalog::MapMetadata> result;

    for (const auto& map : maps)
    {
        if (BrigMapCatalog::IsModeAllowed(map, mode) && IsTrainingMap(map))
            result.push_back(map);
    }

    std::stable_sort(result.begin(), result.end(), MapLess);
    return result;
}
} // namespace BrigCampaignCatalog
