#pragma once

#include <vector>

namespace BrigMenuNavigation
{
enum class Page
{
    PlayHub,
    Singleplayer,
    CreateGame,
    GameSettings,
    MultiplayerHub,
    ServerBrowser,
    Lobby,
    CreateLobby,
    ConnectServer,
    CampaignList,
    CampaignChapters,
    ChapterMaps,
    CampaignMaps,
    TrainingCourses,
    TrainingMaps
};

enum class Action
{
    Singleplayer,
    CreateGame,
    GameSettings,
    ServerBrowser,
    Lobby,
    CreateLobby,
    ConnectServer
};

// Pure navigation state shared by the UI and its tests. No action here starts
// a map, creates a network session, or claims an unfinished subsystem works.
class State
{
public:
    void Start(Page page)
    {
        m_history.clear();
        m_page = page;
    }

    Page CurrentPage() const { return m_page; }
    bool CanGoBack() const { return !m_history.empty(); }

    bool Navigate(Action action)
    {
        Page target = m_page;
        bool valid = true;

        switch (action)
        {
        case Action::Singleplayer:
            valid = m_page == Page::PlayHub;
            target = Page::Singleplayer;
            break;
        case Action::CreateGame:
            valid = m_page == Page::PlayHub;
            target = Page::CreateGame;
            break;
        case Action::GameSettings:
            valid = m_page == Page::PlayHub;
            target = Page::GameSettings;
            break;
        case Action::ServerBrowser:
            valid = m_page == Page::MultiplayerHub;
            target = Page::ServerBrowser;
            break;
        case Action::Lobby:
            valid = m_page == Page::MultiplayerHub;
            target = Page::Lobby;
            break;
        case Action::CreateLobby:
            valid = m_page == Page::MultiplayerHub;
            target = Page::CreateLobby;
            break;
        case Action::ConnectServer:
            valid = m_page == Page::MultiplayerHub;
            target = Page::ConnectServer;
            break;
        }

        if (!valid || target == m_page)
            return false;

        m_history.push_back(m_page);
        m_page = target;
        return true;
    }

    bool NavigateTo(Page target)
    {
        if (target == m_page)
            return false;

        m_history.push_back(m_page);
        m_page = target;
        return true;
    }

    bool Back()
    {
        if (m_history.empty())
            return false;

        m_page = m_history.back();
        m_history.pop_back();
        return true;
    }

private:
    Page m_page = Page::PlayHub;
    std::vector<Page> m_history;
};
} // namespace BrigMenuNavigation
