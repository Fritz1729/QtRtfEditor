#pragma once

#include <QMenuBar>
#include <QMenu>
#include <QAction>

#include "DemoWindow.h"

class MenuBuilder {
public:
    explicit MenuBuilder(QMenuBar* menuBar, DemoWindow* pWindow);
    void Build();
    QMenu* RecentFilesMenu() const { return _pRecentFilesMenu; }

private:
    void BuildFileMenu(QMenu* pFileMenu);
    void BuildFormatMenu(QMenu* pFormatMenu);
    void BuildProtectionMenu(QMenu* pProtectionMenu);
    void BuildHelpMenu(QMenu* pHelpMenu);

    QMenuBar* _pMenuBar;
    DemoWindow* _pWindow;
    QMenu* _pRecentFilesMenu = nullptr;
};
