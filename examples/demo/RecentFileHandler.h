#pragma once

#include <QStringList>
#include <QMenu>

#include "DemoWindow.h"

class RecentFileHandler {
public:
    RecentFileHandler(DemoWindow* pWindow);

    void AddRecentFile(const QString& path);
    void UpdateRecentFilesMenu(QMenu* pMenu);
    void OpenRecentFile(const QString& path);
    void ClearRecentFiles();

private:
    void LoadRecentFiles();
    void SaveRecentFiles() const;

    QStringList _recentFiles;
    DemoWindow* _pWindow;
};
