#pragma once

#include <string>

#include "hud.h"
#include "cl_util.h"
#include "brig_menu_navigation.h"
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
    static constexpr int ButtonCapacity = 5;

    void RenderPage();
    void LayoutControls();
    void SetButton(int slot, BrigMenuNavigation::Action action, const std::string& label);
    void HideButtons();
    void MoveSelection(int direction);
    void ActivateAction(BrigMenuNavigation::Action action);
    std::string BuildCampaignListText() const;
    std::string BuildCreateGameText() const;
    std::string BuildSessionText() const;
    std::string BuildUnavailableText(const char* featureToken, const char* fallback) const;

    BrigMenuNavigation::State m_navigation;
    Label* m_pTitle = nullptr;
    TextPanel* m_pBody = nullptr;
    Label* m_pFootnote = nullptr;
    CommandButton* m_pButtons[ButtonCapacity]{};
    BrigMenuNavigation::Action m_buttonActions[ButtonCapacity]{};
    int m_iButtonCount = 0;
    int m_iSelectedButton = 0;
    int m_iBackButtonSlot = -1;
};
