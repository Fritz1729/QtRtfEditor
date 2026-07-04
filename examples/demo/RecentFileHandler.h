#pragma once

#include <QStringList>
#include <QMenu>
#include <QObject>

class DemoWindow;

class RecentFileHandler : public QObject {
    Q_OBJECT
public:
    explicit RecentFileHandler(DemoWindow* pWindow, QObject* parent = nullptr);

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
