#pragma once

#include <QMainWindow>
#include <QFont>
#include <RichTextEdit.h>

class ToolbarBuilder;
class MenuBuilder;
class RecentFileHandler;

class DemoWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit DemoWindow(QWidget* parent = nullptr);
    Rte::RichTextEdit* Editor() { return &_editor; }
    QMenu* RecentFilesMenu() const;
    QStatusBar* StatusBar() { return statusBar(); }

public slots:
    void MergeFormatOnSelection(const QTextCharFormat& format);
    void ApplyAlignment(Qt::Alignment alignment);
    void ApplyIndent(int delta);
    void InsertSpecialChar(QChar c);
    void ToggleWordWrap(bool checked);
    void ToggleBold(bool checked);
    void ToggleItalic(bool checked);
    void ToggleUnderline(bool checked);
    void ToggleStrikethrough(bool checked);
    void ToggleSuperscript(bool checked);
    void ToggleSubscript(bool checked);
    void LoadFile();
    void SaveFile();
    void ClearRecentFiles();
    void SetProtection();

private:
    template<typename Fn>
    void applyFormatOnSelection(Fn fn);

    Rte::RichTextEdit _editor;
    ToolbarBuilder* _pToolbarBuilder = nullptr;
    MenuBuilder* _pMenuBuilder = nullptr;
    RecentFileHandler* _pRecentFileHandler = nullptr;
};
