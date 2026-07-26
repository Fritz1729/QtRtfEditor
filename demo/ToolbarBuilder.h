#pragma once

#include <QToolBar>
#include <QFontComboBox>
#include <QSpinBox>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QIcon>

#include "DemoWindow.h"

class ToolbarBuilder {
public:
    explicit ToolbarBuilder(QToolBar* toolbar, DemoWindow* pWindow);
    ~ToolbarBuilder();
    void Build();

private:
    void BuildFontControls();
    void BuildFormatControls();
    void BuildColorControls();
    void BuildColorActions(QMenu* pMenu, bool isTextColor);
    void BuildAlignmentControls();
    void BuildIndentControls();
    void BuildSpecialCharControls();
    void BuildWordWrapControl();
    void BuildProtectionControl();
    void BuildUndoRedoControls();

    QToolBar* _pToolbar;
    DemoWindow* _pWindow;

    QAction* _pBold = nullptr;
    QAction* _pItalic = nullptr;
    QAction* _pUnderline = nullptr;
    QAction* _pStrikethrough = nullptr;
    QAction* _pSuperscript = nullptr;
    QAction* _pSubscript = nullptr;
    QAction* _pAlignLeft = nullptr;
    QAction* _pAlignCenter = nullptr;
    QAction* _pAlignRight = nullptr;
    QAction* _pAlignJustify = nullptr;
    QAction* _pIndentDecrease = nullptr;
    QAction* _pIndentIncrease = nullptr;
    QAction* _pWordWrap = nullptr;

    QFontComboBox* _pFontComboBox = nullptr;
    QSpinBox* _pFontSize = nullptr;
    QToolButton* _pTextColorBtn = nullptr;
    QMenu* _pTextColorMenu = nullptr;
    QToolButton* _pBgColorBtn = nullptr;
    QMenu* _pBgColorMenu = nullptr;
    QToolButton* _pSpecialChar = nullptr;
    QMenu* _pSpecialCharMenu = nullptr;
};
