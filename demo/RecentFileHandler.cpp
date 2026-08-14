#include "RecentFileHandler.h"
#include "DemoWindow.h"
#include <QSettings>
#include <QFileInfo>

RecentFileHandler::RecentFileHandler(DemoWindow* pWindow, QObject* parent)
    : QObject(parent)
    , _pWindow(pWindow)
{
    LoadRecentFiles();
    UpdateRecentFilesMenu(_pWindow->RecentFilesMenu());
}

void RecentFileHandler::LoadRecentFiles() {
    QSettings settings;
    QVariant list = settings.value("recentFiles");
    if (list.canConvert<QStringList>()) {
        _recentFiles = list.toStringList();
    }
}

void RecentFileHandler::SaveRecentFiles() const {
    QSettings settings;
    settings.setValue("recentFiles", QVariant(_recentFiles));
    settings.sync();
}

void RecentFileHandler::AddRecentFile(const QString& path) {
    QFileInfo fi(path);
    QString canonical = fi.canonicalFilePath();

    _recentFiles.removeAll(canonical);
    _recentFiles.prepend(canonical);

    const int maxRecent = 8;
    while (_recentFiles.size() > maxRecent) {
        _recentFiles.removeLast();
    }

    SaveRecentFiles();
    UpdateRecentFilesMenu(_pWindow->RecentFilesMenu());
}

void RecentFileHandler::UpdateRecentFilesMenu(QMenu* pMenu) {
    pMenu->clear();

    if (_recentFiles.isEmpty()) {
        pMenu->addAction("Keine Dateien");
        return;
    }

    for (int i = 0; i < _recentFiles.size(); ++i) {
        QString path = _recentFiles[i];
        QAction* pAction = pMenu->addAction(path);
        QObject::connect(pAction, &QAction::triggered, _pWindow, [this, path] {
            OpenRecentFile(path);
        });
    }
}

void RecentFileHandler::OpenRecentFile(const QString& path) {
    _pWindow->LoadFromFile(path);
}

void RecentFileHandler::ClearRecentFiles() {
    _recentFiles.clear();
    QSettings settings;
    settings.setValue("recentFiles", QVariant());
    UpdateRecentFilesMenu(_pWindow->RecentFilesMenu());
}
