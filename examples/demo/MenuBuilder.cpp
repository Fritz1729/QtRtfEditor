#include "MenuBuilder.h"
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QInputDialog>
#include <QMessageBox>
#include <RichTextEdit.h>
#include <QApplication>
#include <QStatusBar>

#include <QObject>

MenuBuilder::MenuBuilder(QMenuBar* menuBar, DemoWindow* pWindow)
    : _pMenuBar(menuBar)
    , _pWindow(pWindow)
{
}

void MenuBuilder::Build() {
    QMenu* pFileMenu = _pMenuBar->addMenu("&File");
    BuildFileMenu(pFileMenu);

    QMenu* pFormatMenu = _pMenuBar->addMenu("&Format");
    BuildFormatMenu(pFormatMenu);

    QMenu* pProtectionMenu = _pMenuBar->addMenu("&Protection");
    BuildProtectionMenu(pProtectionMenu);

    QMenu* pHelpMenu = _pMenuBar->addMenu("&Help");
    BuildHelpMenu(pHelpMenu);
}

void MenuBuilder::BuildFileMenu(QMenu* pFileMenu) {
    QAction* pLoad = pFileMenu->addAction("&Load RTF...");
    pLoad->setShortcut(Qt::CTRL | Qt::Key_O);
    QObject::connect(pLoad, &QAction::triggered, _pWindow, &DemoWindow::LoadFile);

    QAction* pSave = pFileMenu->addAction("Save &RTF...");
    pSave->setShortcut(Qt::CTRL | Qt::Key_S);
    QObject::connect(pSave, &QAction::triggered, _pWindow, &DemoWindow::SaveFile);

    _pRecentFilesMenu = new QMenu("Recent Files");
    pFileMenu->addMenu(_pRecentFilesMenu);

    QAction* pClearRecent = pFileMenu->addAction("Clear Recent Files");
    QObject::connect(pClearRecent, &QAction::triggered, _pWindow, &DemoWindow::ClearRecentFiles);

    pFileMenu->addSeparator();

    QAction* pQuit = pFileMenu->addAction("E&xit");
    pQuit->setShortcut(Qt::CTRL | Qt::Key_Q);
    QObject::connect(pQuit, &QAction::triggered, qApp, &QApplication::quit);
}

void MenuBuilder::BuildFormatMenu(QMenu* pFormatMenu) {
    QAction* pClearFormat = pFormatMenu->addAction("Clear &Formatting");
    pClearFormat->setShortcut(Qt::CTRL | Qt::Key_0);
    QObject::connect(pClearFormat, &QAction::triggered, _pWindow, [this] {
        QTextCursor cursor = _pWindow->Editor()->textCursor();
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(QTextCharFormat());
        _pWindow->Editor()->setTextCursor(cursor);
    });
}

void MenuBuilder::BuildProtectionMenu(QMenu* pProtectionMenu) {
    QAction* pSetProt = pProtectionMenu->addAction("Set &protection...");
    QObject::connect(pSetProt, &QAction::triggered, _pWindow, &DemoWindow::SetProtection);

    QAction* pClearProt = pProtectionMenu->addAction("Clear &protection");
    QObject::connect(pClearProt, &QAction::triggered, _pWindow, [this] {
        _pWindow->Editor()->ClearProtection();
        _pWindow->StatusBar()->showMessage("All protection ranges cleared");
    });
}

void MenuBuilder::BuildHelpMenu(QMenu* pHelpMenu) {
    QAction* pAbout = pHelpMenu->addAction("&About QtRtfEditor");
    QObject::connect(pAbout, &QAction::triggered, _pWindow, [this] {
        QMessageBox::about(_pWindow, "About QtRtfEditor",
            "QtRtfEditor Demo\n"
            "Reusable RTF-capable QTextEdit subclass\n"
            "\n"
            "Features supported in roundtrip:\n"
            "  Formatting: bold, italic, underline,\n"
            "  strikethrough, superscript, subscript\n"
            "  Fonts and sizes\n"
            "  Text and background colors\n"
            "  Paragraph alignment and indents\n"
            "  Spacing before/after paragraphs\n"
            "  Tabs and tab stops\n"
            "  Special characters (accents, bullets,\n"
            "  dashes, smart quotes, NBSP)\n"
            "\n"
            "License: Dual (LGPL-3.0+ / commercial)");
    });
}
