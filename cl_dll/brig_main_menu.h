#pragma once

#include <string>
#include <vector>

#include "hud.h"
#include "cl_util.h"
#include "brig_menu_navigation.h"
#include "campaign_catalog.h"
#include "vgui_TeamFortressViewport.h"

class CBrigMainMenuPanel final : public CMenuPanel
{
public:
    CBrigMainMenuPanel(int x, int y, int wide, int tall);

    void ShowPlay();
    void ShowMultiplayer();
    void Open() override;
    void paintBackground() override;
    void HandleKeyInput(bool down, int keynum);
    void ActivateButton(int slot);

private:
    static constexpr int ButtonCapacity = 6;
    static constexpr int ListPageSize = 3;

    enum class ButtonCommand
    {
        None,
        Navigate,
        Back,
        CycleCategory,
        OpenCampaignList,
        OpenTrainingCourses,
        OpenGameSettings,
        CycleGameMode,
        OpenCreateGameMaps,
        SelectSessionMap,
        CycleMaxPlayers,
        ToggleLan,
        StartConfiguredGame,
        OpenCampaign,
        OpenChapter,
        OpenCampaignMaps,
        OpenTrainingCourse,
        LaunchMap,
        ToggleDifficulty,
        ToggleCheats,
        PreviousPage,
        NextPage
    };

    struct ButtonAction
    {
        ButtonCommand command = ButtonCommand::None;
        BrigMenuNavigation::Action navigationAction = BrigMenuNavigation::Action::Singleplayer;
        std::string target;
    };

    struct MenuEntry
    {
        ButtonCommand command = ButtonCommand::None;
        std::string label;
        std::string target;
    };

    void RenderPage();
    void LayoutControls();
    void SetButton(int slot, BrigMenuNavigation::Action action, const std::string& label);
    void SetCommandButton(int slot, ButtonCommand command, const std::string& label,
        const std::string& target = std::string());
    void RenderPagedEntries(const std::vector<MenuEntry>& entries);
    void ActivateAction(const ButtonAction& action);
    void HideButtons();
    void MoveSelection(int direction);
    std::string BuildCampaignListText() const;
    std::string BuildCreateGameText() const;
    std::string BuildSessionText() const;
    std::string BuildUnavailableText(const char* featureToken, const char* fallback) const;

    BrigMenuNavigation::State m_navigation;
    Label* m_pTitle = nullptr;
    TextPanel* m_pBody = nullptr;
    Label* m_pFootnote = nullptr;
    CommandButton* m_pButtons[ButtonCapacity]{};
    ButtonAction m_buttonActions[ButtonCapacity]{};
    BrigCampaignCatalog::Category m_campaignCategory = BrigCampaignCatalog::Category::All;
    std::string m_selectedCampaignId;
    std::string m_selectedChapterId;
    std::string m_selectedTrainingCourseId;
    int m_iListOffset = 0;
    int m_iButtonCount = 0;
    int m_iSelectedButton = 0;
    int m_iBackButtonSlot = -1;
};
